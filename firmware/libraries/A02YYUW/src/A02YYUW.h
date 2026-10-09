// DFRobot A02YYUW (SEN0311) — waterproof ultrasonic distance sensor, UART.
//
// Mounted face-down above the reservoir, it measures the distance to the
// water surface (30..4500 mm; below 30 mm it is blind). The sensor streams
// 4-byte frames at 9600 8N1 with no request needed when its RX (yellow) wire
// is left floating — the "processed value" mode, one frame every ~100-300 ms:
//
//   0xFF  DATA_H  DATA_L  SUM     distance_mm = DATA_H * 256 + DATA_L
//                                 SUM = (0xFF + DATA_H + DATA_L) & 0xFF
//
// poll() is non-blocking: call it every loop() so the UART buffer never
// overflows, and read distanceMm() whenever a value is wanted. A slosh or an
// echo off the tower wall shows up as a single wild frame, so the value is
// the median of the last WINDOW good frames. Wiring: docs/sensors/level-sonic.md.

#pragma once

#include <Arduino.h>

class A02YYUW {
public:
  static const uint8_t WINDOW = 7;       // frames in the median (~1-2 s of data)
  static const uint16_t MIN_MM = 30;     // the sensor's blind zone
  static const uint16_t MAX_MM = 4500;   // datasheet range
  static const uint32_t STALE_MS = 3000; // no good frame this long -> absent

  explicit A02YYUW(HardwareSerial &port) : _port(port) {}

  void begin() { _port.begin(9600); }

  // Drain whatever bytes have arrived and keep every valid frame. Cheap.
  void poll();

  // Median of the recent good frames in mm, or -1 when the sensor is absent
  // (unplugged, unpowered) or has sent nothing usable for STALE_MS.
  int distanceMm() const;

  // Running counters, for the bench / diagnostics.
  uint32_t goodFrames() const { return _good; }
  uint32_t badFrames() const { return _bad; }

  // Frame check, exposed for tests: the distance in mm, or -1 when the
  // checksum fails or the value is outside MIN_MM..MAX_MM.
  static int decode(uint8_t h, uint8_t l, uint8_t sum);

private:
  HardwareSerial &_port;
  uint8_t _frame[4] = {0};
  uint8_t _pos = 0;
  uint16_t _ring[WINDOW] = {0};
  uint8_t _count = 0; // valid entries in _ring (saturates at WINDOW)
  uint8_t _next = 0;
  uint32_t _lastGoodMs = 0;
  uint32_t _good = 0;
  uint32_t _bad = 0;
};
