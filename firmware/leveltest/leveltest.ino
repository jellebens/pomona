// Pomona — Grove water level strip calibration dump (Trello #242)
//
// Prints the RAW value of all 20 capacitive pads once per second, plus the
// wet-pad count at the current threshold. Use this (not bringup.ino) to
// pick the mounting spot and tune the wet threshold:
//
//   1. Dry strip  -> note the highest raw value  (dry_max)
//   2. Wet pads   -> note the lowest raw value among covered pads (wet_min)
//   3. Threshold goes between them; comfortable gap = good mount.
//      Through-wall mounting weakens the signal — if wet_min barely clears
//      dry_max, move the strip inside the tank in a waterproof sleeve.
//
// Record the outcome in docs/wiring.md ("Level strip calibration").
//
// Wiring (docs/wiring.md): Grove yellow=SCL, white=SDA, red=3V3, black=GND.
// Driver: libraries/GroveWaterLevel (sketchbook = the firmware/ folder).

#include <Wire.h>
#include <GroveWaterLevel.h>

GroveWaterLevel level(Wire); // default threshold 100; tune with this dump

bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 5000) {}
  Serial.println();
  Serial.println("=== Grove level strip raw dump (pad 1 = bottom) ===");
  Wire.begin();
}

void loop() {
  if (!level.read()) {
    Serial.print("I2C ERROR — low(0x77):");
    Serial.print(i2cPresent(GroveWaterLevel::ADDR_LOW) ? "ok" : "FAIL");
    Serial.print(" high(0x78):");
    Serial.println(i2cPresent(GroveWaterLevel::ADDR_HIGH) ? "ok" : "FAIL");
    Serial.println("  check red=3V3 black=GND yellow=SCL white=SDA (yellow/white swap is the classic)");
    delay(1000);
    return;
  }

  Serial.print("raw:");
  for (uint8_t i = 0; i < GroveWaterLevel::PAD_COUNT; i++) {
    Serial.print(i == 8 ? " |" : "");
    Serial.print(' ');
    Serial.print(level.raw(i));
  }
  Serial.print("   wet=");
  Serial.print(level.wetCount());
  Serial.print("/20 (thr ");
  Serial.print(level.threshold());
  Serial.print(") level=");
  Serial.print(level.percent());
  Serial.println("%");

  delay(1000);
}
