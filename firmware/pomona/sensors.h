// Pomona firmware v1 — sensor module (Trello #229).
//
// One sweep of every v1 sensor into a Readings struct. The *Ok flags say
// whether a value is real: absent/unreadable sensors keep their flag false,
// publish nothing (network.cpp) and show "--" on screen (display.cpp) —
// the unit never hangs on a missing sensor. Wiring: docs/wiring.md.

#pragma once

#include <Arduino.h>

struct Readings {
  // water
  bool waterTempOk = false;
  float waterTempC = NAN;
  float ecMsCm = NAN;  // analog — no absence detection, reads ~0 V unplugged
  float ecRawV = NAN;
  bool phOk = false;   // false until PH_V_* calibration is recorded
  float ph = NAN;
  float phRawV = NAN;
  bool levelOk = false; // Grove strip is optional hardware
  int levelPct = -1;
  int probePoints = -1; // CQRSENYW003: 0 dry .. 4, -1 = no signal
  // air
  bool bmeOk = false;
  float airTempC = NAN;
  float humidityPct = NAN;
  float pressureHpa = NAN;
  bool luxOk = false;
  float lux = NAN;
};

void sensorsInit(); // I2C scan + first init attempts (absent sensors OK)
void sensorsRead(Readings &r); // blocking sweep, worst case ~1.5 s
