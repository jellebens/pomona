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
#include "src/control/control.h"
#include "src/display/display.h"
#include "src/dosing/dosing.h"
#include "src/network/network.h"
#include "src/ota/ota.h"
#include "src/sensors/sensors.h"

static Readings readings;
static uint32_t lastReadMs = 0;
static uint32_t lastPublishMs = 0;

void setup() {
  // Safe state FIRST — before serial, watchdog, display, sensors or WiFi.
  // A rebooting unit (OTA, watchdog, brown-out) must be in a known state
  // before anything that can hang gets a chance to run (#260).
  controlInit();
  dosingInit(); // all DFR0523 channels held at explicit stop (#284)

  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) {}
  Serial.println();
  Serial.println("=== Pomona v" POMONA_FW_VERSION
                 " (built " __DATE__ " " __TIME__ ") ===");

  // hardware watchdog: a hang anywhere reboots the unit into a clean state
  mbed::Watchdog::get_instance().start(WATCHDOG_TIMEOUT_MS);

  displayInit(); // boot screen up immediately: the unit is visibly booting

  displayBootStatus("probing sensors...");
  sensorsInit();
  char addrs[96];
  int found = sensorsI2CScan(addrs, sizeof(addrs));
  char i2cLine[96];
  snprintf(i2cLine, sizeof(i2cLine), "I2C: %s (%d found)",
           found ? addrs : "nothing", found);

  char boot[224];
  snprintf(boot, sizeof(boot), "%s\nWiFi: connecting...", i2cLine);
  displayBootStatus(boot);

  networkInit();
  otaInit(); // capability probe; updates arrive via MQTT (network.cpp)

  sensorsRead(readings); // first sweep right away

  // stay on the boot screen while the network comes up (capped), updating
  // the progression line by line; the tiles' icons take over afterwards
  uint32_t netStart = millis();
  bool wifiShown = false;
  while (millis() - netStart < BOOT_NET_WAIT_MS && !mqttConnected()) {
    mbed::Watchdog::get_instance().kick();
    networkService();
    if (wifiConnected() && !wifiShown) {
      wifiShown = true;
      snprintf(boot, sizeof(boot), "%s\nWiFi: connected\nMQTT: connecting...",
               i2cLine);
      displayBootStatus(boot);
    }
    delay(100);
  }
  snprintf(boot, sizeof(boot), "%s\nWiFi: %s\nMQTT: %s", i2cLine,
           wifiConnected() ? "connected" : "not yet - retrying in background",
           mqttConnected() ? "connected" : "not yet - retrying in background");
  displayBootStatus(boot);

  // short hold so the final state is readable
  uint32_t holdStart = millis();
  while (millis() - holdStart < BOOT_SCREEN_HOLD_MS) {
    mbed::Watchdog::get_instance().kick();
    delay(50);
  }

  displayShowMain(); // swap to the tiles UI
  displayUpdate(readings);
  lastReadMs = millis();
}

// Serial debug commands (#284 bench work): "dose chN fwd|rev|stop [ms]
// [speed]", "dose status", "help". Same code paths as pomona/dose/test, so
// USB-tethered testing needs no broker round-trip.
static void pollSerialCommands() {
  static char line[96];
  static size_t len = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (len == 0) continue;
      line[len] = '\0';
      len = 0;
      if (strcmp(line, "dose status") == 0)
        dosingDebugStatus();
      else if (strncmp(line, "dose ", 5) == 0)
        dosingHandleCommand(line + 5);
      else if (strcmp(line, "help") == 0)
        Serial.println(
            "commands: dose chN fwd|rev|stop [ms] [speed] | dose status | help");
      else {
        Serial.print("unknown command: ");
        Serial.println(line);
      }
    } else if (len < sizeof(line) - 1) {
      line[len++] = c;
    }
  }
}

void loop() {
  mbed::Watchdog::get_instance().kick();

  pollSerialCommands(); // USB bench commands (#284)
  networkService(); // reconnect state machine + MQTT keepalive + OTA trigger

  displayLinkStatus(wifiConnected(), mqttConnected()); // live status icons

  uint32_t now = millis();
  if (now - lastReadMs >= SENSOR_READ_MS) {
    lastReadMs = now;
    sensorsRead(readings); // blocking, worst case ~1.5 s (see sensors.h)
    displayUpdate(readings);
  }

  // Decide locally, every loop. This never waits on the network: if WiFi or
  // the broker are down the tower still waters correctly, it just cannot say
  // so (#260, docs/control-architecture.md).
  controlService(readings);
  dosingService(); // stop timed bench runs on schedule (#284)

  if (mqttConnected() && now - lastPublishMs >= MQTT_PUBLISH_MS) {
    lastPublishMs = now;
    networkPublish(readings);
    Serial.println("mqtt: published readings");
  }

  displayService(); // LVGL timers + idle blanking (#248)
  delay(4);
}
