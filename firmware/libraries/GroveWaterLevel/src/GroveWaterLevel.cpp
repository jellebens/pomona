#include "GroveWaterLevel.h"

GroveWaterLevel::GroveWaterLevel(TwoWire &wire, uint8_t wetThreshold)
    : _wire(wire), _threshold(wetThreshold) {}

bool GroveWaterLevel::readBank(uint8_t addr, uint8_t *buf, uint8_t n) {
  _wire.requestFrom(addr, n);
  for (uint8_t i = 0; i < n; i++) {
    if (!_wire.available()) return false;
    buf[i] = _wire.read();
  }
  return true;
}

bool GroveWaterLevel::read() {
  _ok = readBank(ADDR_LOW, _raw, 8) && readBank(ADDR_HIGH, _raw + 8, 12);
  return _ok;
}

uint8_t GroveWaterLevel::raw(uint8_t pad) const {
  return pad < PAD_COUNT ? _raw[pad] : 0;
}

uint8_t GroveWaterLevel::wetCount() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < PAD_COUNT; i++) {
    if (wet(i)) n++;
  }
  return n;
}

uint32_t GroveWaterLevel::bitmap() const {
  uint32_t bits = 0;
  for (uint8_t i = 0; i < PAD_COUNT; i++) {
    if (wet(i)) bits |= 1UL << i;
  }
  return bits;
}

int GroveWaterLevel::percent() const {
  return _ok ? wetCount() * 5 : -1;
}

float GroveWaterLevel::depthCm() const {
  return _ok ? wetCount() * 0.5f : NAN;
}
