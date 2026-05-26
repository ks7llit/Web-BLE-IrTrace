/*
 * IrTrace — ESP32-C5   IR Receiver + IR Transmitter + BLE UART
 * ============================================================
 * Receiver base : IRrecvDumpV3  (IRremoteESP8266 library example)
 * Transmitter   : IR_TRANSMITTER.h  (67 AC variants)
 * BLE           : NimBLE-Arduino by h2zero  v2.0+
 *
 * Libraries (Arduino Library Manager):
 *   IRremoteESP8266   by crankyoldgit
 *   NimBLE-Arduino    by h2zero  v2.0+
 *
 * Board package:
 *   esp32 by Espressif  v3.1.0+   Target: ESP32C5 Dev Module
 *
 * Hardware:
 *   IR Receiver  → GPIO3
 *   IR LED (TX)  → GPIO5
 *   Status LED   → GPIO10 (active HIGH)
 *
 * ── BLE Commands (Phone → ESP32) ────────────────────────────
 *   v <0-77>    Select AC variant
 *   on <temp>   Power ON at temp°C  (16–30)
 *   off         Power OFF
 *   t <temp>    Set temp, keep current power state
 *   r <1-10>    Set TX repeat count
 *   RAW=ON      Enable raw IR timing output on Serial
 *   RAW=OFF     Disable raw IR timing output
 *   status      ESP32 replies with current state
 *   save        Placeholder — NVS save (coming soon)
 *   wscan       Trigger live Wi-Fi scan
 *   wget        Request current Wi-Fi list
 *   WIFI_SET:<SSID>\n<PASS>  Receive Wi-Fi credentials
 *   FIND_RX:ON  Start find-receiver mode (fires t 24 every 3 s)
 *   FIND_RX:OFF Stop find-receiver mode
 *
 * ── BLE Replies (ESP32 → Phone) ─────────────────────────────
 *   TX:OK              IR sent successfully
 *   NET:X:SSID:RSSI:S  Wi-Fi scan result line
 *   WIFI:OK            Credentials received
 *   FIND_RX:ON         Find-receiver mode started
 *   FIND_RX:OFF        Find-receiver mode stopped
 *   FIND_RX:PULSE      IR fired in find mode (replaces TX:OK during find)
 *   ... (other standard replies)
 * ============================================================
 */

// ── IR receive includes ───────────────────────────────────────
#include <Arduino.h>
#include <assert.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>

// ── IR send — brand headers ───────────────────────────────────
#include <IRsend.h>
#include "ir_Mitsubishi.h"
#include "ir_MitsubishiHeavy.h"
#include "ir_Daikin.h"
#include "ir_Fujitsu.h"
#include "ir_Panasonic.h"
#include "ir_Samsung.h"
#include "ir_LG.h"
#include "ir_Hitachi.h"
#include "ir_Toshiba.h"
#include "ir_Haier.h"
#include "ir_Kelvinator.h"
#include "ir_Gree.h"
#include "ir_Midea.h"
#include "ir_Coolix.h"
#include "ir_Carrier.h"
#include "ir_Sharp.h"
#include "ir_Whirlpool.h"
#include "ir_Electra.h"
#include "ir_Vestel.h"
#include "ir_Tcl.h"
#include "ir_Teco.h"
#include "ir_Goodweather.h"
#include "ir_Neoclima.h"
#include "ir_Amcor.h"
#include "ir_Airwell.h"
#include "ir_Delonghi.h"
#include "ir_Sanyo.h"
#include "ir_Voltas.h"
#include "ir_Mirage.h"
#include "ir_Corona.h"
#include "ir_Airton.h"
#include "ir_Ecoclim.h"
#include "ir_Kelon.h"
#include "ir_Rhoss.h"
#include "ir_Technibel.h"
#include "ir_Transcold.h"
#include "ir_Trotec.h"
#include "ir_Truma.h"
#include "ir_Bosch.h"
#include "ir_Argo.h"
#include "ir_Eurom.h"
#include "ir_York.h"

// ── BLE ──────────────────────────────────────────────────────
#include <NimBLEDevice.h>

// ── WIFI ─────────────────────────────────────────────────────
#include <WiFi.h>
#include <Preferences.h>

// ── DBG macros → USB Serial ──────────────────────────────────
// Must be defined BEFORE including IR_TRANSMITTER.h
#define DBG_printf(fmt, ...)  Serial.printf(fmt, ##__VA_ARGS__)
#define DBG_println(x)        Serial.println(x)

// ── Status LED ───────────────────────────────────────────────
// Must be defined BEFORE including IR_TRANSMITTER.h
#define STATUS_LED_PIN  10
#define STATUS_LED_ON   HIGH

// Pending flags — set from ISR/callback context, consumed by pumpLed() in loop()
static bool gLedIrPending     = false;  // set by led_ir_burst(), triggers IR overlay
static bool gLedSubmitPending = false;  // set on config save, triggers submit overlay

// Called by Handle_AC() (inside IR_TRANSMITTER.h) on every IR burst.
// Must keep this exact signature. No delay() — flag pumpLed() instead.
void led_ir_burst() {
  gLedIrPending = true;
}

// ── IR_TRANSMITTER.h ─────────────────────────────────────────
// Included AFTER DBG macros and led_ir_burst() are defined.
// Defines: acVariant, acPower, acMode, kIrLed, irsend, Handle_AC()
#include "IR_TRANSMITTER.h"

// ============================================================
//  BLE — configuration
// ============================================================
#define BLE_DEVICE_NAME      "IrTrace-BLE"
#define BLE_SERVICE_UUID     "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_RX_UUID          "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_TX_UUID          "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define CFG_NAMESPACE        "irtrace_ble"
#define CFG_DEFAULT_UID      "999"
#define CFG_DEFAULT_WAKE_SEC 60u
#define CFG_ADMIN_PASS       "1234admin"

NimBLECharacteristic* gTxChar        = nullptr;
bool                  gBleConnected  = false;
bool                  gNeedsAdvRestart = false;
bool                  gAdminUnlocked = false;
Preferences           gPrefs;
String                gStoredUid      = CFG_DEFAULT_UID;
uint32_t              gStoredWakeSec  = CFG_DEFAULT_WAKE_SEC;
String                gStoredWifiSsid = "";
String                gStoredWifiPass = "";
static const uint8_t  BLE_NOTIFY_QUEUE_LEN = 24;
String                gBleNotifyQueue[BLE_NOTIFY_QUEUE_LEN];
uint8_t               gBleNotifyHead = 0;
uint8_t               gBleNotifyTail = 0;
unsigned long         gBleNextNotifyMs = 0;
bool                  gFindRxActive    = false;  // find-receiver mode flag
unsigned long         gFindRxNextMs    = 0;       // next scheduled find-mode TX

// ── LED state machine ─────────────────────────────────────────
// Base states (persistent — loop continuously)
enum LedBase    : uint8_t { LED_WIFI_SCAN, LED_BLE_ADV, LED_BLE_CONN, LED_FIND_RX };
// Overlay states (one-shot — run once, then return to base)
enum LedOverlay : uint8_t { LED_OVL_NONE, LED_OVL_IR, LED_OVL_SUBMIT };

static LedBase       gLedBase     = LED_WIFI_SCAN; // current base state
static LedOverlay    gLedOverlay  = LED_OVL_NONE;  // active overlay (or NONE)
static uint8_t       gLedOvlStep  = 0;             // overlay animation step counter
static uint8_t       gLedBaseStep = 0;             // base pattern step counter
static unsigned long gLedNextMs   = 0;             // next LED update timestamp

// ============================================================
//  WIFI SCAN — top 10 networks
// ============================================================
struct ScannedNet {
  String  ssid;
  int32_t rssi;
  uint8_t encryption;
};

static ScannedNet gWifiList[10];
static uint8_t    gWifiCount = 0;
static bool       gWifiScanPending = false;
static bool       gWifiSendPending = false;

void performWifiScan() {
  Serial.println(F("[WIFI] Starting scan..."));
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  Serial.printf("[WIFI] Scan done. Found %d networks.\n", n);

  if (n <= 0) {
    gWifiCount = 0;
  } else {
    // Basic bubble sort by RSSI (descending)
    int indices[n];
    for (int i = 0; i < n; i++) indices[i] = i;

    for (int i = 0; i < n - 1; i++) {
      for (int j = 0; j < n - i - 1; j++) {
        if (WiFi.RSSI(indices[j]) < WiFi.RSSI(indices[j+1])) {
          int temp = indices[j];
          indices[j] = indices[j+1];
          indices[j+1] = temp;
        }
      }
    }

    gWifiCount = (n > 10) ? 10 : n;
    for (int i = 0; i < gWifiCount; i++) {
      gWifiList[i].ssid       = WiFi.SSID(indices[i]);
      gWifiList[i].rssi       = WiFi.RSSI(indices[i]);
      gWifiList[i].encryption = WiFi.encryptionType(indices[i]);
      Serial.printf("  #%d: %s (%ddBm) %s\n", i, gWifiList[i].ssid.c_str(),
                    (int)gWifiList[i].rssi, (gWifiList[i].encryption == WIFI_AUTH_OPEN) ? "Open" : "Secured");
    }
  }

  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  Serial.println(F("[WIFI] Scan complete. Radio OFF."));
}

void sendWifiListToBle() {
  if (!gBleConnected) return;
  if (gWifiCount == 0) {
    bleSend("NET:NONE");
    return;
  }
  for (uint8_t i = 0; i < gWifiCount; i++) {
    String msg = "NET:" + String(i) + ":" + gWifiList[i].ssid + ":" +
                 String(gWifiList[i].rssi) + ":" +
                 ((gWifiList[i].encryption == WIFI_AUTH_OPEN) ? "Open" : "Secured");
    bleSend(msg);
  }
  bleSend("NET:DONE");
}

// ============================================================
//  IR RECEIVER — tuneable parameters
// ============================================================
const uint16_t kRecvPin           = 3;
const uint32_t kBaudRate          = 115200;
const uint16_t kCaptureBufferSize = 1024;

#if DECODE_AC
  const uint8_t kTimeout = 50;   // 50ms covers AC inter-packet gaps (40ms+)
#else
  const uint8_t kTimeout = 15;
#endif

// Raised from library default (12) to filter BLE-induced noise bursts
const uint16_t kMinUnknownSize      = 30;
const uint8_t  kTolerancePercentage = kTolerance;  // 25%

#define LEGACY_TIMING_INFO false

// Save-buffer mode: decode() calls resume() internally —
// next capture starts immediately while we process the previous one
IRrecv       irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
decode_results results;

bool gRawEnabled = false;  // toggled via BLE RAW=ON / RAW=OFF

// ============================================================
//  TX job queue
//  Commands received in BLE callback are stored here and
//  executed in loop() — keeps BLE stack responsive.
//  NEVER call Handle_AC() or delay() inside a BLE callback.
// ============================================================
struct TxJob {
  bool   pending;
  int    temp;
  String power;   // "ON" | "OFF" | "" (keep current power state)
};

static TxJob   gTxJob    = { false, 24, "" };
static uint8_t gTxRepeat = 1;   // configurable via  r <1-10>  — 1 is enough: library already sends kMinRepeat+1 frames internally
static int     gTxTemp   = 24;  // last confirmed TX temperature

// ============================================================
//  Variant name table
// ============================================================
struct VariantEntry { uint8_t id; const char* name; };
static const VariantEntry kVariantTable[] = {
  {  0, "Mitsubishi STD 144-bit"    }, {  1, "Mitsubishi 112-bit"        },
  {  2, "Mitsubishi 136-bit"        }, {  3, "Mitsubishi Heavy 152-bit"  },
  {  4, "Mitsubishi Heavy 88-bit"   }, {  5, "Daikin STD 280-bit"        },
  {  6, "Daikin2 312-bit"           }, {  7, "Daikin64 64-bit"           },
  {  8, "Daikin128 128-bit"         }, {  9, "Daikin152 152-bit"         },
  { 10, "Daikin160 160-bit"         }, { 11, "Daikin176 176-bit"         },
  { 12, "Daikin216 216-bit"         }, { 13, "Fujitsu 56-128-bit"        },
  { 14, "Panasonic 216-bit"         }, { 15, "Panasonic32 32-bit"        },
  { 16, "Samsung 112/168-bit"       }, { 17, "LG Standard 28-32-bit"     },
  { 18, "Hitachi STD 224-bit"       }, { 19, "Hitachi AC1 104-bit"       },
  { 20, "Hitachi AC264 264-bit"     }, { 21, "Hitachi AC296 296-bit"     },
  { 22, "Hitachi AC344 344-bit"     }, { 23, "Hitachi AC424 424-bit"     },
  { 24, "Toshiba 72-bit+"           }, { 25, "Haier STD 72-bit"          },
  { 26, "Haier YRW02 112-bit"       }, { 27, "Haier AC160 160-bit"       },
  { 28, "Haier AC176 176-bit"       }, { 29, "Kelvinator 128-bit"        },
  { 30, "Gree 64-bit"               }, { 31, "Midea 48-bit"              },
  { 32, "Midea24 24-bit"            }, { 33, "Coolix 24-bit"             },
  { 34, "Coolix48 48-bit"           }, { 35, "Carrier64 64-bit"          },
  { 36, "Sharp 104-bit"             }, { 37, "Whirlpool 168-bit"         },
  { 38, "Electra/AUX 104-bit"       }, { 39, "Vestel 56-bit"             },
  { 40, "TCL 112-bit"               }, { 41, "TCL 96-bit"                },
  { 42, "Teco 35-bit"               }, { 43, "Goodweather 48-bit"        },
  { 44, "Neoclima 96-bit"           }, { 45, "Amcor 64-bit"              },
  { 46, "Airwell 34-bit"            }, { 47, "Delonghi 64-bit"           },
  { 48, "Sanyo STD 72-bit"          }, { 49, "Sanyo88 88-bit"            },
  { 50, "Voltas 120-bit"            }, { 51, "Mirage 120-bit"            },
  { 52, "Corona 168-bit"            }, { 53, "Airton 56-bit"             },
  { 54, "EcoClim 56-bit"            }, { 55, "Kelon 48-bit"              },
  { 56, "Kelon168 168-bit"          }, { 57, "Rhoss 96-bit"              },
  { 58, "Technibel 56-bit"          }, { 59, "Transcold 136-bit"         },
  { 60, "Trotec STD 56-bit"         }, { 61, "Trotec3550 120-bit"        },
  { 62, "Truma variable"            }, { 63, "Bosch144 144-bit"          },
  { 64, "Argo WREM2 96-bit"         }, { 65, "Argo WREM3 variable"       },
  { 66, "Eurom 96-bit"              }, { 67, "York 88-bit"               },
  { 68, "LG2 28-32-bit"             },
};
static const uint8_t kVariantCount =
    sizeof(kVariantTable) / sizeof(kVariantTable[0]);

static const char* variantName(uint8_t id) {
  for (uint8_t i = 0; i < kVariantCount; i++)
    if (kVariantTable[i].id == id) return kVariantTable[i].name;
  return "Unknown";
}

static bool isValidVariant(uint8_t id) {
  for (uint8_t i = 0; i < kVariantCount; i++)
    if (kVariantTable[i].id == id) return true;
  return false;
}

// ============================================================
//  Stored configuration helpers
// ============================================================
static String padUid(const String& raw) {
  long value = raw.toInt();
  if (value < 1) value = 1;
  if (value > 999) value = 999;
  char buf[4];
  snprintf(buf, sizeof(buf), "%03ld", value);
  return String(buf);
}

static void loadStoredConfig() {
  if (!gPrefs.begin(CFG_NAMESPACE, true)) {
    Serial.println(F("[CFG] NVS open failed for read"));
    return;
  }

  gStoredWifiSsid = gPrefs.getString("ssid", "");
  gStoredWifiPass = gPrefs.getString("pass", "");
  const uint8_t storedVariant = gPrefs.getUChar("variant", acVariant);
  gStoredUid = padUid(gPrefs.getString("uid", CFG_DEFAULT_UID));
  gStoredWakeSec = gPrefs.getUInt("wake_sec", CFG_DEFAULT_WAKE_SEC);
  gPrefs.end();

  if (gStoredWakeSec < 60u) gStoredWakeSec = 60u;
  if (gStoredWakeSec > 36000u) gStoredWakeSec = 36000u;
  if (isValidVariant(storedVariant)) acVariant = storedVariant;

  Serial.printf("[CFG] Loaded variant=%d (%s) uid=%s wake=%lu ssid=%s\n",
                acVariant, variantName(acVariant), gStoredUid.c_str(),
                (unsigned long)gStoredWakeSec,
                gStoredWifiSsid.length() ? gStoredWifiSsid.c_str() : "(none)");
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
  const String paddedUid = padUid(uid);
  if (wakeSec < 60u) wakeSec = 60u;
  if (wakeSec > 36000u) wakeSec = 36000u;

  if (!gPrefs.begin(CFG_NAMESPACE, false)) {
    Serial.println(F("[CFG] NVS open failed for admin save"));
    return false;
  }

  gPrefs.putString("uid", paddedUid);
  gPrefs.putUInt("wake_sec", wakeSec);
  gPrefs.end();

  gStoredUid = paddedUid;
  gStoredWakeSec = wakeSec;
  return true;
}

// ============================================================
//  BLE — send one line to phone
// ============================================================
void bleSend(const String& msg) {
  if (!gBleConnected || gTxChar == nullptr) return;
  const uint8_t nextTail = (uint8_t)((gBleNotifyTail + 1U) % BLE_NOTIFY_QUEUE_LEN);
  if (nextTail == gBleNotifyHead) {
    Serial.printf("[BLE TX] Drop (queue full): %s\n", msg.c_str());
    return;
  }
  gBleNotifyQueue[gBleNotifyTail] = msg;
  gBleNotifyTail = nextTail;
}

static void pumpBleNotifications() {
  if (!gBleConnected || gTxChar == nullptr) return;
  if (gBleNotifyHead == gBleNotifyTail) return;
  if ((long)(millis() - gBleNextNotifyMs) < 0) return;

  const String msg = gBleNotifyQueue[gBleNotifyHead];
  gBleNotifyHead = (uint8_t)((gBleNotifyHead + 1U) % BLE_NOTIFY_QUEUE_LEN);
  gTxChar->setValue(msg.c_str());
  gTxChar->notify();
  gBleNextNotifyMs = millis() + 15UL;
  Serial.printf("[BLE TX] %s\n", msg.c_str());
}

// ============================================================
//  LED state machine — non-blocking, called from loop() only
//
//  Two-layer design:
//    Base state  → persistent pattern for current device mode
//    Overlay     → one-shot event animation (IR / submit success)
//                  overlays return to base state when done
//
//  setLedBase() is safe to call from BLE callbacks — it only
//  writes plain variables, never touches GPIO directly.
// ============================================================
static void setLedBase(LedBase newBase) {
  if (gLedBase == newBase) return;
  gLedBase     = newBase;
  gLedBaseStep = 0;
  // Only reset timer if no overlay is running so we don't cut it short
  if (gLedOverlay == LED_OVL_NONE) gLedNextMs = 0;
}

static void pumpLed() {
  // ── Start overlay if pending (submit takes priority over IR) ──
  if (gLedOverlay == LED_OVL_NONE) {
    if (gLedSubmitPending) {
      gLedSubmitPending = false;
      gLedOverlay  = LED_OVL_SUBMIT;
      gLedOvlStep  = 0;
      gLedNextMs   = 0;
    } else if (gLedIrPending) {
      gLedIrPending = false;
      gLedOverlay  = LED_OVL_IR;
      gLedOvlStep  = 0;
      gLedNextMs   = 0;
    }
  }

  if ((long)(millis() - gLedNextMs) < 0) return;  // not yet time

  // ── IR overlay: 3 × flash (60 ms ON / 60 ms OFF) ─────────────
  if (gLedOverlay == LED_OVL_IR) {
    if (gLedOvlStep < 6) {
      digitalWrite(STATUS_LED_PIN, (gLedOvlStep % 2 == 0) ? STATUS_LED_ON : LOW);
      gLedNextMs = millis() + 60UL;
      gLedOvlStep++;
    } else {
      // Overlay done — resume base state immediately
      gLedOverlay  = LED_OVL_NONE;
      gLedBaseStep = 0;
      gLedNextMs   = 0;
    }
    return;
  }

  // ── Submit overlay: 5 × rapid flash (80 ms) + 500 ms solid ───
  if (gLedOverlay == LED_OVL_SUBMIT) {
    if (gLedOvlStep < 10) {
      // Steps 0-9: 5 pairs of ON/OFF (even = ON, odd = OFF)
      digitalWrite(STATUS_LED_PIN, (gLedOvlStep % 2 == 0) ? STATUS_LED_ON : LOW);
      gLedNextMs = millis() + 80UL;
      gLedOvlStep++;
    } else if (gLedOvlStep == 10) {
      // Step 10: hold solid ON for 500 ms
      digitalWrite(STATUS_LED_PIN, STATUS_LED_ON);
      gLedNextMs = millis() + 500UL;
      gLedOvlStep++;
    } else {
      // Overlay done — resume base state immediately
      gLedOverlay  = LED_OVL_NONE;
      gLedBaseStep = 0;
      gLedNextMs   = 0;
    }
    return;
  }

  // ── Base state patterns ───────────────────────────────────────
  switch (gLedBase) {

    case LED_WIFI_SCAN:
      // Fast blink: 150 ms ON / 150 ms OFF
      digitalWrite(STATUS_LED_PIN, (gLedBaseStep % 2 == 0) ? STATUS_LED_ON : LOW);
      gLedNextMs = millis() + 150UL;
      gLedBaseStep++;
      break;

    case LED_BLE_ADV:
      // Heartbeat: 200 ms ON / 800 ms OFF
      if (gLedBaseStep % 2 == 0) {
        digitalWrite(STATUS_LED_PIN, STATUS_LED_ON);
        gLedNextMs = millis() + 200UL;
      } else {
        digitalWrite(STATUS_LED_PIN, LOW);
        gLedNextMs = millis() + 800UL;
      }
      gLedBaseStep++;
      break;

    case LED_BLE_CONN:
      // Solid ON — re-affirm every 250 ms in case overlay left it off
      digitalWrite(STATUS_LED_PIN, STATUS_LED_ON);
      gLedNextMs = millis() + 250UL;
      break;

    case LED_FIND_RX:
      // Double-tap + long gap — 3 s total cycle matches IR firing rate
      // Step %4 == 0: ON  80 ms
      // Step %4 == 1: OFF 80 ms
      // Step %4 == 2: ON  80 ms
      // Step %4 == 3: OFF 2760 ms   (80+80+80+2760 = 3000 ms)
      switch (gLedBaseStep % 4) {
        case 0: digitalWrite(STATUS_LED_PIN, STATUS_LED_ON); gLedNextMs = millis() +   80UL; break;
        case 1: digitalWrite(STATUS_LED_PIN, LOW);           gLedNextMs = millis() +   80UL; break;
        case 2: digitalWrite(STATUS_LED_PIN, STATUS_LED_ON); gLedNextMs = millis() +   80UL; break;
        case 3: digitalWrite(STATUS_LED_PIN, LOW);           gLedNextMs = millis() + 2760UL; break;
      }
      gLedBaseStep++;
      break;
  }
}

static void bleSendAdminConfig() {
  bleSend("ADM_UID:" + gStoredUid);
  bleSend("ADM_WAKE:" + String(gStoredWakeSec));
}

// ============================================================
//  BLE — send current state to phone
// ============================================================
void bleSendStatus() {
  bleSend("VAR:"  + String(acVariant));
  bleSend("PWR:"  + String(acPower ? "ON" : "OFF"));
  bleSend("TEMP:" + String(gTxTemp));
  bleSend("RPT:"  + String(gTxRepeat));
  bleSend("RAW:"  + String(gRawEnabled ? "ON" : "OFF"));
}

// ============================================================
//  BLE — server connection callbacks
// ============================================================
class BleServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
    gBleConnected = true;
    gAdminUnlocked = false;
    gBleNotifyHead = 0;
    gBleNotifyTail = 0;
    gBleNextNotifyMs = millis() + 300UL;
    Serial.println(F("[BLE] Phone connected"));
    setLedBase(LED_BLE_CONN);  // solid ON — user is now in config mode
    bleSendStatus();        // sync app with current device state on connect
    gWifiSendPending = true; // send Wi-Fi list from loop(), not from callback
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
    gBleConnected    = false;
    gNeedsAdvRestart = true;
    gAdminUnlocked   = false;
    gFindRxActive    = false;   // stop find mode on disconnect
    gBleNotifyHead   = 0;
    gBleNotifyTail   = 0;
    setLedBase(LED_BLE_ADV);    // back to heartbeat — advertising again
    Serial.printf("[BLE] Disconnected (reason %d)\n", reason);
  }
};

// ============================================================
//  BLE — RX callback
//  RULE: never block here, never call Handle_AC() here.
//        Queue via gTxJob and execute in loop() only.
// ============================================================
class BleRxCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
    String cmd = String(c->getValue().c_str());
    cmd.trim();
    if (cmd.length() == 0) return;
    Serial.printf("[BLE RX] %s\n", cmd.c_str());

    // ── wscan  trigger Wi-Fi scan ─────────────────────────
    if (cmd.equalsIgnoreCase("wscan")) {
      gWifiScanPending = true;
      return;
    }

    // ── wget  request current Wi-Fi list ──────────────────
    if (cmd.equalsIgnoreCase("wget")) {
      sendWifiListToBle();
      return;
    }

    // ── WIFI_SET:<SSID>\n<PASS> ───────────────────────────
    if (cmd.startsWith("WIFI_SET:")) {
      String payload = cmd.substring(9);
      int nl = payload.indexOf('\n');
      String ssid = (nl == -1) ? payload : payload.substring(0, nl);
      String pass = (nl == -1) ? "" : payload.substring(nl + 1);
      ssid.trim();
      if (ssid.length() == 0) {
        bleSend("ERR:SSID is required");
        return;
      }
      if (!saveWifiConfig(ssid, pass)) {
        bleSend("ERR:Wi-Fi save failed");
        return;
      }
      Serial.printf("[WIFI] Saved Credentials: SSID='%s' PASS='%s'\n",
                    ssid.c_str(), pass.c_str());
      bleSend("WIFI:OK");
      gLedSubmitPending = true;  // trigger success overlay
      return;
    }

    if (cmd.startsWith("ADMIN_AUTH:")) {
      String pass = cmd.substring(11);
      pass.trim();
      gAdminUnlocked = (pass == CFG_ADMIN_PASS);
      bleSend(gAdminUnlocked ? "ADMIN:AUTH_OK" : "ADMIN:AUTH_FAIL");
      if (gAdminUnlocked) {
        bleSendAdminConfig();
      }
      return;
    }

    if (cmd.equalsIgnoreCase("ADMIN_GET")) {
      if (!gAdminUnlocked) {
        bleSend("ADMIN:LOCKED");
        return;
      }
      bleSendAdminConfig();
      return;
    }

    if (cmd.startsWith("ADMIN_SAVE:")) {
      if (!gAdminUnlocked) {
        bleSend("ADMIN:LOCKED");
        return;
      }

      String payload = cmd.substring(11);
      int nl = payload.indexOf('\n');
      if (nl == -1) {
        bleSend("ERR:ADMIN_SAVE format is UID\\nWAKE_SEC");
        return;
      }

      String uid = payload.substring(0, nl);
      String wakeRaw = payload.substring(nl + 1);
      uid.trim();
      wakeRaw.trim();
      if (wakeRaw.length() == 0) {
        bleSend("ERR:Wake interval is required");
        return;
      }

      const uint32_t wakeSec = (uint32_t)wakeRaw.toInt();
      if (!saveAdminConfig(uid, wakeSec)) {
        bleSend("ERR:Admin save failed");
        return;
      }

      Serial.printf("[ADMIN] Saved uid=%s wake=%lu sec\n",
                    gStoredUid.c_str(), (unsigned long)gStoredWakeSec);
      bleSend("ADMIN:SAVED");
      gLedSubmitPending = true;  // trigger success overlay
      bleSendAdminConfig();
      return;
    }

    // ── FIND_RX:ON / FIND_RX:OFF  find-receiver mode ─────
    if (cmd.equalsIgnoreCase("FIND_RX:ON")) {
      gFindRxActive = true;
      gFindRxNextMs = millis();    // fire immediately on first cycle
      setLedBase(LED_FIND_RX);     // double-tap pattern synced to IR cadence
      bleSend("FIND_RX:ON");
      Serial.println(F("[FIND] Receiver-hunt mode ON"));
      return;
    }
    if (cmd.equalsIgnoreCase("FIND_RX:OFF")) {
      gFindRxActive = false;
      setLedBase(LED_BLE_CONN);    // back to solid ON — still connected
      bleSend("FIND_RX:OFF");
      Serial.println(F("[FIND] Receiver-hunt mode OFF"));
      return;
    }

    // ── v <id>  select variant ────────────────────────────
    if (cmd.startsWith("v ") || cmd.startsWith("V ")) {
      int id = cmd.substring(2).toInt();
      if (!isValidVariant((uint8_t)id)) {
        bleSend("ERR:Unknown variant " + String(id));
        return;
      }
      acVariant = (uint8_t)id;
      Serial.printf("[VAR] Set to %d (%s)\n", acVariant, variantName(acVariant));
      bleSend("VAR:" + String(acVariant));
      return;
    }

    // ── on <temp>  power ON ───────────────────────────────
    if (cmd.startsWith("on") || cmd.startsWith("ON")) {
      int temp = gTxTemp;
      if (cmd.length() > 3) temp = cmd.substring(3).toInt();
      if (temp < 16 || temp > 30) { bleSend("ERR:Temp out of range 16-30"); return; }
      gTxTemp        = temp;
      gTxJob.temp    = temp;
      gTxJob.power   = "ON";
      gTxJob.pending = true;
      return;
    }

    // ── off  power OFF ────────────────────────────────────
    if (cmd.equalsIgnoreCase("off")) {
      gTxJob.temp    = gTxTemp;
      gTxJob.power   = "OFF";
      gTxJob.pending = true;
      return;
    }

    // ── t <temp>  set temperature ─────────────────────────
    if (cmd.startsWith("t ") || cmd.startsWith("T ")) {
      int temp = cmd.substring(2).toInt();
      if (temp < 16 || temp > 30) { bleSend("ERR:Temp out of range 16-30"); return; }
      gTxTemp        = temp;
      gTxJob.temp    = temp;
      gTxJob.power   = "";   // keep current power state
      gTxJob.pending = true;
      return;
    }

    // ── r <1-10>  set repeat count ────────────────────────
    if (cmd.startsWith("r ") || cmd.startsWith("R ")) {
      int rpt = cmd.substring(2).toInt();
      if (rpt < 1 || rpt > 10) { bleSend("ERR:Repeat must be 1-10"); return; }
      gTxRepeat = (uint8_t)rpt;
      Serial.printf("[RPT] Set to %d\n", gTxRepeat);
      bleSend("RPT:" + String(gTxRepeat));
      return;
    }

    // ── RAW=ON / RAW=OFF ──────────────────────────────────
    if (cmd.equalsIgnoreCase("RAW=ON"))  { gRawEnabled = true;  bleSend("RAW:ON");  return; }
    if (cmd.equalsIgnoreCase("RAW=OFF")) { gRawEnabled = false; bleSend("RAW:OFF"); return; }

    // ── status ────────────────────────────────────────────
    if (cmd.equalsIgnoreCase("status")) { bleSendStatus(); return; }

    // ── save  (NVS placeholder — coming soon) ─────────────
    if (cmd.equalsIgnoreCase("save")) {
      Serial.printf("[NVS] Save requested — variant=%d (%s)  [NVS not yet implemented]\n",
                    acVariant, variantName(acVariant));
      if (!saveVariantConfig(acVariant)) {
        bleSend("ERR:Variant save failed");
        return;
      }
      Serial.printf("[CFG] Saved variant=%d (%s)\n", acVariant, variantName(acVariant));
      bleSend("SAVE:OK");
      gLedSubmitPending = true;  // trigger success overlay
      bleSend("VAR:" + String(acVariant));   // echo back confirmation for now
      return;
    }

    bleSend("ERR:Unknown cmd. Use v/on/off/t/r/status/RAW=ON|OFF");
  }
};

// ============================================================
//  BLE — setup
// ============================================================
void setupBle() {
  Serial.println(F("[BLE] Initializing..."));
  NimBLEDevice::init(BLE_DEVICE_NAME);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new BleServerCB());

  NimBLEService* svc = server->createService(BLE_SERVICE_UUID);

  gTxChar = svc->createCharacteristic(
    BLE_TX_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  gTxChar->setValue("IrTrace Ready");

  NimBLECharacteristic* rxChar = svc->createCharacteristic(
    BLE_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(new BleRxCB());

  svc->start();

  // Filter by SERVICE UUID — not by name
  // Chrome Web Bluetooth reads main ad packet (UUID), not scan response (name)
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SERVICE_UUID);
  adv->setName(BLE_DEVICE_NAME);
  adv->enableScanResponse(true);
  adv->start();

  Serial.printf("[BLE] Advertising as: %s\n", BLE_DEVICE_NAME);
}

// ============================================================
//  TX executor — called from loop() only, NEVER from callbacks
//
//  IR receiver is paused during TX so it doesn't capture
//  our own transmitted signal as an incoming remote.
//  RMT handles IR timing in hardware — BLE stays connected
//  throughout TX with no interference.
// ============================================================
void executeTx() {
  if (!gTxJob.pending) return;
  gTxJob.pending = false;

  Serial.printf("[TX] variant=%d (%s)  power=%s  temp=%d  repeat=%d\n",
                acVariant, variantName(acVariant),
                gTxJob.power.c_str(), gTxJob.temp, gTxRepeat);

  // Pause IR receiver — prevents self-capture of transmitted signal
  irrecv.disableIRIn();

  for (uint8_t i = 0; i < gTxRepeat; i++) {
    Serial.printf("[TX] %d/%d\n", i + 1, gTxRepeat);
    Handle_AC(gTxJob.temp, gTxJob.power);
    if (i < gTxRepeat - 1) delay(200);  // gap between repeats
  }

  // Resume IR receiver
  irrecv.enableIRIn();

  // Mirror power state
  if      (gTxJob.power == "ON")  acPower = true;
  else if (gTxJob.power == "OFF") acPower = false;

  // In find mode send a single pulse heartbeat — keeps the log clean
  if (gFindRxActive) {
    bleSend("FIND_RX:PULSE");
  } else {
    bleSend("TX:OK");
    bleSend("PWR:"  + String(acPower ? "ON" : "OFF"));
    bleSend("TEMP:" + String(gTxJob.temp));
  }
}

// ============================================================
//  IR RX — send decoded result to phone
// ============================================================
void sendDecodedToBle(const decode_results* r) {
  if (!gBleConnected) return;
  if (r->decode_type == decode_type_t::UNKNOWN) return;  // skip BLE noise

  bleSend("PROTOCOL:" + typeToString(r->decode_type, r->repeat));

  stdAc::state_t state;
  IRac::initState(&state);
  const bool hasState = IRAcUtils::decodeToState(r, &state);

  if (hasState) {
    bleSend("POWER:" + String(state.power ? "ON" : "OFF"));
    bleSend("MODE:"  + IRac::opmodeToString(state.mode, true));
    String temp = String(state.degrees, 1);
    temp += state.celsius ? "C" : "F";
    bleSend("TEMP_RX:" + temp);   // TEMP_RX avoids clash with TX TEMP echo
    bleSend("FAN:"   + IRac::fanspeedToString(state.fanspeed));
  } else {
    bleSend("POWER:N/A");
    bleSend("MODE:N/A");
    bleSend("TEMP_RX:N/A");
    bleSend("FAN:N/A");
  }
}

// ============================================================
//  setup()
// ============================================================
void setup() {
  // Status LED — solid ON immediately to show device is alive
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, STATUS_LED_ON);

  // IR TX pin — drive LOW at boot, prevents floating GPIO5
  pinMode(kIrLed, OUTPUT);
  digitalWrite(kIrLed, LOW);
  irsend.begin();

#if ARDUINO_USB_CDC_ON_BOOT
  Serial.begin(kBaudRate);
#else
  Serial.begin(kBaudRate, SERIAL_8N1);
#endif
  while (!Serial) delay(50);

  assert(irutils::lowLevelSanityCheck() == 0);
  Serial.printf("\n" D_STR_IRRECVDUMP_STARTUP "\n", kRecvPin);
  loadStoredConfig();

  // 1. Wi-Fi Scan — LED stays solid ON (loop not running yet, can't animate)
  performWifiScan();

  // 2. BLE Setup — after this, loop() takes over and animates LED
  setupBle();
  setLedBase(LED_BLE_ADV);  // heartbeat: advertising, waiting for connection

#if DECODE_HASH
  irrecv.setUnknownThreshold(kMinUnknownSize);
#endif
  irrecv.setTolerance(kTolerancePercentage);
  irrecv.enableIRIn();

  Serial.println(F("[IR] Receiver ready  (RX=GPIO3  TX=GPIO5)"));
  Serial.println(F("[IR] Point remote at sensor and press a button"));
  Serial.printf("[TX] Default variant=%d (%s)  repeat=%d\n",
                acVariant, variantName(acVariant), gTxRepeat);
}

// ============================================================
//  loop()
// ============================================================
void loop() {
  pumpLed();              // LED state machine — always first, never blocks
  pumpBleNotifications();

  // Send cached Wi-Fi list to newly connected phone (deferred from onConnect)
  if (gWifiSendPending) {
    gWifiSendPending = false;
    sendWifiListToBle();
  }

  // Handle Wi-Fi scan request (Live Re-scan)
  if (gWifiScanPending) {
    gWifiScanPending = false;
    performWifiScan();
    sendWifiListToBle();
  }

  // Restart BLE advertising after disconnect
  if (gNeedsAdvRestart) {
    gNeedsAdvRestart = false;
    NimBLEDevice::startAdvertising();
    Serial.println(F("[BLE] Advertising restarted"));
  }

  // Execute queued TX — always before IR decode, always in loop()
  executeTx();

  // Find-receiver mode — queue t 24 every 3 s, only when TX slot is free
  if (gFindRxActive && !gTxJob.pending &&
      (long)(millis() - gFindRxNextMs) >= 0) {
    gTxJob.temp    = 24;
    gTxJob.power   = "";          // temperature only — no power toggle
    gTxJob.pending = true;
    gFindRxNextMs  = millis() + 3000UL;
  }

  // IR decode — IRrecvDumpV3 core logic
  if (irrecv.decode(&results)) {
    // save-buffer mode: resume() already called internally by decode()

    uint32_t now = millis();
    Serial.printf(D_STR_TIMESTAMP " : %06u.%03u\n", (uint32_t)(now / 1000), (uint32_t)(now % 1000));

    if (results.overflow)
      Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);

    Serial.println(D_STR_LIBRARY "   : v" _IRREMOTEESP8266_VERSION_STR "\n");

    if (kTolerancePercentage != kTolerance)
      Serial.printf(D_STR_TOLERANCE " : %d%%\n", kTolerancePercentage);

    Serial.print(resultToHumanReadableBasic(&results));

    String description = IRAcUtils::resultAcToString(&results);
    if (description.length())
      Serial.println(D_STR_MESGDESC ": " + description);

    yield();

#if LEGACY_TIMING_INFO
    Serial.println(resultToTimingInfo(&results));
    yield();
#endif

    if (gRawEnabled) {
      Serial.println(resultToTimingInfo(&results));
      yield();
    }

    Serial.println(resultToSourceCode(&results));
    Serial.println();
    yield();

    // Send decoded result to phone
    sendDecodedToBle(&results);
  }
}
