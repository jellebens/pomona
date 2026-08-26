// Pomona monitoring firmware v1 (Trello #229).
//
// The application sketch for the unit at the tower: reads every v1 sensor,
// publishes to the cluster MQTT broker (docs/mqtt.md), shows current
// readings on the GIGA Display Shield via LVGL (idle blanking + touch
// wake, #248) and accepts basic OTA updates over MQTT (#243 slice).
// Bench verification stays in firmware/bringup; history graphs are #223.
//
// Board: Arduino GIGA R1 WiFi + GIGA Display Shield (arduino:mbed_giga:giga)
// Modules: config.h (settings) / sensors / network / display — see README.md
// Secrets: copy firmware/secrets.h.example here as secrets.h (gitignored).
// NOTE: WiFi needs the external antenna on the Micro UFL connector.

#include <mbed.h>
#include <PomonaVersion.h> // libraries/PomonaVersion — bumped by deploy.ps1

#include "config.h"
#include "display.h"
#include "network.h"
#include "ota.h"
#include "sensors.h"

static Readings readings;
static uint32_t lastReadMs = 0;
static uint32_t lastPublishMs = 0;

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) {}
  Serial.println();
  Serial.println("=== Pomona v" POMONA_FW_VERSION
                 " (built " __DATE__ " " __TIME__ ") ===");

  // hardware watchdog: a hang anywhere reboots the unit into a clean state
  mbed::Watchdog::get_instance().start(WATCHDOG_TIMEOUT_MS);

  displayInit(); // screen first: visible even with sensors/WiFi absent
  sensorsInit();
  networkInit();
  otaInit(); // capability probe; updates arrive via MQTT (network.cpp)

  sensorsRead(readings); // first sweep right away
  displayUpdate(readings);
  lastReadMs = millis();
}

void loop() {
  mbed::Watchdog::get_instance().kick();

  networkService(); // reconnect state machine + MQTT keepalive + OTA trigger

  displayLinkStatus(wifiConnected(), mqttConnected()); // live status icons

  uint32_t now = millis();
  if (now - lastReadMs >= SENSOR_READ_MS) {
    lastReadMs = now;
    sensorsRead(readings); // blocking, worst case ~1.5 s (see sensors.h)
    displayUpdate(readings);
  }

  if (mqttConnected() && now - lastPublishMs >= MQTT_PUBLISH_MS) {
    lastPublishMs = now;
    networkPublish(readings);
    Serial.println("mqtt: published readings");
  }

  displayService(); // LVGL timers + idle blanking (#248)
  delay(4);
}
