#include "PhotoLevelProbe.h"

float PhotoLevelProbe::readFrequency() {
  // One full period = one HIGH + one LOW phase. 120 ms timeout per phase
  // comfortably covers the slowest (20 Hz dry = 25 ms half-periods).
  unsigned long hiUs = pulseIn(_pin, HIGH, 120000UL);
  unsigned long loUs = pulseIn(_pin, LOW, 120000UL);
  if (hiUs == 0 || loUs == 0) return NAN;
  return 1.0e6f / (float)(hiUs + loUs);
}

int PhotoLevelProbe::pointsFromFrequency(float hz) {
  if (isnan(hz)) return -1;
  if (hz < 32.0f) return 0;   // ~20 Hz: dry
  if (hz < 71.0f) return 1;   // ~50 Hz
  if (hz < 141.0f) return 2;  // ~100 Hz
  if (hz < 283.0f) return 3;  // ~200 Hz
  return 4;                   // ~400 Hz
}

int PhotoLevelProbe::points() { return pointsFromFrequency(readFrequency()); }
