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
// the median of the last WINDOW good frames — after dropping the DOUBLE ECHOES:
// over a flat surface the pulse can bounce water -> sensor face -> water and
// arrive at twice the distance (2.4.0 on the tower: 320-355 mm for a 160-175 mm
// surface, often enough to win the median). Wiring: docs/sensors/level-sonic.md.

#pragma once

#include <Arduino.h>

class A02YYUW {
public:
  static const uint8_t WINDOW = 15;      // frames in the median (~2-3 s): wide enough that a
                                         // direct frame is there to unmask the echoes
  static const uint8_t ECHO_TOL_PCT = 10;    // a frame within 10 % of 2x another frame is its echo
                                             // (the tower's echoes scatter ~7 %; two real readings
                                             // never sit 2:1 apart within one window)
  static const uint8_t ECHO_TOL_MIN_MM = 10; // ...or within 10 mm, whichever is wider
  static const uint16_t MIN_MM = 30;     // the sensor's blind zone: closer reads as MIN_MM (full)
  static const uint16_t MAX_MM = 4500;   // datasheet range
  static const uint32_t STALE_MS = 3000; // no usable frame this long -> absent
  static const int NEAR = -2;            // decode(): a valid frame inside the blind zone
  static const int NO_ECHO = -3;         // decode(): a valid frame saying 0 mm / out of range

  explicit A02YYUW(HardwareSerial &port) : _port(port) {}

  void begin() { _port.begin(9600); }

  // Drain whatever bytes have arrived and keep every valid frame. Cheap.
  void poll();

  // Median of the recent good frames in mm, or -1 when the sensor is absent
  // (unplugged, unpowered) or has sent nothing usable for STALE_MS.
  int distanceMm() const;

  // Running counters since boot, published in sys/health so a silent sensor can be told
  // apart remotely: bad climbing = garbage on the line / no echo (0 mm, a tilted sensor);
  // near climbing = water at the face; nothing climbing = no frames at all (wiring, power).
  uint32_t goodFrames() const { return _good; }
  uint32_t badFrames() const { return _bad; }
  uint32_t nearFrames() const { return _near; }

  // Frame check, exposed for tests: the distance in mm; NEAR for a valid frame of
  // 1..MIN_MM-1 mm (blind zone); NO_ECHO for 0 mm or > MAX_MM; -1 for a bad checksum.
  static int decode(uint8_t h, uint8_t l, uint8_t sum);

  // True when `mm` is the double echo of a surface at `direct` mm (exposed for tests).
  static bool isEcho(uint16_t mm, uint16_t direct);

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
  uint32_t _near = 0;
};
