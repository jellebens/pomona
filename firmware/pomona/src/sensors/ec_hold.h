// EC read between pump runs (pomona 2.4.4, ceres ADR-0020).
//
// The tower's EC probe reads the pump: with the pump running the TDS board reads high, more so
// the longer the pump has run since a reset of the condition (2026-10-06/07: 1.43 off -> 1.81 on;
// 2026-10-15/16: 1.63 off -> 1.96 on), the step within seconds of each switch; pH (behind a
// DFR0504 isolator) is unaffected. Most likely stray current from the pump into the water through
// the un-isolated EC circuit's ground. Until the EC probe gets an isolator, the node measures EC
// only while the pump has been OFF for EC_PUMP_SETTLE_MS and holds that value through the pump
// run and the settle that follows. The live reading is still published (tele/water/ec_live_ms_cm).
//
// If the pump runs longer than EC_HOLD_MAX_MS (an override, a long mix), a held value is too old
// to stand for the water: the live reading is published again. Header-only so the host test can
// include it. Since 2.4.6 the tank level uses the same hold (its own settle: the tower drains back).

#pragma once

#include <math.h>
#include <stdint.h>

class EcHold {
public:
  EcHold(uint32_t settleMs, uint32_t maxHoldMs) : _settleMs(settleMs), _maxHoldMs(maxHoldMs) {}

  // The EC to publish for this sweep; *held = true when it is the last pump-off value.
  float apply(float live, bool pumpOn, uint32_t nowMs, bool *held) {
    if (!_known || pumpOn != _pumpOn) {
      _pumpOn = pumpOn;
      _sinceMs = nowMs;
      _known = true;
    }
    if (!pumpOn && nowMs - _sinceMs >= _settleMs && !isnan(live)) {
      _held = live;
      _heldAtMs = nowMs;
      *held = false;
      return live;
    }
    if (!isnan(_held) && nowMs - _heldAtMs <= _maxHoldMs) {
      *held = true;
      return _held;
    }
    *held = false; // nothing recent to hold (boot, or a pump run longer than the hold): the live value
    return live;
  }

private:
  uint32_t _settleMs, _maxHoldMs;
  bool _known = false, _pumpOn = false;
  uint32_t _sinceMs = 0, _heldAtMs = 0;
  float _held = NAN;
};
