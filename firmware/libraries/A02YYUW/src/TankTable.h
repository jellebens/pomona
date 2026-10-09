// Distance -> litres for a reservoir of any shape (pomona 2.4.1, #283).
//
// A tapered tank holds fewer litres per mm near the bottom, so two calibration
// points and a straight line are wrong. The table is a fill measured on the
// tank itself: (sensor-to-water distance, litres in the tank), interpolated
// linearly between neighbours and clamped to the first/last point.

#pragma once

#include <math.h> // NAN

struct TankPoint {
  float mm;     // sensor face -> water surface
  float litres; // water in the tank at that distance
};

// points[] sorted by mm, ascending (fullest first). Returns NAN for an empty table.
inline float tankLitres(const TankPoint *points, unsigned n, float mm) {
  if (n == 0) return NAN;
  if (mm <= points[0].mm) return points[0].litres;
  if (mm >= points[n - 1].mm) return points[n - 1].litres;
  for (unsigned i = 1; i < n; i++) {
    if (mm <= points[i].mm) {
      const TankPoint &a = points[i - 1], &b = points[i];
      return a.litres + (mm - a.mm) * (b.litres - a.litres) / (b.mm - a.mm);
    }
  }
  return points[n - 1].litres; // not reached
}
