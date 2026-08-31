// Pomona firmware — control module implementation (Trello #260).
//
// See control.h for the contract and docs/control-architecture.md for why the
// decision lives here rather than in Home Assistant.

#include "control.h"

#include <string.h> // strcasecmp

#include "../../config.h"
#include "../network/network.h"  // networkEpochNow() — 0 when the clock is unknown

// ---- state -----------------------------------------------------------

static bool pumpOn = false;
static bool lightOn = false;
static PumpReason reason = PumpReason::BootSafe;
static bool dirty = true;  // publish the boot-safe state as soon as we can

// duty cycle
static uint32_t cycleStartMs = 0;
static bool cycleOnPhase = false;

// override / mode
enum class Override : uint8_t { Auto, ForceOn, ForceOff };
static Override overrideMode = Override::Auto;
static bool establishment = true;  // safer default for young transplants

// level interlock state machine
enum class Level : uint8_t { Normal, LowPending, Settling, ConfirmedLow };
static Level levelState = Level::Normal;
static uint32_t levelStateSinceMs = 0;
static uint32_t lastSettleCheckMs = 0;
static bool settleCheckEverRun = false;

static void setPump(bool on, PumpReason why) {
  if (pumpOn != on || reason != why) dirty = true;
  pumpOn = on;
  reason = why;
}

static void setLight(bool on) {
  if (lightOn != on) dirty = true;
  lightOn = on;
}

void controlInit() {
  // Safe state, set before WiFi, before I2C, before the display. A rebooting
  // unit must be in a known state before anything that can hang runs.
  pumpOn = false;
  lightOn = false;
  reason = PumpReason::BootSafe;
  dirty = true;
  cycleStartMs = millis();
  cycleOnPhase = false;
  levelState = Level::Normal;
  levelStateSinceMs = millis();
}

// ---- photoperiod -----------------------------------------------------

// Local seconds-since-midnight, or -1 when the clock has never been set.
// TZ is a fixed offset (config.h): an hour of DST error is irrelevant to a
// 14 h photoperiod and not worth carrying EU DST rules in firmware for.
static int32_t localSecondsOfDay() {
  uint32_t epoch = networkEpochNow();
  if (epoch == 0) return -1;
  int32_t local = (int32_t)((epoch + (uint32_t)(TZ_OFFSET_MINUTES * 60)) % 86400UL);
  return local;
}

static void serviceLight() {
  int32_t sod = localSecondsOfDay();
  if (sod < 0) {
    // No clock. Hold the light off — see the header for why off is the safe
    // failure. The pump is unaffected; it never consults the clock.
    if (lightOn) dirty = true;
    lightOn = false;
    return;
  }
  const int32_t on = (establishment ? LIGHT_ON_ESTABLISHMENT_H : LIGHT_ON_ESTABLISHED_H) * 3600;
  const int32_t off = LIGHT_OFF_H * 3600;
  setLight(sod >= on && sod < off);
}

// ---- level interlock + settle check ----------------------------------

// The probe is a TOP-UP gauge (docs/sensors/level-probe.md): 0 points means
// "below 8.2 L" and it is blind beneath that. It also reads low DURING a pump
// cycle because the tower holds water in transit. So a raw 0 is not actionable
// on its own — stop, let it drain back, then believe the reading.
//
// probePoints < 0 means no signal at all. That is unknown, not empty: fail
// open, keep watering, never inhibit on it.
static void serviceLevel(const Readings &r) {
  const uint32_t now = millis();
  const bool low = (r.probePoints == 0);
  const bool unknown = (r.probePoints < 0);

  if (unknown) {
    // Fail open. Drop any pending suspicion but leave a confirmed state alone:
    // losing the sensor should not silently clear a genuine low.
    if (levelState == Level::LowPending || levelState == Level::Settling) {
      levelState = Level::Normal;
      levelStateSinceMs = now;
    }
    return;
  }

  switch (levelState) {
    case Level::Normal:
      if (low) {
        levelState = Level::LowPending;
        levelStateSinceMs = now;
      }
      break;

    case Level::LowPending:
      if (!low) {
        levelState = Level::Normal;
        levelStateSinceMs = now;
      } else if (now - levelStateSinceMs >= LEVEL_LOW_DEBOUNCE_MS) {
        // Rate-limit the settle checks so a genuinely low tank cannot thrash
        // the pump on and off all day.
        if (!settleCheckEverRun || now - lastSettleCheckMs >= SETTLE_MIN_INTERVAL_MS) {
          levelState = Level::Settling;
          levelStateSinceMs = now;
          lastSettleCheckMs = now;
          settleCheckEverRun = true;
        }
      }
      break;

    case Level::Settling:
      if (now - levelStateSinceMs >= SETTLE_WAIT_MS) {
        if (low) {
          levelState = Level::ConfirmedLow;  // settled and still low: real
        } else {
          levelState = Level::Normal;  // it was water in transit
        }
        levelStateSinceMs = now;
      }
      break;

    case Level::ConfirmedLow:
      if (!low) {
        levelState = Level::Normal;
        levelStateSinceMs = now;
      }
      break;
  }
}

// Inhibit only after a CONFIRMED low has persisted long enough that "nobody
// topped up" is the better explanation than "it is nearly time to top up".
static bool levelInhibits() {
  return levelState == Level::ConfirmedLow &&
         (millis() - levelStateSinceMs) >= LEVEL_INHIBIT_AFTER_MS;
}

// ---- duty cycle ------------------------------------------------------

static uint32_t currentOffMs() {
  if (establishment) return PUMP_OFF_ESTABLISHMENT_MS;
  int32_t sod = localSecondsOfDay();
  // Unknown clock -> assume day, i.e. water more often. Erring wet is
  // recoverable; erring dry is not.
  if (sod < 0) return PUMP_OFF_DAY_MS;
  const int32_t on = LIGHT_ON_ESTABLISHED_H * 3600;
  const int32_t off = LIGHT_OFF_H * 3600;
  return (sod >= on && sod < off) ? PUMP_OFF_DAY_MS : PUMP_OFF_NIGHT_MS;
}

static void servicePump() {
  // Priority order: interlock, then override, then the duty cycle. The
  // interlock is first on purpose — an override must never be able to run the
  // pump while the level is confirmed low. Firmware keeps the veto.
  if (levelState == Level::Settling) {
    setPump(false, PumpReason::Settling);
    return;
  }
  if (levelInhibits()) {
    setPump(false, PumpReason::LevelLow);
    return;
  }
  if (overrideMode == Override::ForceOff) {
    setPump(false, PumpReason::Override);
    return;
  }
  if (overrideMode == Override::ForceOn) {
    setPump(true, PumpReason::Override);
    return;
  }

  const uint32_t now = millis();
  const uint32_t elapsed = now - cycleStartMs;
  if (cycleOnPhase) {
    if (elapsed >= PUMP_ON_MS) {
      cycleOnPhase = false;
      cycleStartMs = now;
    }
  } else {
    if (elapsed >= currentOffMs()) {
      cycleOnPhase = true;
      cycleStartMs = now;
    }
  }
  setPump(cycleOnPhase, PumpReason::Schedule);
}

// ---- public ----------------------------------------------------------

void controlService(const Readings &r) {
  serviceLevel(r);
  servicePump();
  serviceLight();
}

bool controlPumpOn() { return pumpOn; }
bool controlLightOn() { return lightOn; }

const char *controlReasonStr() {
  switch (reason) {
    case PumpReason::BootSafe: return "boot_safe";
    case PumpReason::Schedule: return "schedule";
    case PumpReason::Settling: return "settling";
    case PumpReason::LevelLow: return "level_low";
    case PumpReason::Override: return "override";
  }
  return "unknown";
}

bool controlWantsPublish() { return dirty; }
void controlMarkPublished() { dirty = false; }
void controlForcePublish() { dirty = true; }

void controlSetOverride(const char *payload) {
  if (!payload) return;
  if (strcasecmp(payload, "auto") == 0) overrideMode = Override::Auto;
  else if (strcasecmp(payload, "on") == 0) overrideMode = Override::ForceOn;
  else if (strcasecmp(payload, "off") == 0) overrideMode = Override::ForceOff;
  else return;  // unknown payload: ignore rather than guess
  dirty = true;
}

void controlSetMode(const char *payload) {
  if (!payload) return;
  if (strcasecmp(payload, "establishment") == 0) establishment = true;
  else if (strcasecmp(payload, "established") == 0) establishment = false;
  else return;
  dirty = true;
}
