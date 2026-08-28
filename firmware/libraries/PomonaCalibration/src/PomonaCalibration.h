// Pomona sensor calibration — single source of truth (Trello #229).
//
// Recorded calibration values shared by every sketch that reads the water
// probes (bringup, pomona). Update the values HERE after a calibration
// session; procedures + recorded history live in the per-sensor docs under
// docs/sensors/ (ec-tds.md, ph.md).

#pragma once

#include <Arduino.h>

// EC: single-point against 1413 uS/cm fluid. 1.0 = uncalibrated.
// 2026-08-28: fluid read a stable 1.45 mS/cm at 25.6 C (assembled unit,
// probe settled ~8 min) -> K = 1.413 / 1.45. History: docs/sensors/ec-tds.md.
const float EC_CAL_K = 0.9745f;

// pH: two-point. Record the measured voltages in the buffers; NAN = not yet
// calibrated, readers report raw voltage only. The BUF values are the pH
// printed on the buffer packets actually used (owner's set is 7.00/4.00,
// not the 6.86/4.01 family).
const float PH_BUF_NEUTRAL = 7.00f; // pH of the neutral buffer
const float PH_BUF_ACID = 4.00f;    // pH of the acid buffer
const float PH_V_NEUTRAL = 1.56f; // 2026-08-28, pH 7.00 buffer @ 25.5 C (settled, 2nd pass)
const float PH_V_ACID = 2.10f;    // 2026-08-28, pH 4.00 buffer @ 25.4 C (settled, 2nd pass)
// Validation: pH 10.01 buffer read 1.00 V vs 1.02 V predicted — linear.
