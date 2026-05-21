#pragma once

#include <IRsend.h>
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"

// Maximum rmt_symbol_word_t entries in the transmit buffer.
// Sized for the worst-case single sendGeneric() call:
//   Hitachi424 leader + 424 bits + header + footer = ~430 symbol pairs.
//   Large gaps split across multiple symbols add ~10 more.
//   512 gives comfortable headroom.
static const uint16_t kRmtSymbolBufSize = 512;

// Spaces >= this threshold (µs) are treated as inter-frame gaps.
// After building such a space symbol the buffer is flushed to RMT hardware.
// Threshold set to 20000 µs — above Samsung AC's header space (17844 µs),
// which is an intra-frame space that must stay buffered with the section data.
// All inter-frame gaps are >= 20001 µs (Trotec uses explicit space(20001);
// Samsung AC message gap = 97114 µs; most others use kDefaultMessageGap = 100 ms).
static const uint32_t kRmtGapThresholdUs = 20000;

// RMT resolution: 1 µs per tick (1 MHz clock).
// Maximum representable duration per half-symbol: 32767 µs.
// Durations exceeding this are split across consecutive symbol entries.
static const uint32_t kRmtResolutionHz = 1000000;
static const uint32_t kRmtMaxTickDuration = 32767;

// IR carrier duty cycle — 33 % is standard for IR LEDs.
static const float kRmtCarrierDuty = 0.33f;


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

 private:
  uint16_t              _pin;
  rmt_channel_handle_t  _txChannel;
  rmt_encoder_handle_t  _copyEncoder;
  rmt_symbol_word_t     _symbols[kRmtSymbolBufSize];
  size_t                _symCount;       // symbols currently in buffer
  uint32_t              _currentFreqHz;  // last carrier frequency set
  uint32_t              _pendingMarkUs;  // mark waiting to be paired with space
  bool                  _hasPendingMark;

  // Reconfigure the RMT carrier to a new frequency (called from enableIROut).
  void _reconfigCarrier(uint32_t freqHz);

  // Append one mark+space pair to _symbols[], splitting any duration that
  // exceeds kRmtMaxTickDuration into consecutive silence symbols.
  void _addSymbolPair(uint32_t markUs, uint32_t spaceUs);

  // Transmit everything in _symbols[] via RMT hardware, wait for completion,
  // then reset the buffer.
  void _flushBuffer();
};
