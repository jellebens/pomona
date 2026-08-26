// Pomona sensor calibration — single source of truth (Trello #229).
//
// Recorded calibration values shared by every sketch that reads the water
// probes (bringup, pomona). Update the values HERE after a calibration
// session; procedures + recorded history live in the per-sensor docs under
// docs/sensors/ (ec-tds.md, ph.md).

#pragma once

#include <Arduino.h>

// EC: single-point against 1413 uS/cm fluid. 1.0 = uncalibrated.
const float EC_CAL_K = 1.0f;

// pH: two-point. Record the measured voltages in the buffers; NAN = not yet
// calibrated, readers report raw voltage only.
const float PH_V_NEUTRAL = NAN; // volts in pH 6.86 buffer
const float PH_V_ACID = NAN;    // volts in pH 4.01 buffer
