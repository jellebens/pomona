// CQRobot CQRSENYW003 — contact multi-point photoelectric liquid level probe.
//
// Four optical detection points over ~3 cm of probe height, meant to be
// immersed (Pomona: mounted at pump-intake height as the low-water ladder).
// Single green signal wire, open collector, encoding the highest wet point
// as a frequency: ~20 Hz dry, ~50/100/200/400 Hz for points 1..4.
// Wiring + mounting: docs/wiring.md.

#pragma once

#include <Arduino.h>

class PhotoLevelProbe {
public:
  // pin: any digital pin. The output is open collector, so the pin is
  // configured INPUT_PULLUP; no external resistor needed.
  explicit PhotoLevelProbe(pin_size_t pin) : _pin(pin) {}

  void begin() { pinMode(_pin, INPUT_PULLUP); }

  // Measure one output period. Returns frequency in Hz, or NAN when no
  // signal edges arrive within the timeout (sensor unpowered/disconnected).
  // Blocks up to ~2 periods (worst case ~250 ms at 20 Hz).
  float readFrequency();

  // Highest submerged detection point: 0 = dry, 1..4, or -1 for no signal.
  // Band edges sit at the geometric midpoints between nominal frequencies.
  int points();

  static int pointsFromFrequency(float hz);

private:
  pin_size_t _pin;
};
