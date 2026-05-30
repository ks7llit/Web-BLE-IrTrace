
bool     acPower    = false;
String   acMode     = "cool";
uint16_t acSendDelay = 100;   // ms pause after each ac.send() — configurable via 'd=<ms>'

const uint16_t kIrLed = 5;

// Default IR repeat count
#define kDefaultRepeat 1

// ============================================================
// IR send object — RMT-backed for hardware-timed TX.
// All variants 0–68 use irsend.sendXxx() via hardware RMT.
// No variant calls ac.send() (bit-bang) any longer.
// ============================================================
#include "IRsendRMT.h"

IRsendRMT irsend(kIrLed);


// ============================================================
// AC Variant Index – FSM selector
// ============================================================

// ── Mitsubishi ───────────────────────────────────────────────
#define AC_MITSU_STD        0   // IRMitsubishiAC        144-bit
#define AC_MITSU_112        1   // IRMitsubishi112       112-bit  ← default
#define AC_MITSU_136        2   // IRMitsubishi136       136-bit
#define AC_MITSU_HEAVY_152  3   // IRMitsubishiHeavy152Ac 152-bit
#define AC_MITSU_HEAVY_88   4   // IRMitsubishiHeavy88Ac  88-bit

// ── Daikin ───────────────────────────────────────────────────
#define AC_DAIKIN_STD       5   // IRDaikinESP           280-bit
#define AC_DAIKIN2          6   // IRDaikin2             312-bit
#define AC_DAIKIN64         7   // IRDaikin64             64-bit
#define AC_DAIKIN128        8   // IRDaikin128           128-bit
#define AC_DAIKIN152        9   // IRDaikin152           152-bit
#define AC_DAIKIN160       10   // IRDaikin160           160-bit
#define AC_DAIKIN176       11   // IRDaikin176           176-bit
#define AC_DAIKIN216       12   // IRDaikin216           216-bit

// ── Fujitsu ──────────────────────────────────────────────────
#define AC_FUJITSU         13   // IRFujitsuAC           56-128-bit

// ── Panasonic ────────────────────────────────────────────────
#define AC_PANASONIC       14   // IRPanasonicAc         216-bit
#define AC_PANASONIC32     15   // IRPanasonicAc32        32-bit

// ── Samsung ──────────────────────────────────────────────────
#define AC_SAMSUNG         16   // IRSamsungAc           112/168-bit

// ── LG ───────────────────────────────────────────────────────
#define AC_LG              17   // IRLgAc (GE6711AR2853M) 28-32-bit  LG standard
#define AC_LG2             68   // IRLgAc (AKB75215403)   28-32-bit  LG2 protocol

// ── Hitachi ──────────────────────────────────────────────────
#define AC_HITACHI_STD     18   // IRHitachiAc           224-bit
#define AC_HITACHI_AC1     19   // IRHitachiAc1          104-bit
#define AC_HITACHI_AC264   20   // IRHitachiAc264        264-bit
#define AC_HITACHI_AC296   21   // IRHitachiAc296        296-bit
#define AC_HITACHI_AC344   22   // IRHitachiAc344        344-bit
#define AC_HITACHI_AC424   23   // IRHitachiAc424        424-bit

// ── Toshiba ──────────────────────────────────────────────────
#define AC_TOSHIBA         24   // IRToshibaAC            72-bit+

// ── Haier ────────────────────────────────────────────────────
#define AC_HAIER_STD       25   // IRHaierAC              72-bit
#define AC_HAIER_YRW02     26   // IRHaierACYRW02        112-bit
#define AC_HAIER_160       27   // IRHaierAC160          160-bit
#define AC_HAIER_176       28   // IRHaierAC176          176-bit

// ── Kelvinator ───────────────────────────────────────────────
#define AC_KELVINATOR      29   // IRKelvinatorAC        128-bit

// ── Gree ─────────────────────────────────────────────────────
#define AC_GREE            30   // IRGreeAC               64-bit

// ── Midea ────────────────────────────────────────────────────
#define AC_MIDEA           31   // IRMideaAC              48-bit
#define AC_MIDEA24         32   // IRMideaAC              24-bit

// ── Coolix ───────────────────────────────────────────────────
#define AC_COOLIX          33   // IRCoolixAC             24-bit
#define AC_COOLIX48        34   // IRCoolixAC             48-bit

// ── Carrier ──────────────────────────────────────────────────
#define AC_CARRIER64       35   // IRCarrierAc64          64-bit

// ── Sharp ────────────────────────────────────────────────────
#define AC_SHARP           36   // IRSharpAc             104-bit

// ── Whirlpool ────────────────────────────────────────────────
#define AC_WHIRLPOOL       37   // IRWhirlpoolAc         168-bit

// ── Electra / AUX ────────────────────────────────────────────
#define AC_ELECTRA         38   // IRElectraAc           104-bit

// ── Vestel ───────────────────────────────────────────────────
#define AC_VESTEL          39   // IRVestelAc             56-bit

// ── TCL ──────────────────────────────────────────────────────
#define AC_TCL112          40   // IRTcl112Ac (TAC09CHSD) 112-bit
#define AC_TCL96           41   // IRTcl112Ac (GZ055BE1)   96-bit

// ── Teco ─────────────────────────────────────────────────────
#define AC_TECO            42   // IRTecoAc               35-bit

// ── Goodweather ──────────────────────────────────────────────
#define AC_GOODWEATHER     43   // IRGoodweatherAc        48-bit

// ── Neoclima ─────────────────────────────────────────────────
#define AC_NEOCLIMA        44   // IRNeoclimaAc           96-bit

// ── Amcor ────────────────────────────────────────────────────
#define AC_AMCOR           45   // IRAmcorAc              64-bit

// ── Airwell ──────────────────────────────────────────────────
#define AC_AIRWELL         46   // IRAirwellAc            34-bit

// ── Delonghi ─────────────────────────────────────────────────
#define AC_DELONGHI        47   // IRDelonghiAc           64-bit

// ── Sanyo ────────────────────────────────────────────────────
#define AC_SANYO_STD       48   // IRSanyoAc              72-bit
#define AC_SANYO88         49   // IRSanyoAc88            88-bit

// ── Voltas ───────────────────────────────────────────────────
#define AC_VOLTAS          50   // IRVoltas              120-bit

// ── Mirage ───────────────────────────────────────────────────
#define AC_MIRAGE          51   // IRMirageAc            120-bit

// ── Corona ───────────────────────────────────────────────────
#define AC_CORONA          52   // IRCoronaAc            168-bit

// ── Airton ───────────────────────────────────────────────────
#define AC_AIRTON          53   // IRAirtonAc             56-bit

// ── EcoClim ──────────────────────────────────────────────────
#define AC_ECOCLIM         54   // IREcoclimAc            56-bit

// ── Kelon ────────────────────────────────────────────────────
#define AC_KELON           55   // IRKelonAc              48-bit
#define AC_KELON168        56   // IRKelon168Ac          168-bit

// ── Rhoss ────────────────────────────────────────────────────
#define AC_RHOSS           57   // IRRhossAc              96-bit

// ── Technibel ────────────────────────────────────────────────
#define AC_TECHNIBEL       58   // IRTechnibelAc          56-bit

// ── Transcold ────────────────────────────────────────────────
#define AC_TRANSCOLD       59   // IRTranscoldAc         136-bit

// ── Trotec ───────────────────────────────────────────────────
#define AC_TROTEC_STD      60   // IRTrotecESP            56-bit
#define AC_TROTEC3550      61   // IRTrotec3550          120-bit

// ── Truma ────────────────────────────────────────────────────
#define AC_TRUMA           62   // IRTrumaAc             variable

// ── Bosch ────────────────────────────────────────────────────
#define AC_BOSCH144        63   // IRBosch144AC          144-bit

// ── Argo ─────────────────────────────────────────────────────
#define AC_ARGO_WREM2      64   // IRArgoAC               96-bit
#define AC_ARGO_WREM3      65   // IRArgoAC_WREM3        variable

// ── Eurom ────────────────────────────────────────────────────
#define AC_EUROM           66   // IREuromAc              96-bit

// ── York ─────────────────────────────────────────────────────
#define AC_YORK            67   // IRYorkAc               88-bit


uint8_t acVariant = 1;

IRMitsubishi112 acMitsubishi112(kIrLed);

// ============================================================
// Handle_AC()
// power : "ON"  → setPower(true)  + setTemp + setMode
//         "OFF" → setPower(false) only
//         ""    → setPower(acPower) + setTemp + setMode (preserve state)
// t     : target temperature °C
// ============================================================
void Handle_AC(uint8_t t, const String& power) {

  if      (power == "ON")  acPower = true;
  else if (power == "OFF") acPower = false;

  DBG_printf("[IR] Variant=%d  Power=%s  Temp=%d  Mode=COOL\n",
             acVariant, power.c_str(), t);

  switch (acVariant) {

    // ── 0: Mitsubishi Standard (144-bit) ─────────────────────
    case AC_MITSU_STD: {
        IRMitsubishiAC ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kMitsubishiAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kMitsubishiAcCool); }
        irsend.sendMitsubishiAC(ac.getRaw(), kMitsubishiACStateLength, kMitsubishiACMinRepeat);
        // kMitsubishiAcRptSpace = 15500µs < kRmtGapThresholdUs (36000µs) so
        // _flushBuffer() is never triggered automatically.  Add a closing mark
        // (gives receiver ISR the rising edge to latch the 15500µs trailing space)
        // then space(36001) triggers the auto-flush.
        // Constants are defined in ir_Mitsubishi.cpp (not exported) → hardcoded.
        irsend.mark(440);    // kMitsubishiAcRptMark = 440µs
        irsend.space(36001); // >= kRmtGapThresholdUs (36000µs) — flushes buffer to RMT HW
        delay(acSendDelay);
        break;
      }

    // ── 1: Mitsubishi 112-bit ─────────────────────────────────
    case AC_MITSU_112: {
        IRMitsubishi112 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kMitsubishi112Cool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kMitsubishi112Cool); }
        irsend.sendMitsubishi112(ac.getRaw(), kMitsubishi112StateLength, kMitsubishi112MinRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 2: Mitsubishi 136-bit ─────────────────────────────────
    case AC_MITSU_136: {
        IRMitsubishi136 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kMitsubishi136Cool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kMitsubishi136Cool); }
        irsend.sendMitsubishi136(ac.getRaw(), kMitsubishi136StateLength);
        delay(acSendDelay);
        break;
      }

    // ── 3: Mitsubishi Heavy 152-bit ───────────────────────────
    case AC_MITSU_HEAVY_152: {
        IRMitsubishiHeavy152Ac ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kMitsubishiHeavyCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kMitsubishiHeavyCool); }
        irsend.sendMitsubishiHeavy152(ac.getRaw(), kMitsubishiHeavy152StateLength, kMitsubishiHeavy152MinRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 4: Mitsubishi Heavy 88-bit ────────────────────────────
    case AC_MITSU_HEAVY_88: {
        IRMitsubishiHeavy88Ac ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kMitsubishiHeavyCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kMitsubishiHeavyCool); }
        irsend.sendMitsubishiHeavy88(ac.getRaw(), kMitsubishiHeavy88StateLength, kMitsubishiHeavy88MinRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 5: Daikin Standard 280-bit ───────────────────────────────
    case AC_DAIKIN_STD: {
        IRDaikinESP ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kDaikinCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kDaikinCool); }
        irsend.sendDaikin(ac.getRaw(), kDaikinStateLength, kDaikinDefaultRepeat);
        // kDaikinGap = 29000µs — all 4 sections (preamble+3 data) now stay buffered
        // together (29428µs < kRmtGapThresholdUs=36000µs) and transmit in one RMT burst.
        // Closure mark gives receiver ISR rising edge to latch the trailing 29428µs space.
        irsend.mark(kDaikinBitMark);  // 428µs — closes trailing gap in receiver rawData
        irsend.space(36001);           // >= kRmtGapThresholdUs — flushes all sections to RMT HW
        delay(acSendDelay);
        break;
      }

    // ── 6: Daikin2 312-bit ────────────────────────────────────
    case AC_DAIKIN2: {
        IRDaikin2 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kDaikinCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kDaikinCool); }
        irsend.sendDaikin2(ac.getRaw(), kDaikin2StateLength, kDaikin2DefaultRepeat);
        // kDaikin2Gap = 35204µs (kDaikin2LeaderMark+kDaikin2LeaderSpace) — leader + 2
        // sections all buffered together (35204µs < kRmtGapThresholdUs=36000µs).
        irsend.mark(kDaikin2BitMark);  // 460µs — closes trailing gap in receiver rawData
        irsend.space(36001);            // >= kRmtGapThresholdUs — flushes all sections to RMT HW
        delay(acSendDelay);
        break;
      }

    // ── 7: Daikin 64-bit (toggle protocol) ───────────────────
    case AC_DAIKIN64: {
        IRDaikin64 ac(kIrLed);
        // BUG FIX (Bug #17): Daikin64 is a toggle-style protocol — setPowerToggle(true)
        // signals "change power state" for both ON and OFF commands.  The original OFF
        // branch omitted setTemp() and setMode(), so the temperature field was taken
        // from stateReset() defaults (minimum = 16°C) instead of the requested setpoint.
        // The AC unit needs valid temp+mode even in toggle-off frames.
        // Fix: mirror the ON branch in the OFF branch (same setters, same toggle bit).
        if      (power == "ON")  { ac.setPowerToggle(true); ac.setTemp(t); ac.setMode(kDaikin64Cool); }
        else if (power == "OFF") { ac.setPowerToggle(true); ac.setTemp(t); ac.setMode(kDaikin64Cool); }
        else                     { ac.setTemp(t); ac.setMode(kDaikin64Cool); }
        irsend.sendDaikin64(ac.getRaw(), kDaikin64Bits, kDaikin64DefaultRepeat);
        // No manual flush needed — sendDaikin64 appends mark(kDaikin64HdrMark) then
        // space(kDefaultMessageGap=100ms) after the data section. 100000µs >= 36000µs
        // threshold → auto-flush fires, transmitting all sections in one RMT burst.
        delay(acSendDelay);
        break;
      }

    // ── 8: Daikin 128-bit (toggle protocol) ──────────────────
    case AC_DAIKIN128: {
        IRDaikin128 ac(kIrLed);
        // BUG FIX (Bug #17): same toggle-protocol issue as V7.  stateReset() leaves
        // Mode=0 (UNKNOWN) and Temp=0°C.  Without setTemp()+setMode() in the OFF
        // branch the receiver decodes Mode: 0 (UNKNOWN), Temp: 0°C for every OFF frame.
        if      (power == "ON")  { ac.setPowerToggle(true); ac.setTemp(t); ac.setMode(kDaikin128Cool); }
        else if (power == "OFF") { ac.setPowerToggle(true); ac.setTemp(t); ac.setMode(kDaikin128Cool); }
        else                     { ac.setTemp(t); ac.setMode(kDaikin128Cool); }
        irsend.sendDaikin128(ac.getRaw(), kDaikin128StateLength, kDaikin128DefaultRepeat);
        // kDaikin128Gap = 20300µs — leaders + 2 sections all buffered together
        // (20300µs < kRmtGapThresholdUs=36000µs) and transmit in one RMT burst.
        irsend.mark(kDaikin128BitMark);  // 350µs — closes trailing gap in receiver rawData
        irsend.space(36001);              // >= kRmtGapThresholdUs — flushes all sections to RMT HW
        delay(acSendDelay);
        break;
      }

    // ── 9: Daikin 152-bit ────────────────────────────────────
    case AC_DAIKIN152: {
        IRDaikin152 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kDaikinCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kDaikinCool); }
        irsend.sendDaikin152(ac.getRaw(), kDaikin152StateLength, kDaikin152DefaultRepeat);
        // kDaikin152Gap = 25182µs — leader + data section buffered together
        // (25182µs < kRmtGapThresholdUs=36000µs) and transmit in one RMT burst.
        irsend.mark(kDaikin152BitMark);  // 433µs — closes trailing gap in receiver rawData
        irsend.space(36001);              // >= kRmtGapThresholdUs — flushes all sections to RMT HW
        delay(acSendDelay);
        break;
      }

    // ── 10: Daikin 160-bit ───────────────────────────────────
    case AC_DAIKIN160: {
        IRDaikin160 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kDaikinCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kDaikinCool); }
        irsend.sendDaikin160(ac.getRaw(), kDaikin160StateLength, kDaikin160DefaultRepeat);
        // kDaikin160Gap = 29650µs — 2 sections buffered together
        // (29650µs < kRmtGapThresholdUs=36000µs) and transmit in one RMT burst.
        irsend.mark(kDaikin160BitMark);  // 342µs — closes trailing gap in receiver rawData
        irsend.space(36001);              // >= kRmtGapThresholdUs — flushes all sections to RMT HW
        delay(acSendDelay);
        break;
      }

    // ── 11: Daikin 176-bit ───────────────────────────────────
    case AC_DAIKIN176: {
        IRDaikin176 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kDaikin176Cool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kDaikin176Cool); }
        irsend.sendDaikin176(ac.getRaw(), kDaikin176StateLength, kDaikin176DefaultRepeat);
        // kDaikin176Gap = 29410µs — 2 sections buffered together
        // (29410µs < kRmtGapThresholdUs=36000µs) and transmit in one RMT burst.
        irsend.mark(kDaikin176BitMark);  // 370µs — closes trailing gap in receiver rawData
        irsend.space(36001);              // >= kRmtGapThresholdUs — flushes all sections to RMT HW
        delay(acSendDelay);
        break;
      }

    // ── 12: Daikin 216-bit ───────────────────────────────────
    case AC_DAIKIN216: {
        IRDaikin216 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kDaikinCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kDaikinCool); }
        irsend.sendDaikin216(ac.getRaw(), kDaikin216StateLength, kDaikin216DefaultRepeat);
        // kDaikin216Gap = 29650µs — 2 sections buffered together
        // (29650µs < kRmtGapThresholdUs=36000µs) and transmit in one RMT burst.
        irsend.mark(kDaikin216BitMark);  // 420µs — closes trailing gap in receiver rawData
        irsend.space(36001);              // >= kRmtGapThresholdUs — flushes all sections to RMT HW
        delay(acSendDelay);
        break;
      }

    // ── 13: Fujitsu ──────────────────────────────────────────
    case AC_FUJITSU: {
        IRFujitsuAC ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kFujitsuAcModeCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kFujitsuAcModeCool); }
        irsend.sendFujitsuAC(ac.getRaw(), ac.getStateLength(), kFujitsuAcMinRepeat);
        // kFujitsuAcMinGap = 8100µs < kRmtGapThresholdUs (36000µs) → no auto-flush.
        // Without an explicit flush the frame stays buffered indefinitely; on the
        // next protocol's sendXxx() call the stale Fujitsu frame transmits first,
        // then the intended protocol follows — corrupting both decode results.
        // Constants kFujitsuAcBitMark and kFujitsuAcMinGap are defined in
        // ir_Fujitsu.cpp only (not exported) → hardcoded numeric values here.
        irsend.mark(448);    // kFujitsuAcBitMark = 448µs — closes trailing gap in receiver rawData
        irsend.space(36001); // >= kRmtGapThresholdUs (36000µs) — flushes frame to RMT HW
        delay(acSendDelay);
        break;
      }

    // ── 14: Panasonic 216-bit ────────────────────────────────
    case AC_PANASONIC: {
        IRPanasonicAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kPanasonicAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kPanasonicAcCool); }
        irsend.sendPanasonicAC(ac.getRaw(), kPanasonicAcStateLength, kPanasonicAcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 15: Panasonic 32-bit (toggle) ────────────────────────
    case AC_PANASONIC32: {
        IRPanasonicAc32 ac(kIrLed);
        // BUG FIX (Bug #17): same toggle-protocol issue as V7/V8.  stateReset()
        // defaults leave Temp=16°C (minimum).  OFF branch must include setTemp()+
        // setMode() so the transmitted frame carries the correct setpoint.
        if      (power == "ON")  { ac.setPowerToggle(true); ac.setTemp(t); ac.setMode(kPanasonicAc32Cool); }
        else if (power == "OFF") { ac.setPowerToggle(true); ac.setTemp(t); ac.setMode(kPanasonicAc32Cool); }
        else                     { ac.setTemp(t); ac.setMode(kPanasonicAc32Cool); }
        irsend.sendPanasonicAC32(ac.getRaw(), kPanasonicAc32Bits, kPanasonicAcDefaultRepeat);
        // kPanasonicAc32SectionGap = 13946µs < kRmtGapThresholdUs (36000µs) → no auto-flush.
        // sendPanasonicAC32() sends 2 sections; each section footer ends with
        // space(kPanasonicAc32SectionGap=13946µs). Neither triggers auto-flush, so both
        // sections accumulate in the buffer and are never transmitted — identical stale-buffer
        // bug to V13 Fujitsu (kFujitsuAcMinGap=8100µs).
        // kPanasonicAc32BitMark and kPanasonicAc32SectionGap are defined in ir_Panasonic.cpp
        // only (not exported) → hardcoded numeric values here.
        irsend.mark(920);    // kPanasonicAc32BitMark = 920µs — closes trailing section gap in receiver rawData
        irsend.space(36001); // >= kRmtGapThresholdUs (36000µs) — flushes both sections to RMT HW
        delay(acSendDelay);
        break;
      }

    // ── 16: Samsung ──────────────────────────────────────────
    case AC_SAMSUNG: {
        IRSamsungAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kSamsungAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kSamsungAcCool); }
        irsend.sendSamsungAC(ac.getRaw(), kSamsungAcStateLength, kSamsungAcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 17: LG standard (GE6711AR2853M model) ────────────────
    case AC_LG: {
        IRLgAc ac(kIrLed);
        ac.setModel(lg_ac_remote_model_t::GE6711AR2853M);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kLgAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kLgAcCool); }
        irsend.sendLG(ac.getRaw(), kLgBits, kLgDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 68: LG2 (AKB75215403 model) ──────────────────────────
    case AC_LG2: {
        IRLgAc ac(kIrLed);
        ac.setModel(lg_ac_remote_model_t::AKB75215403);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kLgAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kLgAcCool); }
        irsend.sendLG2(ac.getRaw(), kLgBits, kLgDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 18: Hitachi Standard 224-bit ─────────────────────────
    case AC_HITACHI_STD: {
        IRHitachiAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kHitachiAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kHitachiAcCool); }
        irsend.sendHitachiAC(ac.getRaw(), kHitachiAcStateLength, kHitachiAcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 19: Hitachi AC1 104-bit ──────────────────────────────
    case AC_HITACHI_AC1: {
        IRHitachiAc1 ac(kIrLed);
        // BUG FIX: IRHitachiAc1::stateReset() initialises _.Mode = kHitachiAc1Auto (0xE1
        // at byte 5, high nibble = 14). IRHitachiAc1::setTemp() has an early return:
        //   if (_.Mode == kHitachiAc1Auto) return;  // Can't change temp in Auto mode.
        // Calling setTemp() BEFORE setMode() means the object is still in Auto at that
        // point → setTemp() silently discards every temperature write → the Temp field
        // stays at the reset-state Auto temperature (kHitachiAc1TempAuto = 25°C) for
        // every ON command regardless of what temperature was requested.
        // Fix: call setMode() first to exit Auto, then setTemp() succeeds.
        if      (power == "ON")  { ac.setPower(true);    ac.setMode(kHitachiAc1Cool); ac.setTemp(t); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setMode(kHitachiAc1Cool); ac.setTemp(t); }
        irsend.sendHitachiAC1(ac.getRaw(), kHitachiAc1StateLength, kHitachiAcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 20: Hitachi AC264 264-bit ─────────────────────────────
    case AC_HITACHI_AC264: {
        IRHitachiAc264 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kHitachiAc424Cool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kHitachiAc424Cool); }
        irsend.sendHitachiAc264(ac.getRaw(), kHitachiAc264StateLength, kHitachiAcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 21: Hitachi AC296 296-bit ─────────────────────────────
    case AC_HITACHI_AC296: {
        IRHitachiAc296 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kHitachiAc296Cool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kHitachiAc296Cool); }
        irsend.sendHitachiAc296(ac.getRaw(), kHitachiAc296StateLength, kHitachiAcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 22: Hitachi AC344 344-bit ─────────────────────────────
    case AC_HITACHI_AC344: {
        IRHitachiAc344 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kHitachiAc344Cool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kHitachiAc344Cool); }
        irsend.sendHitachiAc344(ac.getRaw(), kHitachiAc344StateLength, kHitachiAcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 23: Hitachi AC424 424-bit ─────────────────────────────
    case AC_HITACHI_AC424: {
        IRHitachiAc424 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kHitachiAc424Cool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kHitachiAc424Cool); }
        // BUG FIX (Bug #11): sendHitachiAc424() starts with a leader section whose
        // space is kHitachiAc424LdrSpace = 49,290 µs.  This exceeds kRmtGapThresholdUs
        // (36,000 µs), which would normally trigger _flushBuffer() immediately after
        // the leader — splitting the frame into two separate RMT bursts.  The receiver
        // then sees the ~29ms leader mark as LUTRON, and the 424-bit payload as
        // HITACHI_AC2, never as HITACHI_AC424.
        //
        // Fix: suppress auto-flush for the duration of the send, then call
        // flushTx() explicitly.  kDefaultMessageGap (100,000 µs) is emitted
        // by sendGeneric INSIDE sendHitachiAc424 while the flag is still true,
        // so it cannot auto-flush.  flushTx() dispatches the complete
        // leader + payload in one RMT burst after the flag is restored.
        //
        // This is protocol-specific — kRmtGapThresholdUs is NOT changed.
        irsend.suppressAutoFlush(true);
        irsend.sendHitachiAc424(ac.getRaw(), kHitachiAc424StateLength, kHitachiAcDefaultRepeat);
        irsend.suppressAutoFlush(false);
        // IMPORTANT: kDefaultMessageGap (100,000 µs) is emitted by sendGeneric
        // INSIDE sendHitachiAc424(), while suppressAutoFlush is still true — so
        // the auto-flush that would normally fire for that gap is silently skipped.
        // The complete leader + payload is left sitting in the buffer.
        // flushTx() dispatches it to RMT hardware now.
        irsend.flushTx();
        delay(acSendDelay);
        break;
      }

    // ── 24: Toshiba ──────────────────────────────────────────
    case AC_TOSHIBA: {
        IRToshibaAC ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kToshibaAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kToshibaAcCool); }
        irsend.sendToshibaAC(ac.getRaw(), kToshibaACStateLength, kToshibaACMinRepeat);
        // BUG FIX: sendToshibaAC uses kToshibaAcUsualGap = 7,400 µs as its
        // trailing gap — well below kRmtGapThresholdUs (36,000 µs), so
        // space() never auto-flushes.  Explicit mark+space forces the flush.
        irsend.mark(580);                  // kToshibaAcBitMark = 580 µs — defined in ir_Toshiba.cpp only, not exported in header
        irsend.space(36001);               // > 36,000 µs threshold → _flushBuffer()
        delay(acSendDelay);
        break;
      }

    // ── 25: Haier Standard 72-bit ────────────────────────────
    case AC_HAIER_STD: {
        IRHaierAC ac(kIrLed);
        if      (power == "ON")  { ac.setCommand(kHaierAcCmdOn);  ac.setTemp(t); ac.setMode(kHaierAcCool); }
        else if (power == "OFF") { ac.setCommand(kHaierAcCmdOff); }
        else                     { ac.setCommand(acPower ? kHaierAcCmdOn : kHaierAcCmdOff); ac.setTemp(t); ac.setMode(kHaierAcCool); }
        irsend.sendHaierAC(ac.getRaw(), kHaierACStateLength, kHaierAcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 26: Haier YRW02 112-bit ──────────────────────────────
    case AC_HAIER_YRW02: {
        IRHaierACYRW02 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kHaierAcYrw02Cool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kHaierAcYrw02Cool); }
        irsend.sendHaierACYRW02(ac.getRaw(), kHaierACYRW02StateLength, kHaierAcYrw02DefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 27: Haier AC160 160-bit ──────────────────────────────
    case AC_HAIER_160: {
        IRHaierAC160 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kHaierAcYrw02Cool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kHaierAcYrw02Cool); }
        irsend.sendHaierAC160(ac.getRaw(), kHaierAC160StateLength, kHaierAc160DefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 28: Haier AC176 176-bit ──────────────────────────────
    case AC_HAIER_176: {
        IRHaierAC176 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kHaierAcYrw02Cool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kHaierAcYrw02Cool); }
        irsend.sendHaierAC176(ac.getRaw(), kHaierAC176StateLength, kHaierAc176DefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 29: Kelvinator 128-bit ───────────────────────────────
    case AC_KELVINATOR: {
        IRKelvinatorAC ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kKelvinatorCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kKelvinatorCool); }
        // BUG FIX (Bug #16): sendKelvinator() sends the 128-bit message as 4 sections:
        //   CmdBlock#1 + Footer#1 (gap=kKelvinatorGapSpace=19,975µs)  ← below threshold ✓
        //   DataBlock#1          (gap=kKelvinatorGapSpace×2=39,950µs) ← ABOVE 36,000µs → PREMATURE FLUSH
        //   CmdBlock#2 + Footer#2 (gap=kKelvinatorGapSpace=19,975µs)  ← below threshold ✓
        //   DataBlock#2          (gap=kKelvinatorGapSpace×2=39,950µs) ← ABOVE threshold → end-frame flush
        // The 39,950µs gap after DataBlock#1 triggers auto-flush mid-frame, splitting the
        // 128-bit sequence into two 64-bit RMT bursts. Each half resembles a valid Gree
        // frame (Kelvinator and Gree share the same block structure) → receiver decodes
        // each burst as GREE, never as KELVINATOR.
        //
        // Fix: same suppressAutoFlush pattern as V23 (Bug #11, Hitachi AC424).
        // Suppress auto-flush for the entire send so the 39,950µs intra-frame gap does
        // NOT trigger _flushBuffer(). Then call flushTx() explicitly — the 39,950µs
        // end-of-frame gap also fires while the flag is true (inside sendKelvinator),
        // so it too is suppressed and the complete 128-bit frame is dispatched in one
        // RMT burst by flushTx().
        irsend.suppressAutoFlush(true);
        irsend.sendKelvinator(ac.getRaw(), kKelvinatorStateLength, kKelvinatorDefaultRepeat);
        irsend.suppressAutoFlush(false);
        irsend.flushTx();   // dispatch complete 128-bit frame in one RMT burst
        delay(acSendDelay);
        break;
      }

    // ── 30: Gree 64-bit ──────────────────────────────────────
    case AC_GREE: {
        IRGreeAC ac(kIrLed);
        // BUG FIX (Bug #14): stateReset() initialises Mode=kGreeAuto. setTemp()
        // has a forced override: if (_.Mode == kGreeAuto) safecelsius = 25.
        // setMode(kGreeAuto) also calls setTemp(25) internally.
        // Must call setMode() BEFORE setTemp() so the Auto guard is cleared first.
        if      (power == "ON")  { ac.setPower(true);    ac.setMode(kGreeCool); ac.setTemp(t); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setMode(kGreeCool); ac.setTemp(t); }
        irsend.sendGree(ac.getRaw(), kGreeStateLength, kGreeDefaultRepeat);
        // BUG FIX: Gree is a two-section protocol. sendGree() calls sendGeneric()
        // three times — Block#1 (gap=0), Footer#1 (gap=kGreeMsgSpace=19,980 µs),
        // Block#2 (gap=kGreeMsgSpace=19,980 µs).  All gaps are below
        // kRmtGapThresholdUs (36,000 µs), so space() never auto-flushes.
        // Explicit mark+space forces the flush.
        irsend.mark(620);            // kGreeBitMark = 620 µs — defined in ir_Gree.cpp only, not exported in header
        irsend.space(36001);          // > 36,000 µs threshold → _flushBuffer()
        delay(acSendDelay);
        break;
      }

    // ── 31: Midea 48-bit ─────────────────────────────────────
    case AC_MIDEA: {
        IRMideaAC ac(kIrLed);
        // BUG FIX (Bug #18): Two compounding problems:
        //
        // 1. SETTER ORDER — setMode(kMideaACCool) was called AFTER setTemp().
        //    IRMideaAC::setMode() internally modifies the temperature/Fahrenheit
        //    state, overwriting whatever setTemp() wrote — identical root cause
        //    to Bug #14 (Gree) and Bug #15 (Sharp).
        //
        // 2. FAHRENHEIT FLAG — after setMode() overwrites the temp state,
        //    UseFahrenheit=1 and Temp=0 (Fahrenheit minimum).  setTemp(t)
        //    called without an explicit Celsius flag leaves the Fahrenheit bit
        //    set by whatever state setMode() wrote.  Since 24 and 28 are both
        //    below the Fahrenheit minimum (kMideaACMinTempF=62°F), BOTH get
        //    clamped to 62°F = 17°C — making ON 24°C and ON 28°C produce
        //    identical frames.
        //
        // Fix: call setMode() first, then setTemp(t, true) with useCelsius=true.
        //      stateReset() sets _.useFahrenheit=1. setTemp(t, false) treats t
        //      as Fahrenheit → 24°F < kMideaACMinTempF(62°F) → clamped to 62°F
        //      → 17°C for every requested temperature. Passing true tells the
        //      API t is Celsius, clamps against [17–30°C], and converts correctly.
        if      (power == "ON")  { ac.setPower(true);    ac.setMode(kMideaACCool); ac.setTemp(t, true); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setMode(kMideaACCool); ac.setTemp(t, true); }
        irsend.sendMidea(ac.getRaw(), kMideaBits, kMideaMinRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 32: Midea 24-bit ─────────────────────────────────────
    // NOTE: IRMideaAC::send() always calls sendMidea() (48-bit protocol), not
    // sendMidea24(). Both variants 31 and 32 use the same TX path. The "24-bit"
    // label refers to the AC unit model family, not the on-wire protocol.
    case AC_MIDEA24: {
        IRMideaAC ac(kIrLed);
        // BUG FIX (Bug #18): same setter-order + Fahrenheit-flag issue as V31.
        // See V31 (AC_MIDEA) comment above for full root-cause analysis.
        // Same correction: setTemp(t, true) — useCelsius=true.
        if      (power == "ON")  { ac.setPower(true);    ac.setMode(kMideaACCool); ac.setTemp(t, true); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setMode(kMideaACCool); ac.setTemp(t, true); }
        irsend.sendMidea(ac.getRaw(), kMideaBits, kMideaMinRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 33: Coolix 24-bit ────────────────────────────────────
    case AC_COOLIX: {
        IRCoolixAC ac(kIrLed);
        if      (power == "ON")  { ac.on();  ac.setTemp(t); ac.setMode(kCoolixCool); }
        else if (power == "OFF") { ac.off(); }
        else                     { if (acPower) { ac.on(); ac.setTemp(t); ac.setMode(kCoolixCool); } else { ac.off(); } }
        irsend.sendCOOLIX(ac.getRaw(), kCoolixBits, kCoolixDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 34: Coolix 48-bit ────────────────────────────────────
    // NOTE: IRCoolixAC::send() uses sendCoolix48() only for Fahrenheit ranges.
    // Handle_AC sets Celsius temperatures only, so the Celsius path always
    // fires: sendCOOLIX(getRaw(), kCoolixBits). Same send function as V37.
    case AC_COOLIX48: {
        IRCoolixAC ac(kIrLed);
        if      (power == "ON")  { ac.on();  ac.setTemp(t); ac.setMode(kCoolixCool); }
        else if (power == "OFF") { ac.off(); }
        else                     { if (acPower) { ac.on(); ac.setTemp(t); ac.setMode(kCoolixCool); } else { ac.off(); } }
        irsend.sendCOOLIX(ac.getRaw(), kCoolixBits, kCoolixDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 35: Carrier 64-bit ───────────────────────────────────
    case AC_CARRIER64: {
        IRCarrierAc64 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kCarrierAc64Cool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kCarrierAc64Cool); }
        irsend.sendCarrierAC64(ac.getRaw(), kCarrierAc64Bits, kCarrierAc64MinRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 36: Sharp 104-bit ────────────────────────────────────
    case AC_SHARP: {
        IRSharpAc ac(kIrLed);
        // BUG FIX (Bug #15): stateReset() initialises Mode=kSharpAcAuto. setTemp()
        // guard: case kSharpAcAuto → _.raw[kSharpAcByteTemp]=0; return; (t discarded).
        // getTemp() = _.Temp + kSharpAcMinTemp = 0 + 15 = 15°C always.
        // Must call setMode() BEFORE setTemp() so the Auto guard is cleared first.
        if      (power == "ON")  { ac.setPower(true);    ac.setMode(kSharpAcCool); ac.setTemp(t); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setMode(kSharpAcCool); ac.setTemp(t); }
        irsend.sendSharpAc(ac.getRaw(), kSharpAcStateLength, kSharpAcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 37: Whirlpool 168-bit (toggle) ───────────────────────
    case AC_WHIRLPOOL: {
        IRWhirlpoolAc ac(kIrLed);
        // BUG FIX (Bug #17): toggle-style protocol — same root cause as V7/V8/V15.
        // OFF branch must include setTemp()+setMode() so the frame carries the
        // correct setpoint instead of stateReset() defaults.
        if      (power == "ON")  { ac.setPowerToggle(true); ac.setTemp(t); ac.setMode(kWhirlpoolAcCool); }
        else if (power == "OFF") { ac.setPowerToggle(true); ac.setTemp(t); ac.setMode(kWhirlpoolAcCool); }
        else                     { ac.setTemp(t); ac.setMode(kWhirlpoolAcCool); }
        irsend.sendWhirlpoolAC(ac.getRaw(), kWhirlpoolAcStateLength, kWhirlpoolAcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 38: Electra / AUX 104-bit ────────────────────────────
    case AC_ELECTRA: {
        IRElectraAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kElectraAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kElectraAcCool); }
        irsend.sendElectraAC(ac.getRaw(), kElectraAcStateLength, kElectraAcMinRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 39: Vestel 56-bit ────────────────────────────────────
    case AC_VESTEL: {
        IRVestelAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kVestelAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kVestelAcCool); }
        irsend.sendVestelAc(ac.getRaw(), kVestelAcBits, kNoRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 40 & 41: TCL 112-bit / TCL 96-bit ───────────────────
    //
    // TEKNOPOINT NOTE (decoder-only protocol — no TX variant):
    // ─────────────────────────────────────────────────────────
    // The library has a TEKNOPOINT decoder (decode_type_t=101, ir_Teknopoint.cpp)
    // but NO high-level IRTeknopoint AC class → no TX variant exists for it.
    //
    // Physical Teknopoint hardware (e.g. Allegro SSA-09H / GZ-055B-E1 remote)
    // may decode as TEKNOPOINT or TCL96AC depending on signal timing tolerance.
    //
    // When auto-discovery sees TEKNOPOINT from the decoder, the TX candidate
    // list to try is: [41 (TCL96/GZ055BE1), 40 (TCL112), 1 (Mitsubishi112), 2]
    //
    // Field confirmed: Teknopoint Allegro SSA-09H → variant 1 (Mitsubishi112) works.
    // TCL96 (variant 41) is tried first because the GZ055BE1 model in the library
    // is explicitly listed for Teknopoint hardware in SupportedProtocols.md.
    //
    // See IR_Protocol_Family_Map.md → Family 1 (Mitsubishi) and Family 3 (TCL)
    // for full decoder confusion analysis.
    // ─────────────────────────────────────────────────────────

    // ── 40: TCL 112-bit ──────────────────────────────────────
    case AC_TCL112: {
        IRTcl112Ac ac(kIrLed);
        ac.setModel(tcl_ac_remote_model_t::TAC09CHSD);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kTcl112AcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kTcl112AcCool); }
        irsend.sendTcl112Ac(ac.getRaw(), kTcl112AcStateLength, kTcl112AcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 41: TCL 96-bit ───────────────────────────────────────
    // IRTcl112Ac::send() always calls sendTcl112Ac(getRaw(), kTcl112AcStateLength)
    // regardless of model. GZ055BE1 only affects the _.isTcl flag, not the TX path.
    case AC_TCL96: {
        IRTcl112Ac ac(kIrLed);
        ac.setModel(tcl_ac_remote_model_t::GZ055BE1);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kTcl112AcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kTcl112AcCool); }
        irsend.sendTcl112Ac(ac.getRaw(), kTcl112AcStateLength, kTcl112AcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 42: Teco 35-bit ──────────────────────────────────────
    case AC_TECO: {
        IRTecoAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kTecoCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kTecoCool); }
        irsend.sendTeco(ac.getRaw(), kTecoBits, kTecoDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 43: Goodweather 48-bit ───────────────────────────────
    case AC_GOODWEATHER: {
        IRGoodweatherAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kGoodweatherCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kGoodweatherCool); }
        irsend.sendGoodweather(ac.getRaw(), kGoodweatherBits, kGoodweatherMinRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 44: Neoclima 96-bit ──────────────────────────────────
    case AC_NEOCLIMA: {
        IRNeoclimaAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kNeoclimaCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kNeoclimaCool); }
        irsend.sendNeoclima(ac.getRaw(), kNeoclimaStateLength, kNeoclimaMinRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 45: Amcor 64-bit ─────────────────────────────────────
    case AC_AMCOR: {
        IRAmcorAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kAmcorCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kAmcorCool); }
        irsend.sendAmcor(ac.getRaw(), kAmcorStateLength, kAmcorDefaultRepeat);
        // kAmcorGap = 34300µs < kRmtGapThresholdUs (36000µs) → no auto-flush.
        // kAmcorDefaultRepeat = kSingleRepeat = 1, so sendGeneric() loops twice
        // (r=0 and r=1), each ending with space(kAmcorGap=34300µs). Neither
        // triggers auto-flush, so BOTH repeat frames accumulate in the buffer and
        // are never transmitted until the next protocol's sendXxx() call fires the
        // next flush — corrupting both decode results.
        // Constants kAmcorFooterMark and kAmcorGap are defined in ir_Amcor.cpp
        // only (not exported) → hardcoded numeric values here.
        irsend.mark(1900);   // kAmcorFooterMark = 1900µs — closes trailing gap in receiver rawData
        irsend.space(36001); // >= kRmtGapThresholdUs (36000µs) — flushes both repeat frames to RMT HW
        delay(acSendDelay);
        break;
      }

    // ── 46: Airwell 34-bit (toggle) ──────────────────────────
    case AC_AIRWELL: {
        IRAirwellAc ac(kIrLed);
        // BUG FIX (Bug #17): toggle-style protocol — same root cause as V7/V8/V15.
        // OFF branch must include setTemp()+setMode() so the frame carries the
        // correct setpoint instead of stateReset() defaults.
        if      (power == "ON")  { ac.setPowerToggle(true); ac.setTemp(t); ac.setMode(kAirwellCool); }
        else if (power == "OFF") { ac.setPowerToggle(true); ac.setTemp(t); ac.setMode(kAirwellCool); }
        else                     { ac.setTemp(t); ac.setMode(kAirwellCool); }
        irsend.sendAirwell(ac.getRaw(), kAirwellBits, kAirwellMinRepeats);
        delay(acSendDelay);
        break;
      }

    // ── 47: Delonghi 64-bit ──────────────────────────────────
    case AC_DELONGHI: {
        IRDelonghiAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kDelonghiAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kDelonghiAcCool); }
        irsend.sendDelonghiAc(ac.getRaw(), kDelonghiAcBits, kDelonghiAcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 48 & 49: Sanyo Standard 72-bit / Sanyo88 88-bit ─────
    //
    // SANYO_AC152 NOTE (decoder-only protocol — no TX variant):
    // ─────────────────────────────────────────────────────────
    // The library has a SANYO_AC152 decoder (ir_Sanyo.cpp) covering:
    //   Sanyo RCS-4MHVPIS4EE remote, SAP-KMRV124EHE A/C unit
    //
    // But there is NO IRSanyoAc152 TX class in the library → no TX variant.
    //
    // When auto-discovery sees SANYO_AC152 from the decoder, the best-effort
    // TX candidate list is: [48 (Sanyo72), 49 (Sanyo88)]
    // These are different bit-lengths so they may NOT work on a 152-bit unit.
    // If both fail → fall through to raw capture/replay (see TODO block below).
    //
    // Future: if IRSanyoAc152 TX class is added in a future library version,
    // add a new variant here (e.g. AC_SANYO152 = 69) mirroring the pattern
    // of cases 48 and 49 below.
    //
    // See IR_Protocol_Family_Map.md → Family 24 (Sanyo) for full details.
    // ─────────────────────────────────────────────────────────

    // ── 48: Sanyo Standard 72-bit ────────────────────────────
    case AC_SANYO_STD: {
        IRSanyoAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kSanyoAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kSanyoAcCool); }
        irsend.sendSanyoAc(ac.getRaw(), kSanyoAcStateLength, kNoRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 49: Sanyo AC88 88-bit ────────────────────────────────
    case AC_SANYO88: {
        IRSanyoAc88 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kSanyoAc88Cool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kSanyoAc88Cool); }
        irsend.sendSanyoAc88(ac.getRaw(), kSanyoAc88StateLength, kSanyoAc88MinRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 50: Voltas 120-bit ───────────────────────────────────
    case AC_VOLTAS: {
        IRVoltas ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kVoltasCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kVoltasCool); }
        irsend.sendVoltas(ac.getRaw(), kVoltasStateLength, kNoRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 51: Mirage 120-bit ───────────────────────────────────
    case AC_MIRAGE: {
        IRMirageAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kMirageAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kMirageAcCool); }
        irsend.sendMirage(ac.getRaw(), kMirageStateLength, kMirageMinRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 52: Corona 168-bit ───────────────────────────────────
    case AC_CORONA: {
        IRCoronaAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kCoronaAcModeCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kCoronaAcModeCool); }
        irsend.sendCoronaAc(ac.getRaw(), kCoronaAcStateLength, kNoRepeat);
        // BUG FIX (Bug #19): kCoronaAcSpaceGap = 10,800µs < kRmtGapThresholdUs (36,000µs).
        // sendCoronaAc() calls sendGeneric 3× (kCoronaAcSections = 3), each ending with
        // kCoronaAcSpaceGap — no section gap reaches threshold → full 3-section frame never
        // auto-flushes. Identical stale-buffer bug to V13 Fujitsu, V15 Panasonic AC32.
        // kCoronaAcBitMark=450 and kCoronaAcSpaceGap=10800 defined in ir_Corona.cpp only
        // (not exported in header) → hardcoded here.
        irsend.mark(450);    // kCoronaAcBitMark = 450µs — closes trailing gap in receiver rawData
        irsend.space(36001); // >= kRmtGapThresholdUs (36,000µs) — flushes 3-section frame to RMT HW
        delay(acSendDelay);
        break;
      }

    // ── 53: Airton 56-bit ────────────────────────────────────
    case AC_AIRTON: {
        IRAirtonAc ac(kIrLed);
        // BUG FIX (Bug #20): stateReset() initialises Mode=kAirtonAuto.
        // setPower(true) internally calls setMode(getMode())=setMode(kAirtonAuto) which calls
        // setTemp(25); but setTemp's Auto guard forces temp=kAirtonMaxTemp=31C.
        // Then setTemp(t) runs while Mode is still Auto → guard forces 31C regardless of t.
        // Then setMode(kAirtonCool) changes mode too late — Temp field already wrong.
        // Fix: setMode(kAirtonCool) before setTemp(t) — same pattern as Bug #10/14/15.
        // Note: kDefaultMessageGap (100,000µs) ends each send → auto-flush ✓ no mark+space needed.
        if      (power == "ON")  { ac.setPower(true);    ac.setMode(kAirtonCool); ac.setTemp(t); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setMode(kAirtonCool); ac.setTemp(t); }
        irsend.sendAirton(ac.getRaw(), kAirtonBits, kAirtonDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 54: EcoClim 56-bit ───────────────────────────────────
    case AC_ECOCLIM: {
        IREcoclimAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kEcoclimCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kEcoclimCool); }
        irsend.sendEcoclim(ac.getRaw(), kEcoclimBits, kNoRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 55: Kelon 48-bit ──────────────────────────────────────
    case AC_KELON: {
        IRKelonAc ac(kIrLed);
        // BUG FIX (Bug #21): ensurePower() calls ac.send() internally via ac._irsend (standard
        // IRsend). These internal sends do NOT transmit via IRsendRMT — only the external
        // irsend.sendKelon() call actually transmits.
        // For OFF: ensurePower(false) does 3 internal sends. The 3rd sets PowerToggle=true then
        // immediately resets it to false (send() always clears toggle flags after transmitting).
        // Leftover state = stateReset defaults (Mode=Heat=0, Temp=18°C, PowerToggle=false) →
        // RX decodes as KELON 0x683, Temp:18C, Mode:Heat regardless of last-used temperature.
        // Fix: bypass ensurePower() entirely. For OFF, call setTogglePower(true) directly so the
        // external irsend.sendKelon() transmits the toggle bit. For ON, plain setMode+setTemp.
        // Note: kKelonGap = 2×kDefaultMessageGap = 200,000µs > threshold → auto-flushes ✓
        if      (power == "ON")  { ac.setMode(kKelonModeCool); ac.setTemp(t); }
        else if (power == "OFF") { ac.setMode(kKelonModeCool); ac.setTemp(t); ac.setTogglePower(true); }
        else                     { ac.setMode(kKelonModeCool); ac.setTemp(t); }
        irsend.sendKelon(ac.getRaw(), kKelonBits, kNoRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 56: Kelon 168-bit ────────────────────────────────────
    case AC_KELON168: {
        IRKelon168Ac ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kKelon168ModeCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kKelon168ModeCool); }
        irsend.sendKelon168(ac.getRaw(), kKelon168StateLength, kKelon168DefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 57: Rhoss 96-bit ─────────────────────────────────────
    case AC_RHOSS: {
        IRRhossAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kRhossModeCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kRhossModeCool); }
        irsend.sendRhoss(ac.getRaw(), kRhossStateLength, kRhossDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 58: Technibel 56-bit ─────────────────────────────────
    case AC_TECHNIBEL: {
        IRTechnibelAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kTechnibelAcCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kTechnibelAcCool); }
        irsend.sendTechnibelAc(ac.getRaw(), kTechnibelAcBits, kTechnibelAcDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 59: Transcold 136-bit ────────────────────────────────
    case AC_TRANSCOLD: {
        IRTranscoldAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kTranscoldCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kTranscoldCool); }
        irsend.sendTranscold(ac.getRaw(), kTranscoldBits, kTranscoldDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 60: Trotec Standard 56-bit ───────────────────────────
    case AC_TROTEC_STD: {
        IRTrotecESP ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kTrotecCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kTrotecCool); }
        irsend.sendTrotec(ac.getRaw(), kTrotecStateLength, kTrotecDefaultRepeat);
        // Two problems with Trotec on the RMT path:
        //
        // 1. FLUSH: kTrotecGap (6184µs) and kTrotecHdrSpace (7364µs) are both
        //    below kRmtGapThresholdUs (20000µs), so _flushBuffer() is never
        //    triggered automatically by space().
        //
        // 2. RECEIVER ISR GAP: The IrTrace receiver ISR records a space only
        //    when the NEXT mark begins (rising edge triggers the ISR that closes
        //    the space timer). kTrotecGapEnd (1500µs) is the last space in the
        //    frame; with no following mark the ISR never fires and rawData ends
        //    at 149 entries (rawlen=150). decodeTrotec() requires rawlen > 150
        //    and fails its minimum-length check at that value.
        //
        // Fix for both: append one extra kTrotecBitMark + 36ms gap after
        // sendTrotec().  The mark provides the rising-edge ISR trigger that
        // closes the kTrotecGapEnd space in rawData (rawData[149] ≈ 1524µs,
        // matchAtLeast(1524, 1500) passes).  space(36001) >= kRmtGapThresholdUs
        // triggers the auto-flush, transmitting the complete frame + closure in
        // one RMT burst. The Trotec decoder returns true before examining the
        // extra mark entry.
        // Note: kRmtGapThresholdUs was raised to 36000 µs (was 20000 µs) to
        // keep all Daikin inter-section gaps buffered. Trotec explicit flush
        // value updated from 20001 to 36001 accordingly.
        irsend.mark(592);    // kTrotecBitMark — not exported from ir_Trotec.cpp
        irsend.space(36001); // >= kRmtGapThresholdUs (36000µs) — auto-flushes buffer
        delay(acSendDelay);
        break;
      }

    // ── 61: Trotec 3550 120-bit ──────────────────────────────
    case AC_TROTEC3550: {
        IRTrotec3550 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kTrotecCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kTrotecCool); }
        irsend.sendTrotec3550(ac.getRaw(), kTrotecStateLength, kTrotecDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 62: Truma (variable-bit) ─────────────────────────────
    case AC_TRUMA: {
        IRTrumaAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kTrumaCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kTrumaCool); }
        irsend.sendTruma(ac.getRaw(), kTrumaBits, kNoRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 63: Bosch 144-bit ────────────────────────────────────
    case AC_BOSCH144: {
        // BUG FIX (Bug #22): Bosch144 OFF uses a completely separate hardcoded 96-bit payload
        // (kBosch144Off[12]) — NOT a flag within _.raw. setPower(false) only sets powerFlag;
        // getRaw() ALWAYS returns _.raw regardless. The payload switch (if !powerFlag → send
        // kBosch144Off) lives only in ac.send() which calls ac._irsend internally. Since
        // ac._irsend doesn't transmit via IRsendRMT, irsend.sendBosch144(ac.getRaw(),
        // kBosch144StateLength) for OFF transmitted the stateReset defaults (Mode=Auto,
        // Temp=25°C), decoded by RX as "Power:On, Mode:Auto, Temp:25C" — no power-off sent.
        // Fix: pass kBosch144Off directly for OFF. sizeof(kBosch144Off)=12 bytes (2 sections),
        // vs kBosch144StateLength=18 bytes (3 sections) for ON. Both are valid multiples of
        // kBosch144BytesPerSection(6), so sendBosch144() length-check passes ✓
        bool wantOff = (power == "OFF") || (power != "ON" && !acPower);
        if (wantOff) {
            irsend.sendBosch144(kBosch144Off, sizeof(kBosch144Off), kNoRepeat);
        } else {
            IRBosch144AC ac(kIrLed);
            ac.setTemp(t); ac.setMode(kBosch144Cool);
            irsend.sendBosch144(ac.getRaw(), kBosch144StateLength, kNoRepeat);
        }
        delay(acSendDelay);
        break;
      }

    // ── 64: Argo WREM-2 96-bit ───────────────────────────────
    // BUG FIX (Bug #23): sendArgo() defaults sendFooter=false → trailing gap=0µs
    // → RMT buffer never auto-flushes (threshold=36,000µs) → stale-buffer bug.
    // Fix: pass sendFooter=true as 4th arg → uses kArgoGap=kDefaultMessageGap
    // (100,000µs) as trailer → above threshold → frame flushes immediately.
    case AC_ARGO_WREM2: {
        IRArgoAC ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kArgoCool); }
        else if (power == "OFF") { ac.setPower(false);   ac.setTemp(t); ac.setMode(kArgoCool); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kArgoCool); }
        irsend.sendArgo(ac.getRaw(), kArgoStateLength, kArgoDefaultRepeat, true);
        delay(acSendDelay);
        break;
      }

    // ── 65: Argo WREM-3 (variable-bit) ───────────────────────
    // BUG FIX (Bug #24): static_cast<argoMode_t>(kArgoCool) casts value 0, but
    // argoMode_t enum has no member 0 (COOL=1, AUTO=5) → setMode() hits default
    // case → _.Mode = AUTO (5). Fix: use argoMode_t::COOL directly.
    case AC_ARGO_WREM3: {
        IRArgoAC_WREM3 ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(argoMode_t::COOL); }
        else if (power == "OFF") { ac.setPower(false);   ac.setTemp(t); ac.setMode(argoMode_t::COOL); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(argoMode_t::COOL); }
        irsend.sendArgoWREM3(ac.getRaw(), ac.getRawByteLength(), kArgoDefaultRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 66: Eurom 96-bit ─────────────────────────────────────
    case AC_EUROM: {
        IREuromAc ac(kIrLed);
        if      (power == "ON")  { ac.setPower(true);    ac.setTemp(t); ac.setMode(kEuromCool); }
        else if (power == "OFF") { ac.setPower(false); }
        else                     { ac.setPower(acPower); ac.setTemp(t); ac.setMode(kEuromCool); }
        irsend.sendEurom(ac.getRaw(), kEuromStateLength, kNoRepeat);
        delay(acSendDelay);
        break;
      }

    // ── 67: York 88-bit ──────────────────────────────────────
    // setPowerToggle() not implemented in library v2.9.0 — work around via getRaw()
    case AC_YORK: {
        IRYorkAc ac(kIrLed);
        uint8_t* raw = ac.getRaw();
        if      (power == "ON")  { raw[7] |=  0x10; ac.setTemp(t); ac.setMode(kYorkCool); }
        else if (power == "OFF") { raw[7] &= ~0x10; }
        else                     { if (acPower) raw[7] |= 0x10; else raw[7] &= ~0x10; ac.setTemp(t); ac.setMode(kYorkCool); }
        ac.calcChecksum();
        irsend.sendYork(ac.getRaw(), kYorkStateLength, kNoRepeat);
        delay(acSendDelay);
        break;
      }

    default:
      // ── Future: Raw Capture/Replay ────────────────────────────
      // Protocols that reach here have NO high-level AC class in
      // the library (DAIKIN200, DAIKIN312, BLUESTARHEAVY, etc.).
      // Instead of generating commands dynamically, the device
      // captures exact bytes from the real remote and replays them.
      //
      // When implemented, this case will call Handle_Raw_Replay():
      //   Handle_Raw_Replay(power);  // reads NVS → sendXxx(buf, len)
      //
      // See the "RAW CAPTURE / REPLAY — FUTURE IMPLEMENTATION"
      // section at the bottom of this file for full design notes.
      // ─────────────────────────────────────────────────────────
      DBG_printf("[ERROR] Unknown AC_VARIANT: %d — no TX class for this protocol.\n"
                 "        If this is DAIKIN200/DAIKIN312/BLUESTARHEAVY, see raw-replay TODO.\n",
                 acVariant);
      return;
  }

  // GPIO5 is owned by IRsendRMT for the lifetime of irsend.
  // RMT eot_level=0 drives the pin LOW after every transmission automatically.
  // No pinMode/digitalWrite reclaim needed — doing so would disconnect RMT
  // from GPIO5 and kill all subsequent transmissions.

  led_ir_burst();
  DBG_printf("[IR] Sent OK  Variant=%d  Power=%s  Temp=%d\n",
             acVariant, power.c_str(), t);
}


// ================================================================
// RAW CAPTURE / REPLAY — FUTURE IMPLEMENTATION
// ================================================================
//
// PURPOSE
// -------
// Some AC protocols have a decoder in the library but NO high-level
// IRXxxAc class and are NOT in the IRac unified interface.
// This means we cannot call setTemp() / setMode() / setPower() on them.
//
// Affected protocols (as of IRremoteESP8266 v2.9.0):
//
//   Protocol       decode_type_t   Library send function
//   ----------     -------------   ---------------------
//   DAIKIN200      113             irsend.sendDaikin200(buf, len, repeat)
//   DAIKIN312      121             irsend.sendDaikin312(buf, len, repeat)
//   BLUESTARHEAVY  126             irsend.sendBluestarHeavy(buf, len, repeat)
//
// These are NOT in the existing 65 TX variants (AC_MITSU_STD ... AC_YORK).
// If the auto-discovery system exhausts all candidate TX variants and the
// AC still does not respond, it falls back to this raw capture approach.
//
//
// HOW IT WORKS
// ------------
// The library's decoder always fills results.state[] with the decoded
// byte array, regardless of whether a high-level class exists.
// We store those bytes in NVS (non-volatile flash) and replay them later.
//
// CAPTURE (during installer configuration):
//   1. Installer presses ON button on physical remote.
//   2. Receiver decodes signal → results.state[] contains exact bytes.
//   3. Device saves bytes to NVS under key "raw_on"  + stores byte count
//      and protocol ID (decode_type_t) so we know which sendXxx() to call.
//   4. Installer presses OFF button on physical remote.
//   5. Device saves bytes to NVS under key "raw_off".
//
// REPLAY (during normal operation):
//   Read stored bytes from NVS → call the matching library send function.
//
//
// CRITICAL LIMITATION — FIXED TEMPERATURE
// ----------------------------------------
// Raw replay sends the EXACT bytes captured from the remote.
// The temperature is encoded inside those bytes and CANNOT be changed
// without knowing the protocol's byte format.
//
//   ✅ Dynamic TX (65 variants):  app sets temp=22°C → works
//   ❌ Raw replay:                captured at 24°C → always sends 24°C
//
// The installer must set the desired temperature on the physical remote
// BEFORE pressing the ON button during capture. That temperature is
// then fixed for this device forever (unless recaptured).
//
//
// NVS KEY DESIGN (suggested)
// ---------------------------
//   "raw_protocol"   uint8_t   — decode_type_t value (e.g. 113 = DAIKIN200)
//   "raw_len"        uint8_t   — byte count (e.g. kDaikin200StateLength = 25)
//   "raw_on"         blob      — byte array for ON command
//   "raw_off"        blob      — byte array for OFF command
//
//
// CODE SKELETON — Handle_Raw_Replay()
// -------------------------------------
//
// void Handle_Raw_Replay(const String& power) {
//
//     uint8_t protocol = nvs_get_u8("raw_protocol", 0);
//     uint8_t len      = nvs_get_u8("raw_len",      0);
//     uint8_t buf[32]  = {0};  // max raw state length across these 3 protocols
//
//     if (len == 0 || protocol == 0) {
//         DBG_printf("[RAW] No raw command stored. Run capture first.\n");
//         return;
//     }
//
//     const char* key = (power == "OFF") ? "raw_off" : "raw_on";
//     nvs_get_blob(key, buf, len);
//
//     DBG_printf("[RAW] Replaying protocol=%d  len=%d  cmd=%s\n",
//                protocol, len, key);
//
//     switch (protocol) {
//         case DAIKIN200:
//             irsend.sendDaikin200(buf, len);
//             break;
//         case DAIKIN312:
//             irsend.sendDaikin312(buf, len);
//             break;
//         case BLUESTARHEAVY:
//             irsend.sendBluestarHeavy(buf, len);
//             break;
//         default:
//             DBG_printf("[RAW] Unknown protocol %d — cannot replay.\n", protocol);
//             return;
//     }
//
//     // Reclaim GPIO after RMT send (same as Handle_AC)
//     pinMode(kIrLed, OUTPUT);
//     digitalWrite(kIrLed, LOW);
//     led_ir_burst();
// }
//
//
// INTEGRATION POINTS
// ------------------
// 1. Receiver code (auto-discovery): after all TX candidates fail,
//    trigger the capture flow → store bytes → set acVariant = RAW_REPLAY_SLOT.
//
// 2. Handle_AC() default: case should call Handle_Raw_Replay(power)
//    when acVariant == RAW_REPLAY_SLOT (define a constant like 255).
//
// 3. Config portal: show "Temperature fixed at capture value" warning
//    when the locked protocol is in the raw-replay category.
//
// ================================================================
