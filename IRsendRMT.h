#pragma once

#include <IRsend.h>
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"

// Maximum rmt_symbol_word_t entries in the transmit buffer.
// Each entry = one mark+space pair = 4 bytes of SRAM.
// The ESP32-C5 hardware FIFO is only 48 symbols deep, but the copy encoder
// uses a ping-pong interrupt mechanism to stream any amount of SRAM data
// through that FIFO — so this software buffer has no hardware-imposed limit.
// The only constraint is available SRAM (ESP32-C5: 400 KB).
//
// Worst-case symbol counts (marks + spaces per repeat × repeat count):
//   Mitsubishi STD 144-bit, 2 sections, kMitsubishiACMinRepeat=2 : ~870
//   Hitachi AC424 424-bit, kHitachiAcDefaultRepeat=0             : ~430
//   Daikin STD 280-bit, 2 sections, kDaikinDefaultRepeat=0       : ~580
//   Kelvinator 128-bit, 2 sections, kKelvinatorDefaultRepeat=1   : ~540
//
// 2048 covers every protocol at maximum repeats with comfortable headroom.
// RAM cost: 2048 × 4 bytes = 8 KB (trivial on a 400 KB chip).
static const uint16_t kRmtSymbolBufSize = 2048;

// Spaces >= this threshold (µs) are treated as inter-frame gaps.
// After building such a space symbol the buffer is flushed to RMT hardware.
//
// Threshold set to 36,000 µs — just above the largest Daikin inter-section
// gap: kDaikin2Gap = 35,204 µs.  All Daikin sections (V5–V12) accumulate in
// the buffer and transmit in one RMT burst.  Samsung AC intra-frame space
// (17,844 µs) and all Trotec / Mitsubishi STD gaps (< 16,000 µs) remain
// safely below threshold.
//
// Still safe for all other protocols:
//   Daikin family max inter-section gap (kDaikin2Gap = 35,204 µs) ← buffered ✓
//   Samsung AC intra-frame hdr space    (17,844 µs)                ← buffered ✓
//   All Trotec / Mitsubishi STD gaps    (< 16,000 µs)              ← buffered ✓
//   kDefaultMessageGap (100,000 µs) and all other end-of-frame gaps ← flush ✓
//
// Protocols with gaps < 36,000 µs that do NOT auto-flush need an explicit
//   irsend.mark(<footerMark>); irsend.space(36001);
// after their send call.  Currently applied to:
//   V0  Mitsubishi STD, V5  Daikin STD,  V6  Daikin2,
//   V8  Daikin128,      V9  Daikin152,   V10 Daikin160,
//   V11 Daikin176,      V12 Daikin216,   V13 Fujitsu,
//   V15 Panasonic AC32, V45 Amcor,       V60 Trotec STD.
//
// V23 Hitachi AC424 has an intra-frame leader space of 49,290 µs which is
// ABOVE this threshold.  Rather than raising the threshold globally (which
// would risk breaking untested protocols), auto-flush is suppressed only for
// the duration of sendHitachiAc424() via irsend.suppressAutoFlush(true/false).
// See IR_TRANSMITTER.h case AC_HITACHI_AC424.
static const uint32_t kRmtGapThresholdUs = 36000;

// RMT resolution: 1 µs per tick (1 MHz clock).
// Maximum representable duration per half-symbol: 32767 µs.
// Durations exceeding this are split across consecutive symbol entries.
static const uint32_t kRmtResolutionHz = 1000000;
static const uint32_t kRmtMaxTickDuration = 32767;

// IR carrier duty cycle — 33 % is standard for IR LEDs.
static const float kRmtCarrierDuty = 0.33f;

// Uncomment to print the RMT symbol buffer (as mark/space µs) to Serial
// before each hardware transmission.  Use for TX/RX side-by-side comparison.
#define IRTX_DEBUG_PRINT


class IRsendRMT : public IRsend {
 public:
  // Same constructor signature as IRsend so it can be a drop-in replacement.
  explicit IRsendRMT(uint16_t IRsendPin,
                     bool inverted       = false,
                     bool use_modulation = true);
  ~IRsendRMT();

  // Overrides — these replace the bit-bang TX path with RMT hardware.
  void     begin()                                           override;
  void     enableIROut(uint32_t freq,
                       uint8_t  duty = kDutyDefault)        override;
  uint16_t mark(uint16_t usec)                              override;
  void     space(uint32_t usec)                             override;

  // Force an immediate RMT hardware transmit of whatever is in the symbol
  // buffer.  Use after protocols whose inter-frame gap is below
  // kRmtGapThresholdUs (e.g. Trotec: kTrotecGap = 6184 µs) so that
  // _flushBuffer() is never triggered automatically by space().
  // Calling this does NOT add any extra mark or space to the frame — it
  // simply dispatches the already-built symbols to the RMT peripheral.
  void flushTx() { _flushBuffer(); }

  // Temporarily disable the automatic gap-threshold flush inside space().
  // Use when a protocol's intra-frame space exceeds kRmtGapThresholdUs but
  // must NOT trigger a premature flush (e.g. V23 Hitachi AC424 leader space
  // = 49,290 µs > 36,000 µs threshold).
  // Always call suppressAutoFlush(false) immediately after the send call.
  // The end-of-frame gap (kDefaultMessageGap = 100,000 µs) still triggers
  // auto-flush because suppressAutoFlush is restored to false before it fires.
  void suppressAutoFlush(bool suppress) { _suppressAutoFlush = suppress; }

 private:
  uint16_t              _pin;
  rmt_channel_handle_t  _txChannel;
  rmt_encoder_handle_t  _copyEncoder;
  rmt_symbol_word_t     _symbols[kRmtSymbolBufSize];
  size_t                _symCount;          // symbols currently in buffer
  uint32_t              _currentFreqHz;     // last carrier frequency set
  uint32_t              _pendingMarkUs;     // mark waiting to be paired with space
  bool                  _hasPendingMark;
  bool                  _suppressAutoFlush; // when true, space() skips the >= threshold flush check

  // Reconfigure the RMT carrier to a new frequency (called from enableIROut).
  void _reconfigCarrier(uint32_t freqHz);

  // Append one mark+space pair to _symbols[], splitting any duration that
  // exceeds kRmtMaxTickDuration into consecutive silence symbols.
  void _addSymbolPair(uint32_t markUs, uint32_t spaceUs);

  // Transmit everything in _symbols[] via RMT hardware, wait for completion,
  // then reset the buffer.
  void _flushBuffer();
};
