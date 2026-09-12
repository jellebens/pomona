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

// DFR0523 dosing channels (docs/dosing/dfr0523.md — 2026-09-09 recalibration on
// the FINAL wiring and tube runs). ml/s is per channel AND per speed: the
// speed->flow curve is nonlinear, never interpolate; use speed 50 for fine
// doses and 100 for volume. Since firmware 2.0.0 (#295, demeter ADR-0008) the
// node converts Demeter's ml-based dose/request with THESE numbers and
// announces them in sys/meta; the same facts sit in the unit's config document
// (Demeter's registry) for the brain's own sizing. Recalibrate after any tube
// change and after the first week of use; keep both copies in step.
// Channel order = DFR0523 ch1..ch4; an empty reagent = no doser on that channel.
struct DoserCal {
  const char *reagent; // Demeter's reagent id
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
