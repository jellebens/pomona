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

// Is a distance the water surface at all? (pomona 2.4.3, #283)
//
// 2.4.2 clamped every distance to the table's ends, so a reading the sensor could not
// mean became a confident number: on the tower 2026-10-10/11 the return water falling
// past the sensor during a pump run read 33-53 mm -> "10 L", and a wet face (condensation
// over 25 C water) read 240-720 mm, jumping minute to minute -> "0 L" for a day, while
// the tank held ~9 L. Now a reading is litres only when it is plausible AND steady:
//   * Splash   — nearer than FULL by more than TANK_SPLASH_MARGIN_MM: the return stream,
//                foam, a drop on the face. The tank cannot fill past its full mark.
//   * NoEcho   — farther than EMPTY by more than TANK_FAR_MARGIN_MM: the floor is 284 mm
//                away, so the pulse did not come back off the water (a wet or tilted face).
//   * Unsteady — the last readings disagree by more than TANK_STEADY_MM.
//   * Near     — (2.4.9) at or under the sensor's blind-zone floor: most frames said "closer
//                than I can measure" (water, a drop or the return stream at the face). The
//                2.4.5 mount's FULL (37 mm) sits within the splash margin of the 30 mm floor,
//                so a pinned sensor passed as Ok: 30 mm -> "9.00 L" for 2.5 days, 2026-10-16..19,
//                every frame `near`, zero good.
// Between FULL-margin and FULL (or EMPTY and EMPTY+margin) a reading still clamps.
enum class TankRead : unsigned char { Ok, Splash, NoEcho, Unsteady, Near };

const float TANK_SPLASH_MARGIN_MM = 15.0f;
const float TANK_FAR_MARGIN_MM = 20.0f;
const float TANK_STEADY_MM = 15.0f;

// spreadMm: max - min of the recent readings (the caller's window), NAN = not enough yet.
// floorMm: the sensor's blind-zone floor (A02YYUW::MIN_MM), the distance it reports for a
// frame inside the blind zone.
inline TankRead tankJudge(const TankPoint *points, unsigned n, float mm, float spreadMm,
                          float floorMm) {
  if (n == 0) return TankRead::NoEcho;
  if (mm <= floorMm) return TankRead::Near;
  if (mm < points[0].mm - TANK_SPLASH_MARGIN_MM) return TankRead::Splash;
  if (mm > points[n - 1].mm + TANK_FAR_MARGIN_MM) return TankRead::NoEcho;
  if (isnan(spreadMm) || spreadMm > TANK_STEADY_MM) return TankRead::Unsteady;
  return TankRead::Ok;
}

inline const char *tankReadName(TankRead s) {
  switch (s) {
  case TankRead::Ok: return "ok";
  case TankRead::Splash: return "splash";
  case TankRead::NoEcho: return "no_echo";
  case TankRead::Unsteady: return "unsteady";
  case TankRead::Near: return "near";
  }
  return "?";
}
