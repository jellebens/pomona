// Pomona firmware v1 — application configuration (Trello #229/#251).
//
// Pins, ADC, I2C addresses, timing, WiFi/MQTT connection settings and
// topics for the pomona sketch. Calibration constants live in
// <PomonaCalibration.h> (shared with bringup); ONLY the two passwords
// (WIFI_PASS, MQTT_PASS) live in secrets.h (gitignored — copy
// firmware/secrets.h.example here and fill it in).

#pragma once

#include <Arduino.h>

// ---- pins (docs/wiring.md pin map) -----------------------------------
const int PIN_TDS = A0;    // Grove TDS, powered from 3V3
const int PIN_PH = A1;     // SEN0169-V2 via DFR0504 isolator
const int PIN_ONEWIRE = 2; // DS18B20 data, 4.7k pull-up to 3V3
const int PIN_PROBE = 3;   // CQRSENYW003 green wire (open collector)

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
#define MQTT_USER "pomona"

// ---- MQTT topics — pomona/<zone>/<metric> (docs/mqtt.md, final w/ #222)
const char MQTT_CLIENT_ID[] = "pomona-giga";
#define MQTT_BASE "pomona"
const char TOPIC_UNIT_STATUS[] = MQTT_BASE "/unit/status"; // retained + LWT
const char TOPIC_UNIT_FWVER[] = MQTT_BASE "/unit/fw_version"; // retained
const char TOPIC_UNIT_SENSORS[] = MQTT_BASE "/unit/sensors"; // retained JSON
const char TOPIC_UNIT_I2C_SCAN[] = MQTT_BASE "/unit/i2c_scan"; // retained JSON (diagnostics)
const char TOPIC_UNIT_I2C_REQUEST[] = MQTT_BASE "/unit/i2c_scan/get"; // any msg -> scan now (subscribed)
const char TOPIC_UNIT_RSSI[] = MQTT_BASE "/unit/rssi_dbm";
const char TOPIC_UNIT_UPTIME[] = MQTT_BASE "/unit/uptime_s";
const char TOPIC_WATER_TEMP[] = MQTT_BASE "/water/temp_c";
const char TOPIC_WATER_EC[] = MQTT_BASE "/water/ec_ms_cm";
const char TOPIC_WATER_PH[] = MQTT_BASE "/water/ph";
const char TOPIC_WATER_PH_RAW[] = MQTT_BASE "/water/ph_raw_v"; // always published (calibration/drift)
const char TOPIC_WATER_LEVEL_PCT[] = MQTT_BASE "/water/level_pct";
const char TOPIC_WATER_LEVEL_POINTS[] = MQTT_BASE "/water/level_points";
const char TOPIC_AIR_TEMP[] = MQTT_BASE "/air/temp_c";
const char TOPIC_AIR_RH[] = MQTT_BASE "/air/humidity_pct";
const char TOPIC_AIR_PRESSURE[] = MQTT_BASE "/air/pressure_hpa";
const char TOPIC_AIR_LUX[] = MQTT_BASE "/air/lux";
// OTA (basic slice of #243, see docs/mqtt.md): publish an http(s) .ota URL
// NON-retained to ota_url; the unit stages it in QSPI and reboots to apply.
const char TOPIC_UNIT_OTA_URL[] = MQTT_BASE "/unit/ota_url"; // subscribed
const char TOPIC_UNIT_OTA_RESULT[] = MQTT_BASE "/unit/ota_result"; // retained
// Control (#260, docs/mqtt.md "Control topics"): the unit publishes what it
// WANTS to happen and HA relays it to the Fibaro plugs. Retained, unlike the
// metrics — a request is the current desired state, so the broker replaying it
// on an HA restart is exactly right, whereas a replayed stale metric is not.
const char TOPIC_PUMP_REQUEST[] = MQTT_BASE "/pump/request";   // retained on|off
const char TOPIC_PUMP_REASON[] = MQTT_BASE "/pump/reason";     // retained
const char TOPIC_LIGHT_REQUEST[] = MQTT_BASE "/light/request"; // retained on|off
const char TOPIC_PUMP_OVERRIDE[] = MQTT_BASE "/pump/override"; // subscribed auto|on|off
const char TOPIC_CONTROL_MODE[] = MQTT_BASE "/control/mode";   // subscribed establishment|established
