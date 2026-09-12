// Pomona firmware — a tiny flat-JSON reader (v2 wire, #295).
//
// The v2 documents the node READS are small and flat at the keys it needs
// (dose/request: id, reagent, ml, rate; desired: stage). A full JSON library
// is not worth its flash and heap for that; these helpers find `"key":` and
// read the value that follows. Nested objects are skipped only in the sense
// that a key inside them is found too — the documents we parse have unique
// key names at the level we care about. Not a general parser: do not reuse
// for anything adversarial.

#pragma once

#include <stdlib.h>
#include <string.h>

// Copy the string value of `key` into out (NUL-terminated, truncated to n-1).
// Returns false when the key is missing or its value is not a string.
static inline bool jsonGetString(const char *json, const char *key, char *out, size_t n) {
  if (!json || !key || !out || n == 0) return false;
  char needle[48];
  snprintf(needle, sizeof(needle), "\"%s\"", key);
  const char *p = strstr(json, needle);
  if (!p) return false;
  p += strlen(needle);
  while (*p == ' ' || *p == ':' || *p == '\t') p++;
  if (*p != '"') return false;
  p++;
  size_t i = 0;
  while (*p && *p != '"' && i < n - 1) {
    if (*p == '\\' && p[1]) p++; // keep the escaped char, drop the backslash
    out[i++] = *p++;
  }
  out[i] = '\0';
  return true;
}

// Read the numeric value of `key`. Returns false when missing or not a number.
static inline bool jsonGetNumber(const char *json, const char *key, float *out) {
  if (!json || !key || !out) return false;
  char needle[48];
  snprintf(needle, sizeof(needle), "\"%s\"", key);
  const char *p = strstr(json, needle);
  if (!p) return false;
  p += strlen(needle);
  while (*p == ' ' || *p == ':' || *p == '\t') p++;
  if (!(*p == '-' || *p == '+' || *p == '.' || (*p >= '0' && *p <= '9'))) return false;
  char *end = nullptr;
  float v = strtof(p, &end);
  if (end == p) return false;
  *out = v;
  return true;
}
