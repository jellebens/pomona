// Grove 10 cm capacitive water level strip (Seeed) — Pomona sensor unit.
//
// The strip is two I2C targets: 0x77 serves the low 8 pads (bottom of the
// strip), 0x78 the high 12 pads. Each pad returns one raw byte; a value
// above the wet threshold means water. 20 pads over 10 cm = 0.5 cm/pad.
// Wiring + calibration procedure: docs/wiring.md.

#pragma once

#include <Arduino.h>
#include <Wire.h>

class GroveWaterLevel {
public:
  static const uint8_t PAD_COUNT = 20;
  static const uint8_t ADDR_LOW = 0x77;  // low 8 pads (bottom)
  static const uint8_t ADDR_HIGH = 0x78; // high 12 pads (top)
  static const uint8_t DEFAULT_WET_THRESHOLD = 100; // Seeed reference; tune per mount

  explicit GroveWaterLevel(TwoWire &wire = Wire,
                           uint8_t wetThreshold = DEFAULT_WET_THRESHOLD);

  // Refresh all pad values over I2C. Returns false (and ok() goes false) on
  // a bus error; previous raw values are then stale.
  bool read();

  bool ok() const { return _ok; }

  // Raw pad value, pad 0 = bottom … 19 = top. 0 for out-of-range pads.
  uint8_t raw(uint8_t pad) const;
  bool wet(uint8_t pad) const { return raw(pad) > _threshold; }

  uint8_t wetCount() const;
  uint32_t bitmap() const; // bit 0 = bottom pad
  int percent() const;     // 0..100 (5 %/pad), -1 if the last read failed
  float depthCm() const;   // wetCount * 0.5 cm, NAN if the last read failed

  void setThreshold(uint8_t t) { _threshold = t; }
  uint8_t threshold() const { return _threshold; }

private:
  bool readBank(uint8_t addr, uint8_t *buf, uint8_t n);

  TwoWire &_wire;
  uint8_t _threshold;
  uint8_t _raw[PAD_COUNT] = {0};
  bool _ok = false;
};
