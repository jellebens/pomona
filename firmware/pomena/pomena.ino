#include <WiFi.h>
#include <ArduinoOTA.h>
#include <secrets.h>

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASSWORD;

void setup() {
  Serial.begin(115200);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  // Set an OTA password (strongly recommended)
  ArduinoOTA.setPassword("myOtaPass123");

  // Optional: give the board a human-readable name
  ArduinoOTA.setHostname("garden-sensor");

  // OTA progress callbacks (helpful for debugging)
  ArduinoOTA.onStart([]() {
    Serial.println("OTA update starting...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update complete!");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error: %u\n", error);
  });

  ArduinoOTA.begin();  // Start the OTA listener
  Serial.println("OTA ready.");
}

void loop() {
  ArduinoOTA.handle();  // Check for incoming OTA updates

  // Your normal code goes here
  // e.g., read sensors, blink LEDs, etc.
}