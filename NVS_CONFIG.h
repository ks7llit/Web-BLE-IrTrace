#pragma once
// NVS_CONFIG.h — Preferences wrappers for IrTrace BLE firmware
// Covers:  (1) main "irtrace_ble" namespace (Wi-Fi, variant, admin)
//          (2) "irlearn" partition (learned IR blobs)
//
// Rules:
//  - gPrefs must NOT be open when any function here is called.
//  - isValidVariant() / variantName() are defined AFTER this file is
//    included in the .ino; call them from the .ino, not from here.

#include <Preferences.h>

// ── Namespace / defaults ─────────────────────────────────────
#define CFG_NAMESPACE        "irtrace_ble"
#define CFG_DEFAULT_UID      "999"
#define CFG_DEFAULT_WAKE_SEC 60u
#define CFG_ADMIN_PASS       "1234admin"

// Separate partition for learned IR data — avoids filling shared NVS
#define IRLEARN_PARTITION    "irlearn"

// ── Globals declared here (definitions live in .ino) ─────────
extern Preferences gPrefs;
extern String      gStoredUid;
extern uint32_t    gStoredWakeSec;
extern String      gStoredWifiSsid;
extern String      gStoredWifiPass;

// ── Helpers ──────────────────────────────────────────────────
static String nvsConfigPadUid(const String& raw) {
  long value = raw.toInt();
  if (value < 1)   value = 1;
  if (value > 999) value = 999;
  char buf[4];
  snprintf(buf, sizeof(buf), "%03ld", value);
  return String(buf);
}

// ── Main config load/save ─────────────────────────────────────

// Call once in setup() — populates globals from NVS.
// storedVariant is written back through the pointer; caller applies it
// if isValidVariant() passes (can't call that here — defined later).
static void loadStoredConfig(uint8_t* storedVariantOut) {
  if (!gPrefs.begin(CFG_NAMESPACE, true)) {
    Serial.println(F("[CFG] NVS open failed for read"));
    if (storedVariantOut) *storedVariantOut = 0xFF;  // sentinel = not loaded
    return;
  }

  gStoredWifiSsid = gPrefs.getString("ssid", "");
  gStoredWifiPass = gPrefs.getString("pass", "");
  if (storedVariantOut) *storedVariantOut = gPrefs.getUChar("variant", 1);
  gStoredUid     = nvsConfigPadUid(gPrefs.getString("uid", CFG_DEFAULT_UID));
  gStoredWakeSec = gPrefs.getUInt("wake_sec", CFG_DEFAULT_WAKE_SEC);
  gPrefs.end();

  if (gStoredWakeSec < 60u)    gStoredWakeSec = 60u;
  if (gStoredWakeSec > 36000u) gStoredWakeSec = 36000u;
}

static bool saveWifiConfig(const String& ssid, const String& pass) {
  if (!gPrefs.begin(CFG_NAMESPACE, false)) {
    Serial.println(F("[CFG] NVS open failed for Wi-Fi save"));
    return false;
  }
  gPrefs.putString("ssid", ssid);
  gPrefs.putString("pass", pass);
  gPrefs.end();
  gStoredWifiSsid = ssid;
  gStoredWifiPass = pass;
  return true;
}

static bool saveVariantConfig(uint8_t variant) {
  if (!gPrefs.begin(CFG_NAMESPACE, false)) {
    Serial.println(F("[CFG] NVS open failed for variant save"));
    return false;
  }
  gPrefs.putUChar("variant", variant);
  gPrefs.end();
  return true;
}

static bool saveAdminConfig(const String& uid, uint32_t wakeSec) {
  const String paddedUid = nvsConfigPadUid(uid);
  if (wakeSec < 60u)    wakeSec = 60u;
  if (wakeSec > 36000u) wakeSec = 36000u;

  if (!gPrefs.begin(CFG_NAMESPACE, false)) {
    Serial.println(F("[CFG] NVS open failed for admin save"));
    return false;
  }
  gPrefs.putString("uid", paddedUid);
  gPrefs.putUInt("wake_sec", wakeSec);
  gPrefs.end();

  gStoredUid     = paddedUid;
  gStoredWakeSec = wakeSec;
  return true;
}

// ── Learned IR NVS helpers ────────────────────────────────────
// These open the "irlearn" partition (separate from main config).
// All callers must treat the Preferences handle as local — open, use, end.

static bool learnNvsSaveBlob(const char* key, const uint8_t* data, size_t len) {
  Preferences lp;
  if (!lp.begin(IRLEARN_PARTITION, false, IRLEARN_PARTITION)) {
    Serial.printf("[LEARN NVS] Open failed for write (key=%s)\n", key);
    return false;
  }
  const size_t written = lp.putBytes(key, data, len);
  lp.end();
  if (written != len) {
    Serial.printf("[LEARN NVS] Write partial: %u/%u bytes (key=%s)\n",
                  (unsigned)written, (unsigned)len, key);
    return false;
  }
  return true;
}

static size_t learnNvsLoadBlob(const char* key, uint8_t* buf, size_t bufLen) {
  Preferences lp;
  if (!lp.begin(IRLEARN_PARTITION, true, IRLEARN_PARTITION)) {
    return 0;
  }
  const size_t n = lp.getBytes(key, buf, bufLen);
  lp.end();
  return n;
}

static bool learnNvsEraseKey(const char* key) {
  Preferences lp;
  if (!lp.begin(IRLEARN_PARTITION, false, IRLEARN_PARTITION)) {
    return false;
  }
  const bool ok = lp.remove(key);
  lp.end();
  return ok;
}

static bool learnNvsKeyExists(const char* key) {
  Preferences lp;
  if (!lp.begin(IRLEARN_PARTITION, true, IRLEARN_PARTITION)) {
    return false;
  }
  const bool exists = lp.isKey(key);
  lp.end();
  return exists;
}
