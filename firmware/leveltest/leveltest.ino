// Pomona — Grove water level strip calibration dump (Trello #242)
//
// Prints the RAW value of all 20 capacitive pads once per second, plus the
// wet-pad count at the current threshold. Use this (not bringup.ino) to
// pick the mounting spot and tune LEVEL_WET_THRESHOLD:
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

#include <Wire.h>

const uint8_t ADDR_LEVEL_LOW = 0x77;  // low 8 pads  (bottom of the strip)
const uint8_t ADDR_LEVEL_HIGH = 0x78; // high 12 pads (top of the strip)
uint8_t LEVEL_WET_THRESHOLD = 100;    // Seeed default; tune with this dump

bool readBank(uint8_t addr, uint8_t *buf, uint8_t n) {
  Wire.requestFrom(addr, n);
  for (uint8_t i = 0; i < n; i++) {
    if (!Wire.available()) return false;
    buf[i] = Wire.read();
  }
  return true;
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
  uint8_t low[8], high[12];
  bool okLow = readBank(ADDR_LEVEL_LOW, low, 8);
  bool okHigh = readBank(ADDR_LEVEL_HIGH, high, 12);

  if (!okLow || !okHigh) {
    Serial.print("I2C ERROR — low(0x77):");
    Serial.print(okLow ? "ok" : "FAIL");
    Serial.print(" high(0x78):");
    Serial.println(okHigh ? "ok" : "FAIL");
    Serial.println("  check red=3V3 black=GND yellow=SCL white=SDA (yellow/white swap is the classic)");
    delay(1000);
    return;
  }

  int wet = 0;
  Serial.print("raw:");
  for (int i = 0; i < 8; i++) {
    Serial.print(' ');
    Serial.print(low[i]);
    if (low[i] > LEVEL_WET_THRESHOLD) wet++;
  }
  Serial.print(" |");
  for (int i = 0; i < 12; i++) {
    Serial.print(' ');
    Serial.print(high[i]);
    if (high[i] > LEVEL_WET_THRESHOLD) wet++;
  }
  Serial.print("   wet=");
  Serial.print(wet);
  Serial.print("/20 (thr ");
  Serial.print(LEVEL_WET_THRESHOLD);
  Serial.print(") level=");
  Serial.print(wet * 5);
  Serial.println("%");

  delay(1000);
}
