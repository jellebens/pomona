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
const uint8_t ADDR_BME280 = 0x76; // strapped! 0x77 belongs to the level strip
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
const char TOPIC_UNIT_RSSI[] = MQTT_BASE "/unit/rssi_dbm";
const char TOPIC_UNIT_UPTIME[] = MQTT_BASE "/unit/uptime_s";
const char TOPIC_WATER_TEMP[] = MQTT_BASE "/water/temp_c";
const char TOPIC_WATER_EC[] = MQTT_BASE "/water/ec_ms_cm";
const char TOPIC_WATER_PH[] = MQTT_BASE "/water/ph";
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
