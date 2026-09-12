// Pomona firmware — application configuration (Trello #229/#251; v2 wire #295).
//
// Pins, ADC, I2C addresses, timing, WiFi/MQTT connection settings and
// topics for the pomona sketch. Calibration constants live in
// <PomonaCalibration.h> (shared with bringup); ONLY the two passwords
// (WIFI_PASS, MQTT_PASS) live in secrets.h (gitignored — copy
// firmware/secrets.h.example here and fill it in).

#pragma once

#include <Arduino.h>

// ---- pins (docs/wiring.md pin map) -----------------------------------
const int PIN_TDS = A2;    // Grove TDS, powered from 3V3 (perm wiring 2026-09-10; bench: A0)
const int PIN_PH = A1;     // SEN0169-V2 via DFR0504 isolator
const int PIN_ONEWIRE = 1; // DS18B20 data via Rnaenaor T2 (perm wiring 2026-09-09; bench: D2). Pull-up on the board. NOTE: D1 belongs to a hardware UART — that port is now off-limits (A02YYUW level sensor must use another Serial).
const int PIN_PROBE = 3;   // CQRSENYW003 green wire (open collector; probe REMOVED 2026-09-03, pin kept reserved)
// DFR0523 dosing pumps (#284-287): PPM signal per channel, back on the
// bench-proven D4-D7 block (owner + meter, 2026-09-09). The perm-wiring
// move to D10-D13 failed: on the GIGA only D10 of that block produces
// mbed PwmOut frames — D11/D12/D13 sit on STM32 port pins with no timer
// route and idle at ~3.3 V. NEVER put a servo/PPM signal on D11-D13.
const int PIN_DOSE_CH1 = 6; // ch1 pH-Down (BPT tube) — as-landed by owner 2026-09-09
const int PIN_DOSE_CH2 = 4; // ch2 Nutrient A
const int PIN_DOSE_CH3 = 5; // ch3 Nutrient B
const int PIN_DOSE_CH4 = 7; // ch4 spare, configured but idle (#287)

// ---- ADC -------------------------------------------------------------
const float VREF = 3.3f;
const int ADC_BITS = 12;
const float ADC_MAX = 4095.0f;

// ---- I2C addresses ---------------------------------------------------
const uint8_t ADDR_BME280 = 0x76; // strapped boards; unstrapped default is 0x77
const uint8_t ADDR_BME280_ALT = 0x77; // OK since the Grove level strip (0x77/0x78) is not used
const uint8_t ADDR_BH1750 = 0x23;

// ---- timing ----------------------------------------------------------
const uint32_t SENSOR_READ_MS = 5000;       // sensor sweep + screen refresh
const uint32_t MQTT_PUBLISH_MS = 30000;     // publish cadence (docs/mqtt.md)
const uint32_t NET_RETRY_MIN_MS = 5000;     // reconnect backoff, doubles...
const uint32_t NET_RETRY_MAX_MS = 60000;    // ...up to this cap
const int WIFI_BEGIN_ATTEMPTS = 3;          // WiFi.begin tries per connect
const uint32_t SENSOR_REINIT_MS = 60000;    // absent-sensor re-probe cadence
const uint32_t WATCHDOG_TIMEOUT_MS = 30000; // hardware IWDG (max ~32 s)
const uint32_t DISPLAY_BLANK_TIMEOUT_MS = 60000; // idle -> backlight off (#248)
const int OTA_DOWNLOAD_ATTEMPTS = 3; // download+size-verify tries (see ota.cpp)
const uint32_t BOOT_SCREEN_HOLD_MS = 2500; // final boot state stays readable this long
const uint32_t BOOT_NET_WAIT_MS = 30000; // max boot-screen wait for WiFi+MQTT
const int OTA_EST_TOTAL_S = 240; // whole-OTA estimate driving the on-screen countdown

// ---- control: pump duty cycle + photoperiod (#260) --------------------
// The decision lives on the unit (src/control), HA only relays it to the
// plugs. Rationale and the two-mode table: docs/control-architecture.md and
// the home-assitant repo's pomona-schedule.md.
const uint32_t PUMP_ON_MS = 15UL * 60UL * 1000UL;             // 15 min run
const uint32_t PUMP_OFF_ESTABLISHMENT_MS = 15UL * 60UL * 1000UL;  // 15/15
const uint32_t PUMP_OFF_DAY_MS = 45UL * 60UL * 1000UL;        // 15 min per hour
const uint32_t PUMP_OFF_NIGHT_MS = 105UL * 60UL * 1000UL;     // 15 min per 2 h

// Photoperiod, local hours. Establishment starts later (gentler 12 h day for
// seedlings straight off a propagation tray); established runs 14 h.
const int LIGHT_ON_ESTABLISHMENT_H = 8;
const int LIGHT_ON_ESTABLISHED_H = 6;
const int LIGHT_OFF_H = 20;
// Fixed offset: an hour of DST error is irrelevant to a 14 h photoperiod and
// not worth carrying EU DST rules in firmware for. 60 = CET, 120 = CEST.
const int TZ_OFFSET_MINUTES = 120;

// Level interlock (probe is a TOP-UP gauge, blind below 8.2 L — see
// docs/sensors/level-probe.md). A raw 0 during a pump cycle is not actionable:
// the tower holds water in transit, so stop, let it drain back, then believe
// the reading.
const uint32_t LEVEL_LOW_DEBOUNCE_MS = 2UL * 60UL * 1000UL;   // raw 0 this long -> check
const uint32_t SETTLE_WAIT_MS = 5UL * 60UL * 1000UL;          // pump off, drain back
const uint32_t SETTLE_MIN_INTERVAL_MS = 60UL * 60UL * 1000UL; // at most hourly
const uint32_t LEVEL_INHIBIT_AFTER_MS = 24UL * 60UL * 60UL * 1000UL; // confirmed low this long -> stop

// NTP: the photoperiod needs wall-clock time and this is the only source. The
// PUMP never consults it — that path is millis() only and works with no
// network at all.
#define NTP_HOST "pool.ntp.org"
const uint32_t NTP_RESYNC_MS = 6UL * 60UL * 60UL * 1000UL; // re-sync every 6 h
const uint32_t NTP_TIMEOUT_MS = 1200; // short: the watchdog is 30 s, do not stall the loop

// ---- WiFi / MQTT connection (non-secret — passwords in secrets.h) ----
#define WIFI_SSID "B3ns-2-4"
#define MQTT_HOST "mqtt.lab.local" // in-cluster EMQX (docs/mqtt.md)
#define MQTT_PORT 1883

// ---- THE UNIT on the wire — demeter/<unit_id>/… (demeter ADR-0008/0009, #295)
// Firmware 2.0.0 speaks contract v2: the tree is Demeter's, keyed by the
// unit id (<name>-NNNN); below the unit, `sys/` is the system layer (what
// the node says about itself + what Demeter concludes) and everything else
// is process data (tele/, actuator/, dose/, desired). Full schema and
// payloads: docs/mqtt.md (points at the demeter repo's ADR-0008).
#define MQTT_UNIT_ID "pomona-0001"
#define MQTT_USER "unit-" MQTT_UNIT_ID     // the node's own least-privilege broker user
const char MQTT_CLIENT_ID[] = "unit-" MQTT_UNIT_ID;
#define MQTT_BASE "demeter/" MQTT_UNIT_ID
#define UNIT_TYPE "aeroponic_tower"        // selects Demeter's profile
#define UNIT_NODE "giga-r1"                // the board, informational
#define MQTT_CONTRACT 2
const float UNIT_RESERVOIR_L = 10.0f;      // announced in sys/meta; the config document is authoritative

// -- the system layer the NODE publishes
const char TOPIC_SYS_STATUS[] = MQTT_BASE "/sys/status";   // retained + LWT: online|offline
const char TOPIC_SYS_META[] = MQTT_BASE "/sys/meta";       // retained JSON self-description (on connect)
const char TOPIC_SYS_HEALTH[] = MQTT_BASE "/sys/health";   // retained JSON availability map (every publish cycle)
const char TOPIC_SYS_DIAG_I2C[] = MQTT_BASE "/sys/diag/i2c_scan";         // retained JSON (diagnostics)
const char TOPIC_SYS_DIAG_I2C_GET[] = MQTT_BASE "/sys/diag/i2c_scan/get"; // any msg -> scan now (subscribed)
// OTA (basic slice of #243): publish an http(s) .ota URL NON-retained to
// sys/ota/url; the unit stages it in QSPI and reboots to apply.
const char TOPIC_SYS_OTA_URL[] = MQTT_BASE "/sys/ota/url";       // subscribed
const char TOPIC_SYS_OTA_RESULT[] = MQTT_BASE "/sys/ota/result"; // retained
// -- telemetry: plain numbers, one per topic, NON-retained, only when the sensor answered
const char TOPIC_NODE_RSSI[] = MQTT_BASE "/tele/node/rssi_dbm";
const char TOPIC_NODE_UPTIME[] = MQTT_BASE "/tele/node/uptime_s";
const char TOPIC_WATER_TEMP[] = MQTT_BASE "/tele/water/temp_c";
const char TOPIC_WATER_EC[] = MQTT_BASE "/tele/water/ec_ms_cm";
const char TOPIC_WATER_PH[] = MQTT_BASE "/tele/water/ph";
const char TOPIC_WATER_PH_RAW[] = MQTT_BASE "/tele/water/ph_raw_v"; // always published (calibration/drift)
const char TOPIC_WATER_LEVEL_PCT[] = MQTT_BASE "/tele/water/level_pct";
const char TOPIC_WATER_LEVEL_POINTS[] = MQTT_BASE "/tele/water/level_points";
const char TOPIC_AIR_TEMP[] = MQTT_BASE "/tele/air/temp_c";
const char TOPIC_AIR_RH[] = MQTT_BASE "/tele/air/humidity_pct";
const char TOPIC_AIR_PRESSURE[] = MQTT_BASE "/tele/air/pressure_hpa";
const char TOPIC_AIR_LUX[] = MQTT_BASE "/tele/air/lux";
// -- actuators (#260, docs/control-architecture.md): the unit publishes what
// it DECIDED (state + reason, retained — a request is the current desired
// state, so a broker replay on an HA restart is exactly right) and accepts a
// REQUEST (set: auto|on|off, never retained) that its interlock may veto.
const char TOPIC_PUMP_STATE[] = MQTT_BASE "/actuator/pump/state";     // retained on|off
const char TOPIC_PUMP_REASON[] = MQTT_BASE "/actuator/pump/reason";   // retained
const char TOPIC_PUMP_SET[] = MQTT_BASE "/actuator/pump/set";         // subscribed auto|on|off
const char TOPIC_LIGHT_STATE[] = MQTT_BASE "/actuator/light/state";   // retained on|off
const char TOPIC_LIGHT_SET[] = MQTT_BASE "/actuator/light/set";       // subscribed auto|on|off (new in 2.0.0)
// -- desired state (Demeter's registry, retained JSON): the node applies what
// it supports — today `stage` (establishment|established) — and ignores the rest.
const char TOPIC_DESIRED[] = MQTT_BASE "/desired";                    // subscribed
// -- dosing (#224 contract, demeter ADR-0008 rule 4): an ml-based request with
// an id, NEVER retained; the node converts ml with ITS OWN calibration
// (PomonaCalibration.h), enforces its own caps below and acks every request on
// dose/result (done|refused|failed). A result without an id is a bench dose.
const char TOPIC_DOSE_REQUEST[] = MQTT_BASE "/dose/request";  // subscribed JSON {id, reagent, ml, rate, ts}
const char TOPIC_DOSE_RESULT[] = MQTT_BASE "/dose/result";    // retained JSON, last action
// THE NODE'S OWN RAILS — the unit stays safe against a misbehaving controller.
// Per command: pH-Down 0.2 ml/L * 10 L; nutrients one feed of 10 ml. Per
// rolling 24 h: twice Demeter's derived acid cap (0.4 ml/L), four feeds of A/B.
// Channel 4 is idle (#287): 0 = every request refused.
const float DOSE_MAX_ML_PER_CMD[4] = {2.0f, 10.0f, 10.0f, 0.0f};
const float DOSE_MAX_ML_PER_24H[4] = {8.0f, 40.0f, 40.0f, 0.0f};
const unsigned long DOSE_MAX_MS = 60000;      // absolute cap per run
const unsigned long DOSE_TEST_MAX_MS = 10000; // hard cap per bench command (Serial "dose chN …")

// ---- display band coloring (owner 2026-09-02) -------------------------
// Tile values render green inside the target band, amber inside tolerance,
// red beyond. Bands MUST match Tethys (.claude/agents/tethys.md, gitops) and
// the Grafana dashboard. EC is the TRANSPLANT band (0.8-1.0); when
// establishment ends (~2026-09-14, card #262) ramp these toward 1.4-1.6.
const float BAND_PH_G_LO = 5.8f,  BAND_PH_G_HI = 6.2f;
const float BAND_PH_A_LO = 5.4f,  BAND_PH_A_HI = 6.5f;
const float BAND_EC_G_LO = 0.8f,  BAND_EC_G_HI = 1.0f;
const float BAND_EC_A_LO = 0.7f,  BAND_EC_A_HI = 1.1f;
const float BAND_WTEMP_G_LO = 18.0f, BAND_WTEMP_G_HI = 24.0f;
const float BAND_WTEMP_A_LO = 15.0f, BAND_WTEMP_A_HI = 26.5f;
const float BAND_ATEMP_G_LO = 18.0f, BAND_ATEMP_G_HI = 28.0f;
const float BAND_ATEMP_A_LO = 12.0f, BAND_ATEMP_A_HI = 32.0f;
const float BAND_RH_G_LO = 40.0f, BAND_RH_G_HI = 70.0f;
const float BAND_RH_A_LO = 30.0f, BAND_RH_A_HI = 80.0f;
