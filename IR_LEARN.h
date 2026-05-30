#pragma once
// IR_LEARN.h — IR Learn Fallback for unsupported AC controllers
//
// Purpose: backup control path when the protocol library fails.
//   Case A — cannot decode the remote at all.
//   Case B — decoded, but no supported variant makes the AC respond.
//
// Constraints:
//   - TSOP38238 demodulates IR; true carrier frequency is not captured.
//     All replays use 38 kHz.  Store carrier policy, not a measured value.
//   - rawbuf[0] is time-since-last-signal (leading gap), NOT part of the frame.
//     Skip index 0; store rawbuf[1..rawlen-1] converted to µs.
//   - Two separate buffers: gLearnRamBuf (capture workflow, not yet saved)
//     and gLearnReplayBuf (loaded from saved slot for replay).
//     Replaying a different slot must NOT destroy an unsaved capture.
//   - learnSendAllStatus() cannot live here because bleSend() is defined
//     later in the .ino.  It is declared here and defined in the .ino.
//   - All NVS I/O goes through NVS_CONFIG.h helpers (irlearn partition).

#include <IRrecv.h>
#include "NVS_CONFIG.h"

// ── Slot definitions ─────────────────────────────────────────
#define LEARN_SLOT_OFF      0
#define LEARN_SLOT_COOL_16  1
#define LEARN_SLOT_COOL_17  2
#define LEARN_SLOT_COOL_18  3
#define LEARN_SLOT_COOL_19  4
#define LEARN_SLOT_COOL_20  5
#define LEARN_SLOT_COOL_21  6
#define LEARN_SLOT_COOL_22  7
#define LEARN_SLOT_COOL_23  8
#define LEARN_SLOT_COOL_24  9
#define LEARN_SLOT_COOL_25  10
#define LEARN_SLOT_COOL_26  11
#define LEARN_SLOT_COOL_27  12
#define LEARN_SLOT_COOL_28  13
#define LEARN_SLOT_COOL_29  14
#define LEARN_SLOT_COOL_30  15
#define LEARN_NUM_SLOTS     16
#define LEARN_MAX_RAWLEN    1023  // matches kCaptureBufferSize-1 (rawbuf[0] skipped)

#define LEARN_NVS_VERSION   1
#define LEARN_CARRIER_KHZ   38   // default replay carrier — cannot measure from TSOP38238

static const char* const kLearnSlotNames[LEARN_NUM_SLOTS] = {
  "OFF",
  "COOL_16","COOL_17","COOL_18","COOL_19","COOL_20",
  "COOL_21","COOL_22","COOL_23","COOL_24","COOL_25",
  "COOL_26","COOL_27","COOL_28","COOL_29","COOL_30"
};

// NVS key per slot — short enough for Preferences (max 15 chars)
static const char* const kLearnNvsKeys[LEARN_NUM_SLOTS] = {
  "sl_off",
  "sl_c16","sl_c17","sl_c18","sl_c19","sl_c20",
  "sl_c21","sl_c22","sl_c23","sl_c24","sl_c25",
  "sl_c26","sl_c27","sl_c28","sl_c29","sl_c30"
};

// ── Control mode ─────────────────────────────────────────────
enum CtrlMode : uint8_t { CTRL_PROTOCOL = 0, CTRL_LEARNED = 1 };
CtrlMode gCtrlMode = CTRL_PROTOCOL;

// ── Slot state ───────────────────────────────────────────────
enum LearnSlotState : uint8_t {
  SLOT_EMPTY    = 0,
  SLOT_CAPTURED = 1,   // in RAM, not yet saved
  SLOT_SAVED    = 2,   // persisted to irlearn NVS
};

static LearnSlotState gSlotState[LEARN_NUM_SLOTS];
static uint8_t        gActiveLearnSlot   = 0xFF;   // slot currently in the learn workflow
static LearnSlotState gPreCaptureState   = SLOT_EMPTY; // state before capture started (for discard)
static bool           gLearnCaptureArmed = false;  // true only while waiting for first IR frame

// ── Capture buffer (RAM only — not yet saved) ─────────────────
// Stores mark/space durations in µs (rawbuf × kRawTick=2, index 0 skipped).
static uint16_t gLearnRamBuf[LEARN_MAX_RAWLEN];
static uint16_t gLearnRamLen = 0;

// ── Replay buffer (loaded from a saved slot) ──────────────────
// Separate from gLearnRamBuf so a pending capture is not clobbered.
static uint16_t gLearnReplayBuf[LEARN_MAX_RAWLEN];
static uint16_t gLearnReplayLen = 0;
static uint8_t  gLearnReplayCarrier = LEARN_CARRIER_KHZ;

// Forward declaration — body lives in .ino where bleSend() is visible.
void learnSendAllStatus();

// ── NVS blob layout ──────────────────────────────────────────
// Byte 0:   LEARN_NVS_VERSION
// Byte 1:   slot_id
// Byte 2:   carrier_khz (= 38)
// Byte 3:   reserved / 0
// Bytes 4-5: rawlen (little-endian uint16_t)
// Bytes 6+:  rawbuf as uint16_t array (little-endian, values in µs)
//
// Max blob: 6 + 1023×2 = 2052 bytes per slot.
// 16 slots max total: ~32 KB << 48 KB irlearn partition.

static bool learnEncodeBlob(uint8_t slotId,
                             const uint16_t* buf, uint16_t rawlen,
                             uint8_t* out, size_t* outLen) {
  const size_t blobLen = 6u + (size_t)rawlen * 2u;
  out[0] = LEARN_NVS_VERSION;
  out[1] = slotId;
  out[2] = LEARN_CARRIER_KHZ;
  out[3] = 0;
  out[4] = (uint8_t)(rawlen & 0xFF);
  out[5] = (uint8_t)(rawlen >> 8);
  memcpy(out + 6, buf, (size_t)rawlen * 2u);
  *outLen = blobLen;
  return true;
}

static bool learnDecodeBlob(const uint8_t* in, size_t inLen,
                              uint16_t* bufOut, uint16_t* rawlenOut,
                              uint8_t* carrierOut) {
  if (inLen < 6) return false;
  if (in[0] != LEARN_NVS_VERSION) return false;
  const uint16_t rawlen = (uint16_t)in[4] | ((uint16_t)in[5] << 8);
  if (rawlen == 0 || rawlen > LEARN_MAX_RAWLEN) return false;
  if (inLen < 6u + (size_t)rawlen * 2u) return false;
  *rawlenOut  = rawlen;
  *carrierOut = in[2];
  memcpy(bufOut, in + 6, (size_t)rawlen * 2u);
  return true;
}

// ── Public API ────────────────────────────────────────────────

static bool learnValidateSavedBlob(uint8_t slot) {
  if (slot >= LEARN_NUM_SLOTS) return false;

  static uint8_t blob[6u + LEARN_MAX_RAWLEN * 2u];
  const size_t n = learnNvsLoadBlob(kLearnNvsKeys[slot], blob, sizeof(blob));
  if (n < 6 || blob[1] != slot) return false;

  uint16_t rawlen = 0;
  uint8_t carrier = LEARN_CARRIER_KHZ;
  return learnDecodeBlob(blob, n, gLearnReplayBuf, &rawlen, &carrier);
}

// Call once in setup() — scan irlearn NVS and mark only valid slots as saved.
static void learnInit() {
  for (uint8_t i = 0; i < LEARN_NUM_SLOTS; i++) {
    if (learnNvsKeyExists(kLearnNvsKeys[i]) && learnValidateSavedBlob(i)) {
      gSlotState[i] = SLOT_SAVED;
    } else {
      gSlotState[i] = SLOT_EMPTY;
    }
  }
  gLearnReplayLen = 0;
  Serial.println(F("[LEARN] Slot states loaded from NVS"));
}

static bool learnAllSlotsSaved() {
  for (uint8_t i = 0; i < LEARN_NUM_SLOTS; i++) {
    if (gSlotState[i] != SLOT_SAVED) return false;
  }
  return true;
}

// Return the slot index for a given TX job (power string + temp).
// Returns 0xFF if no matching slot.
static uint8_t learnSlotForJob(const String& power, int temp) {
  if (power == "OFF") return LEARN_SLOT_OFF;
  // "ON" or "" (temp only) → map temperature to COOL slot
  const int clamp = (temp < 16) ? 16 : (temp > 30) ? 30 : temp;
  return (uint8_t)(LEARN_SLOT_COOL_16 + (clamp - 16));
}

// Start a capture workflow for a given slot.
// Saves current state so discard can restore it.
static void learnBeginCapture(uint8_t slot) {
  if (slot >= LEARN_NUM_SLOTS) return;
  gActiveLearnSlot = slot;
  gPreCaptureState = gSlotState[slot];
  gLearnRamLen     = 0;
  gLearnCaptureArmed = true;
  Serial.printf("[LEARN] Begin capture → slot %d (%s)\n", slot, kLearnSlotNames[slot]);
}

// Store a decoded result into gLearnRamBuf.
// rawbuf values are in half-µs ticks (kRawTick=2); multiply to get µs.
// rawbuf[0] is the leading gap — skip it.
static bool learnStoreCapture(const decode_results* r) {
  if (gActiveLearnSlot >= LEARN_NUM_SLOTS) return false;
  if (!gLearnCaptureArmed) return false;
  if (r->rawlen < 2) return false;  // nothing useful beyond the leading gap

  const uint16_t copyLen = (r->rawlen - 1u < LEARN_MAX_RAWLEN)
                           ? (uint16_t)(r->rawlen - 1u)
                           : LEARN_MAX_RAWLEN;

  for (uint16_t i = 0; i < copyLen; i++) {
    // rawbuf[i+1]: half-µs ticks → µs.  Clamp to uint16_t max before storing
    // so we never truncate via wrap-around (65535×2=131070 wraps to 65534).
    const uint32_t us = (uint32_t)r->rawbuf[i + 1u] * kRawTick;
    gLearnRamBuf[i] = (us > 65535u) ? 65535u : (uint16_t)us;
  }
  gLearnRamLen = copyLen;
  gLearnCaptureArmed = false;

  gSlotState[gActiveLearnSlot] = SLOT_CAPTURED;
  Serial.printf("[LEARN] Captured %d marks/spaces for slot %d (%s)\n",
                copyLen, gActiveLearnSlot, kLearnSlotNames[gActiveLearnSlot]);
  return true;
}

// Load gLearnRamBuf into gLearnReplayBuf for test replay.
// Does NOT touch saved NVS data.
static bool learnPrepareTestReplay() {
  if (gActiveLearnSlot >= LEARN_NUM_SLOTS) return false;
  if (gSlotState[gActiveLearnSlot] != SLOT_CAPTURED) return false;
  if (gLearnRamLen == 0) return false;

  memcpy(gLearnReplayBuf, gLearnRamBuf, gLearnRamLen * sizeof(uint16_t));
  gLearnReplayLen     = gLearnRamLen;
  gLearnReplayCarrier = LEARN_CARRIER_KHZ;
  return true;
}

// Save the active capture (gLearnRamBuf) to irlearn NVS.
// Only call after a successful capture — state must be SLOT_CAPTURED.
static bool learnSaveSlot() {
  if (gActiveLearnSlot >= LEARN_NUM_SLOTS) return false;
  if (gSlotState[gActiveLearnSlot] != SLOT_CAPTURED) return false;
  if (gLearnRamLen == 0) return false;

  // Static: keeps 2052-byte blob off the NimBLE host task stack (4 KB limit).
  // Safe because LRN:SAVE is serialized — BLE callbacks never run concurrently.
  static uint8_t blob[6u + LEARN_MAX_RAWLEN * 2u];
  size_t blobLen = 0;
  learnEncodeBlob(gActiveLearnSlot, gLearnRamBuf, gLearnRamLen, blob, &blobLen);

  if (!learnNvsSaveBlob(kLearnNvsKeys[gActiveLearnSlot], blob, blobLen)) {
    Serial.printf("[LEARN] NVS save failed for slot %d\n", gActiveLearnSlot);
    return false;
  }
  gSlotState[gActiveLearnSlot] = SLOT_SAVED;
  gPreCaptureState             = SLOT_SAVED;  // update so discard no longer reverts
  gLearnCaptureArmed           = false;
  Serial.printf("[LEARN] Saved slot %d (%s)  %u µs-pairs\n",
                gActiveLearnSlot, kLearnSlotNames[gActiveLearnSlot], gLearnRamLen);
  return true;
}

// Discard the unsaved capture — restore pre-capture state.
// Does NOT erase a previously saved slot.
static void learnDiscardCapture() {
  if (gActiveLearnSlot >= LEARN_NUM_SLOTS) return;
  gSlotState[gActiveLearnSlot] = gPreCaptureState;
  gLearnRamLen     = 0;
  gActiveLearnSlot = 0xFF;
  gLearnCaptureArmed = false;
  Serial.println(F("[LEARN] Capture discarded"));
}

// Delete a saved slot from NVS and reset its state to EMPTY.
static bool learnDeleteSlot(uint8_t slot) {
  if (slot >= LEARN_NUM_SLOTS) return false;
  const bool existed = learnNvsKeyExists(kLearnNvsKeys[slot]);
  if (existed && !learnNvsEraseKey(kLearnNvsKeys[slot])) return false;
  gSlotState[slot] = SLOT_EMPTY;
  if (gActiveLearnSlot == slot) {
    gActiveLearnSlot = 0xFF;
    gLearnRamLen     = 0;
    gLearnCaptureArmed = false;
  }
  Serial.printf("[LEARN] Deleted slot %d (%s)\n", slot, kLearnSlotNames[slot]);
  return true;
}

// Delete every saved learned slot and reset any in-progress capture/replay RAM.
static bool learnDeleteAllSlots() {
  bool ok = true;
  for (uint8_t i = 0; i < LEARN_NUM_SLOTS; i++) {
    const bool existed = learnNvsKeyExists(kLearnNvsKeys[i]);
    if (existed && !learnNvsEraseKey(kLearnNvsKeys[i])) {
      ok = false;
    } else {
      gSlotState[i] = SLOT_EMPTY;
    }
  }
  gActiveLearnSlot = 0xFF;
  gPreCaptureState = SLOT_EMPTY;
  gLearnRamLen     = 0;
  gLearnReplayLen  = 0;
  gLearnCaptureArmed = false;
  Serial.println(F("[LEARN] Deleted all learned slots"));
  return ok;
}

// Load a saved slot into gLearnReplayBuf for playback from executeTx().
// Returns false if the slot is not saved or the NVS blob is corrupt.
static bool learnPrepareReplay(uint8_t slot) {
  if (slot >= LEARN_NUM_SLOTS) return false;
  if (gSlotState[slot] != SLOT_SAVED) return false;

  // Static: keeps 2052-byte blob off the loop() stack and avoids VLA.
  // learnPrepareReplay() is only called from executeTx() (no re-entrancy).
  static uint8_t blob[6u + LEARN_MAX_RAWLEN * 2u];
  const size_t n = learnNvsLoadBlob(kLearnNvsKeys[slot], blob, sizeof(blob));
  if (n < 6) return false;
  if (blob[1] != slot) return false;

  uint16_t rawlen = 0;
  uint8_t  carrier = LEARN_CARRIER_KHZ;
  if (!learnDecodeBlob(blob, n, gLearnReplayBuf, &rawlen, &carrier)) {
    Serial.printf("[LEARN] Blob decode failed for slot %d\n", slot);
    return false;
  }
  gLearnReplayLen     = rawlen;
  gLearnReplayCarrier = carrier;
  return true;
}

// State string for BLE reporting
static const char* learnSlotStateStr(uint8_t slot) {
  if (slot >= LEARN_NUM_SLOTS) return "INVALID";
  switch (gSlotState[slot]) {
    case SLOT_EMPTY:    return "EMPTY";
    case SLOT_CAPTURED: return "CAPTURED";
    case SLOT_SAVED:    return "SAVED";
    default:            return "UNKNOWN";
  }
}
