// Pomona firmware v1 — network module implementation (Trello #229).
//
// Broker + topic schema: docs/mqtt.md (EMQX mqtt.lab.local:1883, finalized
// with #222). Credentials come from secrets.h (gitignored, Layer 1 of
// docs/ota-and-secrets.md); #244 moves them into the ATECC608A.

#include "network.h"
#include "config.h"
#include "ota.h"

#if !__has_include("secrets.h")
#error "Copy firmware/secrets.h.example to firmware/pomona/secrets.h and fill in credentials (docs/ota-and-secrets.md, Layer 1)"
#endif
#include "secrets.h"

#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include <PomonaVersion.h>

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

// ---- connect steps ---------------------------------------------------

static bool connectWifi() {
  Serial.print("WiFi: connecting to ");
  Serial.println(SECRET_WIFI_SSID);
  // blocking, but bounded well under WATCHDOG_TIMEOUT_MS
  if (WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASS) != WL_CONNECTED) {
    Serial.println("WiFi: connect FAILED");
    return false;
  }
  Serial.print("WiFi: connected, IP ");
  Serial.println(WiFi.localIP());
  return true;
}

static bool connectMqtt() {
  mqtt.setId(MQTT_CLIENT_ID);
  mqtt.setUsernamePassword(SECRET_MQTT_USER, SECRET_MQTT_PASS);

  // last will: broker flips the retained status to offline if we vanish
  mqtt.beginWill(TOPIC_UNIT_STATUS, true, 1);
  mqtt.print("offline");
  mqtt.endWill();

  Serial.print("MQTT: connecting to ");
  Serial.print(SECRET_MQTT_HOST);
  Serial.print(":");
  Serial.println(SECRET_MQTT_PORT);
  if (!mqtt.connect(SECRET_MQTT_HOST, SECRET_MQTT_PORT)) {
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
  // Nothing up-front: networkService() connects lazily, so the screen
  // comes up immediately even with WiFi down.
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
