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
  float ecMsCm = NAN;  // analog — no absence detection, reads ~0 V unplugged; the held value since 2.4.4
  float ecLiveMsCm = NAN; // this sweep's reading, pump on or off (2.4.4)
  bool ecHeld = false;    // ecMsCm is the last pump-off value, held through a pump run (2.4.4)
  float ecRawV = NAN;
  bool phOk = false;   // false until PH_V_* calibration is recorded
  float ph = NAN;
  float phRawV = NAN;
  bool levelOk = false; // Grove strip is optional hardware
  int levelPct = -1;
  int probePoints = -1; // CQRSENYW003: 0 dry .. 4, -1 = no signal
  bool tankDistOk = false; // A02YYUW answered (#283)
  int tankDistMm = -1;     // sensor face -> water surface, this sweep's median (live again since 2.4.8)
  int tankDistLiveMm = -1; // the same reading — kept for the level_distance_live_mm topic (2.4.6)
  bool tankHeld = false;   // always false since 2.4.8 (the level hold is gone); sys/health.level_held
  bool tankCalibrated = false; // the fill table (PomonaCalibration TANK_TABLE) is set
  bool tankOk = false;     // ...and the reading is the water surface: plausible + steady (2.4.3)
  const char *tankState = "absent"; // ok|splash|no_echo|unsteady|near|absent|uncalibrated (sys/health)
  float tankPct = NAN;     // tankL as a share of TANK_FULL_L
  float tankL = NAN;       // litres from the fill table
  uint32_t sonicGood = 0, sonicBad = 0, sonicNear = 0; // A02YYUW frame counters since boot (sys/health)
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
void sensorsPoll(); // every loop(): drain the A02YYUW UART (non-blocking)

// Rescan the I2C bus and write the found addresses into out as a comma
// list ("0x23,0x76"; empty string = nothing found). Returns the count.
// Cheap (~ms) — used for the remote diagnostics topic (docs/mqtt.md).
int sensorsI2CScan(char *out, size_t outLen);
