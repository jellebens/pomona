// DFRobot A02YYUW — implementation. See A02YYUW.h for the frame format.

#include "A02YYUW.h"

int A02YYUW::decode(uint8_t h, uint8_t l, uint8_t sum) {
  if ((uint8_t)(0xFF + h + l) != sum) return -1;
  int mm = h * 256 + l;
  if (mm < MIN_MM || mm > MAX_MM) return -1; // blind zone / no echo
  return mm;
}

void A02YYUW::poll() {
  while (_port.available() > 0) {
    uint8_t b = (uint8_t)_port.read();
    if (_pos == 0) {
      if (b == 0xFF) _frame[_pos++] = b; // hunt for the header
      continue;
    }
    _frame[_pos++] = b;
    if (_pos < 4) continue;
    _pos = 0;
    int mm = decode(_frame[1], _frame[2], _frame[3]);
    if (mm < 0) {
      _bad++;
      // A data byte of 0xFF may have been a real header: resync on it.
      if (_frame[3] == 0xFF) _frame[_pos++] = 0xFF;
      continue;
    }
    _good++;
    uint32_t now = millis();
    if (now - _lastGoodMs > STALE_MS) _count = _next = 0; // back after a gap: forget the old water
    _lastGoodMs = now;
    _ring[_next] = (uint16_t)mm;
    _next = (_next + 1) % WINDOW;
    if (_count < WINDOW) _count++;
  }
}

int A02YYUW::distanceMm() const {
  if (_count == 0 || millis() - _lastGoodMs > STALE_MS) return -1;
  uint16_t s[WINDOW];
  for (uint8_t i = 0; i < _count; i++) s[i] = _ring[i];
  for (uint8_t i = 1; i < _count; i++) { // insertion sort, n <= 7
    uint16_t v = s[i];
    int8_t j = i - 1;
    while (j >= 0 && s[j] > v) {
      s[j + 1] = s[j];
      j--;
    }
    s[j + 1] = v;
  }
  return s[_count / 2];
}
