#include <Arduino.h>
#include "IRsendRMT.h"
#include "esp_check.h"
#include "esp_log.h"
#include <algorithm>

static const char* TAG = "IRsendRMT";

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

IRsendRMT::IRsendRMT(uint16_t IRsendPin, bool inverted, bool use_modulation)
    : IRsend(IRsendPin, inverted, use_modulation),
      _pin(IRsendPin),
      _txChannel(nullptr),
      _copyEncoder(nullptr),
      _symCount(0),
      _currentFreqHz(38000),
      _pendingMarkUs(0),
      _hasPendingMark(false) {}

IRsendRMT::~IRsendRMT() {
  if (_txChannel) {
    rmt_disable(_txChannel);
    rmt_del_channel(_txChannel);
    _txChannel = nullptr;
  }
  if (_copyEncoder) {
    rmt_del_encoder(_copyEncoder);
    _copyEncoder = nullptr;
  }
}

// ---------------------------------------------------------------------------
// begin() — initialise RMT TX channel (replaces pinMode + LEDC setup)
// ---------------------------------------------------------------------------

void IRsendRMT::begin() {
  // outputOn is HIGH for normal LEDs, LOW for inverted — inherited from IRsend.
  bool invert = (outputOn == LOW);

  rmt_tx_channel_config_t txCfg = {};
  txCfg.gpio_num             = static_cast<gpio_num_t>(_pin);
  txCfg.clk_src              = RMT_CLK_SRC_DEFAULT;
  txCfg.resolution_hz        = kRmtResolutionHz;  // 1 MHz → 1 µs per tick
  txCfg.mem_block_symbols    = 64;                 // hardware FIFO depth
  txCfg.trans_queue_depth    = 4;
  txCfg.flags.invert_out     = invert ? 1u : 0u;
  txCfg.flags.with_dma       = 0;

  ESP_ERROR_CHECK(rmt_new_tx_channel(&txCfg, &_txChannel));

  rmt_copy_encoder_config_t encCfg = {};
  ESP_ERROR_CHECK(rmt_new_copy_encoder(&encCfg, &_copyEncoder));

  // Apply default 38 kHz carrier at 33 % duty.
  _reconfigCarrier(_currentFreqHz);

  ESP_ERROR_CHECK(rmt_enable(_txChannel));
}

// ---------------------------------------------------------------------------
// enableIROut() — switch carrier frequency for protocols that need != 38 kHz
// (e.g. Daikin2 / Panasonic → 36700 Hz, TrotecESP → 36000 Hz)
// ---------------------------------------------------------------------------

void IRsendRMT::enableIROut(uint32_t freq, uint8_t /*duty*/) {
  // Library sometimes passes kHz (< 1000) and sometimes full Hz.
  // Normalise to Hz to match what the base class comment says.
  if (freq < 1000) freq *= 1000;

  if (freq != _currentFreqHz) {
    _reconfigCarrier(freq);
  }

  // Buffer is NOT reset here.  _flushBuffer() already resets _symCount,
  // _hasPendingMark, and _pendingMarkUs after every RMT transmission, so the
  // buffer is always clean at the start of a new send*() call.
  //
  // Resetting here was harmful for multi-section protocols (e.g. Samsung AC):
  // sendGeneric() calls enableIROut() once per section, so a reset here wiped
  // section 1's accumulated symbols before section 2 was added to the buffer.
}

// ---------------------------------------------------------------------------
// mark() — buffer a carrier-on period; paired with the next space() call
// ---------------------------------------------------------------------------

uint16_t IRsendRMT::mark(uint16_t usec) {
  if (usec == 0) return 0;
  _pendingMarkUs  = usec;
  _hasPendingMark = true;
  return 1;
}

// ---------------------------------------------------------------------------
// space() — complete the mark/space pair, flush on inter-frame gaps
// ---------------------------------------------------------------------------

void IRsendRMT::space(uint32_t usec) {
  if (!_hasPendingMark && usec == 0) return;

  uint32_t markUs = _hasPendingMark ? _pendingMarkUs : 0;
  _hasPendingMark = false;
  _pendingMarkUs  = 0;

  _addSymbolPair(markUs, usec);

  // Inter-frame gap detected → flush everything to RMT hardware now.
  if (usec >= kRmtGapThresholdUs) {
    _flushBuffer();
  }
}

// ---------------------------------------------------------------------------
// _reconfigCarrier() — apply new carrier to the RMT channel
// ---------------------------------------------------------------------------

void IRsendRMT::_reconfigCarrier(uint32_t freqHz) {
  rmt_carrier_config_t carrier = {};
  carrier.frequency_hz = freqHz;
  carrier.duty_cycle   = kRmtCarrierDuty;  // 33 %

  ESP_ERROR_CHECK(rmt_apply_carrier(_txChannel, &carrier));
  _currentFreqHz = freqHz;
}

// ---------------------------------------------------------------------------
// _addSymbolPair() — append one mark+space entry to _symbols[].
// ---------------------------------------------------------------------------

void IRsendRMT::_addSymbolPair(uint32_t markUs, uint32_t spaceUs) {
  if (_symCount + 4 >= kRmtSymbolBufSize) {
    _flushBuffer();
  }

  // Pre-compensate the space for carrier-cycle rounding on the mark.
  // The RMT hardware extends each mark to the nearest whole carrier cycle,
  // consuming that extra time from duration1 (the following space). Adding
  // the overshoot to duration1 cancels the steal so the actual space output
  // matches the intended spaceUs.
  uint32_t overshoot = 0;
  if (markUs > 0 && spaceUs > 0 && _currentFreqHz > 0) {
    uint32_t period  = (kRmtResolutionHz + _currentFreqHz - 1) / _currentFreqHz;
    uint32_t rounded = ((markUs + period - 1) / period) * period;
    overshoot        = rounded - markUs;
  }

  // Guard: marks exceeding the 15-bit RMT limit are clamped. No known IR
  // protocol triggers this today (max observed: Hitachi AC424 leader = 29,784 µs)
  // but log a warning so any future violation is immediately visible.
  if (markUs > kRmtMaxTickDuration)
    ESP_LOGW(TAG, "_addSymbolPair: mark %uus exceeds 15-bit limit (%u), clamping",
             markUs, kRmtMaxTickDuration);

  uint32_t markTicks  = std::min(markUs,  static_cast<uint32_t>(kRmtMaxTickDuration));
  uint32_t spaceAdj   = spaceUs + overshoot;
  uint32_t spaceChunk = std::min(spaceAdj, static_cast<uint32_t>(kRmtMaxTickDuration));
  rmt_symbol_word_t sym = {};
  sym.level0    = 1;
  sym.duration0 = static_cast<uint16_t>(markTicks);
  sym.level1    = 0;
  sym.duration1 = static_cast<uint16_t>(spaceChunk);
  _symbols[_symCount++] = sym;

  uint32_t remaining = spaceAdj - spaceChunk;

  while (remaining > 0) {
    if (_symCount + 1 >= kRmtSymbolBufSize) {
      _flushBuffer();
    }
    uint32_t d0 = std::min(remaining, static_cast<uint32_t>(kRmtMaxTickDuration));
    remaining  -= d0;
    uint32_t d1 = std::min(remaining, static_cast<uint32_t>(kRmtMaxTickDuration));
    remaining  -= d1;

    rmt_symbol_word_t extra = {};
    extra.level0    = 0;
    extra.duration0 = static_cast<uint16_t>(d0);
    extra.level1    = 0;
    extra.duration1 = static_cast<uint16_t>(d1);
    _symbols[_symCount++] = extra;
  }
}

// ---------------------------------------------------------------------------
// _flushBuffer() — transmit accumulated symbols via RMT hardware
// ---------------------------------------------------------------------------

void IRsendRMT::_flushBuffer() {
  if (_symCount == 0) return;

  // rmt_copy_encoder ends transmission after exactly (_symCount * sizeof symbol)
  // bytes — no zero-sentinel is needed or wanted. Zeroing duration1 of the
  // last symbol would destroy inter-frame gaps (e.g. MITSUBISHI_AC uses a
  // 15.5 ms gap that is part of the protocol timing).

  rmt_transmit_config_t txCfg = {};
  txCfg.loop_count    = 0;   // send once
  txCfg.flags.eot_level = 0; // pin LOW (silence) after transmission

  esp_err_t err = rmt_transmit(_txChannel, _copyEncoder,
                                _symbols, _symCount * sizeof(rmt_symbol_word_t),
                                &txCfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "rmt_transmit failed: %s", esp_err_to_name(err));
  } else {
    // Block until RMT hardware finishes. 2 s timeout covers even the slowest
    // multi-repeat protocol (Hitachi424 with repeats).
    err = rmt_tx_wait_all_done(_txChannel, 2000);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "rmt_tx_wait_all_done failed: %s", esp_err_to_name(err));
    }
  }

  _symCount        = 0;
  _hasPendingMark  = false;
  _pendingMarkUs   = 0;
}
