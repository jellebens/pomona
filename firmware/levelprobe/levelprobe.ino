// Pomona — CQRSENYW003 photoelectric level probe test (Trello #242)
//
// Prints the probe's output frequency and the decoded number of submerged
// detection points once per second. Use for bench verification and to find
// the right mounting height (dip it step by step and watch the points).
//
// Expected: ~20 Hz dry (0 pts), ~50/100/200/400 Hz for points 1..4.
// "no signal" = green wire not on PIN_PROBE, or probe unpowered — it needs
// its own 3V3+GND and draws up to ~80 mA.
//
// Wiring (docs/wiring.md): black=GND, red=3V3, green=D3 (open collector,
// pin uses the internal pull-up).

#include <PhotoLevelProbe.h>

const pin_size_t PIN_PROBE = 3;

PhotoLevelProbe probe(PIN_PROBE);

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 5000) {}
  Serial.println();
  Serial.println("=== CQRSENYW003 level probe test (D3) ===");
  probe.begin();
}

void loop() {
  float hz = probe.readFrequency();
  int pts = PhotoLevelProbe::pointsFromFrequency(hz);

  Serial.print("freq=");
  if (isnan(hz)) {
    Serial.print("no signal");
  } else {
    Serial.print(hz, 1);
    Serial.print(" Hz");
  }
  Serial.print("  points=");
  if (pts < 0) Serial.println("?");
  else {
    Serial.print(pts);
    Serial.println(pts == 0 ? "/4 (dry)" : "/4");
  }

  delay(1000);
}
