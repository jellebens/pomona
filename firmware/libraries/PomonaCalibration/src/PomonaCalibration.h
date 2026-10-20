// Pomona sensor + doser calibration — single source of truth (Trello #229, #295).
//
// Recorded calibration values shared by every sketch that reads the water
// probes (bringup, pomona) or drives the DFR0523 dosers (pomona). Update the
// values HERE after a calibration session; procedures + recorded history
// live in the per-sensor docs under docs/sensors/ (ec-tds.md, ph.md) and
// docs/dosing/dfr0523.md.

#pragma once

#include <Arduino.h>

// EC: single-point against 1413 uS/cm fluid. 1.0 = uncalibrated.
// 2026-09-10: post-Cat5e perm wiring (signal now A2), fluid read a stable
// 1.79 mS/cm at 26.2 C with the old K 0.9745 -> K = 0.9745 * 1.413 / 1.79.
// The 21% shift is the new cable run - recalibrate after ANY wiring change.
// History: docs/sensors/ec-tds.md.
const float EC_CAL_K = 0.7692f;

// pH: two-point. Record the measured voltages in the buffers; NAN = not yet
// calibrated, readers report raw voltage only. The BUF values are the pH
// printed on the buffer packets actually used (owner's set is 7.00/4.00,
// not the 6.86/4.01 family).
const float PH_BUF_NEUTRAL = 7.00f; // pH of the neutral buffer
const float PH_BUF_ACID = 4.00f;    // pH of the acid buffer
const float PH_V_NEUTRAL = 1.529f; // 2026-09-10, pH 7.00 buffer @ 26.1 C (settled, 2nd pass; post-Cat5e)
const float PH_V_ACID = 2.074f;    // 2026-09-10, pH 4.00 buffer @ 26.1 C (settled, 2nd pass)
// Validation: pH 10.01 buffer read 0.979 V vs 0.982 V predicted — linear.
// History (2026-08-28 anchors 1.56/2.10): same slope, ~0.03 V offset drift.

// Tank level: A02YYUW ultrasonic above the reservoir (docs/sensors/level-sonic.md).
// The tank TAPERS (narrower at the bottom), so distance -> litres is a measured
// table, not a line. Shape from the fill of 2026-10-09 21:56-22:14 CEST (tank emptied
// to a puddle, known volumes poured in, pump off, settled readings; 6.5 -> 9.5 L was
// one 3 L pour). That fill was read with the sensor lower than its final mount; with
// the LID CLOSED (the final mount) 10 L reads a steady 76 mm (2026-10-09 22:50-22:55,
// pump off; owner's call), so the fill is shifted +36 mm (9.5 L: 46 -> 82 mm; the top
// runs ~12 mm per litre). FULL is 10 L — 46 mm clear of the 30 mm blind zone.
// Sorted fullest first. Re-measure after moving the sensor or changing the tank — a
// fixed offset is enough when only the sensor height changes. Empty = raw distance only.
//
// 2.4.5 — the sensor REMOUNTED (2026-10-16, new holes in the lid): the old mount had been
// hearing something before the water (87 mm steady at ~5 L). Measured at the new mount, pump
// off, known volumes poured in by the owner: 5.5 L = 110 mm (109-114), 7.5 L = 66, 8.0 L = 56,
// 9.0 L = 37 — a straight ~20 mm per litre. These replace the top of the table; below 5.5 L the
// 2026-10-09 fill's shape is kept (21 mm/L at 4.5-5.5 L, it joins), shifted -18 mm to meet 110 mm
// (verify once with a known volume when the tank runs low). 10 L would sit inside the 30 mm blind
// zone at this mount: 9.0 L is the top — "9 L or more".
//
// 2.4.10 — the sensor REMOUNTED again (2026-10-19, its two transducer bosses clear of the lid):
// the 2.4.5 points no longer hold. The owner's fill of 2026-10-20, pump off and settled: before
// it ~88 mm (85-90 over the afternoon's pump-off spells), "almost 4 L" poured in, then exactly
// 10 L = 36 mm. So 10 L = 36 and ~6.1 L = 88: ~13 mm per litre, as the 2026-10-09 fill measured
// near the top (~12 mm/L; the tank tapers). Straight between the two; below 6.1 L the
// 2026-10-09 shape, shifted -33 mm to meet 88 mm (verify once with a known volume when the tank
// runs low). 10 L is FULL again, 6 mm clear of the 30 mm blind-zone floor (2.4.9 says `near`
// there).
#include <TankTable.h> // libraries/A02YYUW
const TankPoint TANK_TABLE[] = {
    {36, 10.0}, {49, 9.0}, {63, 8.0}, {76, 7.0}, {88, 6.1}, {95, 5.5}, {116, 4.5}, {124, 4.0},
    {138, 3.5}, {159, 3.0}, {176, 2.5}, {188, 2.0}, {204, 1.5}, {218, 1.0}, {230, 0.5}, {251, 0.0},
};
const unsigned TANK_TABLE_N = sizeof(TANK_TABLE) / sizeof(TANK_TABLE[0]);
const float TANK_FULL_L = 10.0f; // 100 % — the volume at the table's first point

// DFR0523 dosing channels (docs/dosing/dfr0523.md — 2026-09-09 recalibration on
// the FINAL wiring and tube runs). ml/s is per channel AND per speed: the
// speed->flow curve is nonlinear, never interpolate; use speed 50 for fine
// doses and 100 for volume. Since firmware 2.0.0 (#295, ceres ADR-0008) the
// node converts Ceres's ml-based dose/request with THESE numbers and
// announces them in sys/meta; the same facts sit in the unit's config document
// (Annona (Ceres' config service)) for the brain's own sizing. Recalibrate after any tube
// change and after the first week of use; keep both copies in step.
// Channel order = DFR0523 ch1..ch4; an empty reagent = no doser on that channel.
struct DoserCal {
  const char *reagent; // Ceres's reagent id
  float fullMlS;       // speed 100
  float slowMlS;       // at slowSpeed
  int slowSpeed;
};
const DoserCal DOSER_CAL[4] = {
    {"ph_down", 0.48f, 0.11f, 50},    // ch1, D6 — BPT tube
    {"nutrient_a", 0.66f, 0.24f, 50}, // ch2, D4
    {"nutrient_b", 0.60f, 0.11f, 50}, // ch3, D5
    {"", 0.0f, 0.0f, 0},              // ch4, D7 — spare, no healthy pump (#287)
};
