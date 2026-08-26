// Pomona firmware v1 — network module implementation (Trello #229).
//
// Broker + topic schema: docs/mqtt.md (EMQX mqtt.lab.local:1883, finalized
// with #222). Credentials come from secrets.h (gitignored, Layer 1 of
// docs/ota-and-secrets.md); #244 moves them into the ATECC608A.

#include "network.h"
#include "../../config.h"
#include "../ota/ota.h"

#if !__has_include("../../secrets.h")
#error "Copy firmware/secrets.h.example to firmware/pomona/secrets.h and fill in the two passwords (docs/ota-and-secrets.md, Layer 1)"
#endif
#include "../../secrets.h"

#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include <PomonaVersion.h>
#include <mbed.h> // Watchdog kicks around blocking WiFi attempts

static WiFiClient wifiClient;
static MqttClient mqtt(wifiClient);

static uint32_t nextAttemptMs = 0;
static uint32_t backoffMs = NET_RETRY_MIN_MS;

static char otaUrlPending[224] = ""; // set by onMqttMessage, run in service

static void onMqttMessage(int /*messageSize*/) {
  String topic = mqtt.messageTopic();
  char payload[224];
  size_t n = 0;
  while (mqtt.available() && n < sizeof(payload) - 1)
    payload[n++] = (char)mqtt.read();
  payload[n] = '\0';
  if (topic == TOPIC_UNIT_OTA_URL)
    snprintf(otaUrlPending, sizeof(otaUrlPending), "%s", payload);
}

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }
bool mqttConnected() { return mqtt.connected(); }

// ---- WiFi diagnostics (field bug: silent connect failures) -----------

static const char *wlStatusName(int s) {
  switch (s) {
    case WL_CONNECTED: return "CONNECTED";
    case WL_NO_SHIELD: return "NO_SHIELD/NO_MODULE";
    case WL_IDLE_STATUS: return "IDLE";
    case WL_NO_SSID_AVAIL: return "NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED: return "DISCONNECTED";
    default: return "?";
  }
}

// One-time module/firmware report at boot. On the GIGA the CYW4343W WiFi
// firmware lives in a QSPI partition; if that partition was never
// provisioned the core reports version "v0.0.0" ("" = no module at all)
// and every scan/connect fails — detect it and say exactly what to run.
static void wifiDiagnostics() {
  const char *fw = WiFi.firmwareVersion();
  int s = WiFi.status();
  Serial.print("WiFi: firmware ");
  Serial.print((fw && fw[0]) ? fw : "(none — no module?)");
  Serial.print(", status ");
  Serial.print(s);
  Serial.print(" (");
  Serial.print(wlStatusName(s));
  Serial.println(")");
  if (!fw || !fw[0] || strcmp(fw, "v0.0.0") == 0) {
    Serial.println("WiFi: !! WiFi firmware missing/unprovisioned — run the");
    Serial.println("WiFi: !! one-time STM32H747_System -> WiFiFirmwareUpdater");
    Serial.println("WiFi: !! sketch over USB, then reflash this firmware.");
  }
}

// After every attempt failed, scan once: distinguishes "radio dead / no
// firmware" (0 networks) from "our SSID not visible" from "visible but
// join refused" (passphrase/band).
static void wifiScanReport() {
  mbed::Watchdog::get_instance().kick(); // scan blocks a few seconds
  int8_t n = WiFi.scanNetworks();
  if (n <= 0) {
    Serial.println("WiFi: scan found NO networks — antenna on the UFL "
                   "connector? WiFi firmware provisioned?");
    return;
  }
  bool seen = false;
  for (int8_t i = 0; i < n; i++)
    if (strcmp(WiFi.SSID(i), WIFI_SSID) == 0) seen = true;
  Serial.print("WiFi: scan saw ");
  Serial.print(n);
  Serial.print(" network(s); SSID \"" WIFI_SSID "\" ");
  Serial.println(seen ? "IS visible — join refused (passphrase? band?)"
                      : "NOT visible (2.4 GHz off? hidden? out of range?)");
}

// ---- connect steps ---------------------------------------------------

static bool connectWifi() {
  for (int attempt = 1; attempt <= WIFI_BEGIN_ATTEMPTS; attempt++) {
    // each begin() blocks up to ~12 s (scan + 7 s connect timeout) —
    // kick the 30 s watchdog per attempt, never during one
    mbed::Watchdog::get_instance().kick();
    Serial.print("WiFi: connecting to \"" WIFI_SSID "\" (attempt ");
    Serial.print(attempt);
    Serial.print("/");
    Serial.print(WIFI_BEGIN_ATTEMPTS);
    Serial.println(")");
    int rc = WiFi.begin(WIFI_SSID, WIFI_PASS);
    if (rc == WL_CONNECTED) {
      Serial.print("WiFi: connected, IP ");
      Serial.print(WiFi.localIP());
      Serial.print(", RSSI ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      return true;
    }
    Serial.print("WiFi: attempt failed, status ");
    Serial.print(rc);
    Serial.print(" (");
    Serial.print(wlStatusName(rc));
    Serial.println(")");
  }
  wifiScanReport();
  return false;
}

static bool connectMqtt() {
  mqtt.setId(MQTT_CLIENT_ID);
  mqtt.setUsernamePassword(MQTT_USER, MQTT_PASS);

  // last will: broker flips the retained status to offline if we vanish
  mqtt.beginWill(TOPIC_UNIT_STATUS, true, 1);
  mqtt.print("offline");
  mqtt.endWill();

  Serial.print("MQTT: connecting to ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  if (!mqtt.connect(MQTT_HOST, MQTT_PORT)) {
    Serial.print("MQTT: connect FAILED, error ");
    Serial.println(mqtt.connectError());
    return false;
  }

  mqtt.beginMessage(TOPIC_UNIT_STATUS, true, 1);
  mqtt.print("online");
  mqtt.endMessage();
  mqtt.beginMessage(TOPIC_UNIT_FWVER, true, 1);
  mqtt.print(POMONA_FW_VERSION);
  mqtt.endMessage();

  mqtt.onMessage(onMqttMessage);
  mqtt.subscribe(TOPIC_UNIT_OTA_URL, 1); // basic OTA trigger (docs/mqtt.md)

  Serial.println("MQTT: connected");
  return true;
}

// Run a queued OTA request outside the MQTT receive callback. On success
// the unit reboots inside otaApplyFromUrl and never returns here.
static void handleOtaPending() {
  if (otaUrlPending[0] == '\0') return;
  char url[sizeof(otaUrlPending)];
  snprintf(url, sizeof(url), "%s", otaUrlPending);
  otaUrlPending[0] = '\0';

  mqtt.beginMessage(TOPIC_UNIT_OTA_RESULT, true, 1);
  mqtt.print("applying ");
  mqtt.print(url);
  mqtt.endMessage();

  char err[96];
  if (!otaApplyFromUrl(url, err, sizeof(err))) {
    Serial.print("ota: FAILED — ");
    Serial.println(err);
    if (mqtt.connected()) {
      mqtt.beginMessage(TOPIC_UNIT_OTA_RESULT, true, 1);
      mqtt.print("failed: ");
      mqtt.print(err);
      mqtt.endMessage();
    }
  }
}

// ---- public API ------------------------------------------------------

void networkInit() {
  // Boot-time diagnostics only; networkService() connects lazily, so the
  // screen comes up immediately even with WiFi down.
  wifiDiagnostics();
}

void networkService() {
  if (wifiConnected() && mqtt.connected()) {
    mqtt.poll(); // keepalive + inbound (ota_url subscription)
    handleOtaPending();
    return;
  }

  if ((int32_t)(millis() - nextAttemptMs) < 0) return; // backoff window

  bool ok = wifiConnected() || connectWifi();
  if (ok) ok = connectMqtt();

  if (ok) {
    backoffMs = NET_RETRY_MIN_MS;
  } else {
    nextAttemptMs = millis() + backoffMs;
    Serial.print("net: retry in ");
    Serial.print(backoffMs / 1000);
    Serial.println(" s");
    backoffMs = min(backoffMs * 2, NET_RETRY_MAX_MS);
  }
}

// ---- publishing ------------------------------------------------------

static void pubFloat(const char *topic, float v, uint8_t decimals) {
  mqtt.beginMessage(topic);
  mqtt.print(v, decimals);
  mqtt.endMessage();
}

static void pubInt(const char *topic, long v) {
  mqtt.beginMessage(topic);
  mqtt.print(v);
  mqtt.endMessage();
}

void networkPublish(const Readings &r) {
  if (!mqtt.connected()) return;

  // water — only what the sensors actually answered (docs/mqtt.md)
  if (r.waterTempOk) pubFloat(TOPIC_WATER_TEMP, r.waterTempC, 1);
  pubFloat(TOPIC_WATER_EC, r.ecMsCm, 2); // analog: always published
  if (r.phOk) pubFloat(TOPIC_WATER_PH, r.ph, 2);
  if (r.levelOk) pubInt(TOPIC_WATER_LEVEL_PCT, r.levelPct);
  if (r.probePoints >= 0) pubInt(TOPIC_WATER_LEVEL_POINTS, r.probePoints);

  // air
  if (r.bmeOk) {
    pubFloat(TOPIC_AIR_TEMP, r.airTempC, 1);
    pubFloat(TOPIC_AIR_RH, r.humidityPct, 1);
    pubFloat(TOPIC_AIR_PRESSURE, r.pressureHpa, 1);
  }
  if (r.luxOk) pubFloat(TOPIC_AIR_LUX, r.lux, 0);

  // unit health
  pubInt(TOPIC_UNIT_RSSI, WiFi.RSSI());
  pubInt(TOPIC_UNIT_UPTIME, millis() / 1000);

  // retained availability map: consumers see which metrics to expect
  char buf[192];
  snprintf(buf, sizeof(buf),
           "{\"water_temp\":%s,\"ph_calibrated\":%s,\"level_strip\":%s,"
           "\"level_probe\":%s,\"bme280\":%s,\"bh1750\":%s}",
           r.waterTempOk ? "true" : "false", r.phOk ? "true" : "false",
           r.levelOk ? "true" : "false", r.probePoints >= 0 ? "true" : "false",
           r.bmeOk ? "true" : "false", r.luxOk ? "true" : "false");
  mqtt.beginMessage(TOPIC_UNIT_SENSORS, true, 1);
  mqtt.print(buf);
  mqtt.endMessage();
}
