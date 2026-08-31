// Pomona firmware — control module (Trello #260).
//
// Decides what the pump and the grow light should be doing, locally, and
// publishes those decisions as requests that Home Assistant relays onto the
// Fibaro plugs (docs/mqtt.md "Control topics", docs/control-architecture.md).
//
// The point of this module is that the device holding the level probe is also
// the device that decides to stop the pump. Nothing here waits on the network:
// networkService() may never connect and the tower still waters correctly —
// publishing is *reporting*, not deciding.
//
// Two deliberate degradations:
//   * The PUMP duty cycle runs off millis() alone, so it is unaffected by a
//     missing clock. This is the survival-critical path and it has no
//     dependencies beyond the board being powered.
//   * The LIGHT photoperiod needs wall-clock time, and the only source is NTP
//     at first connect. With no time it holds the light OFF and reports
//     "no_time" — lights stuck off costs growth, lights stuck on at night
//     costs the plants their dark period, so off is the safe failure.

#pragma once

#include <Arduino.h>

#include "../sensors/sensors.h"

// Why the pump is in its current state. Published to pomona/pump/reason.
enum class PumpReason : uint8_t {
  BootSafe,  // set in controlInit(), before WiFi or sensors
  Schedule,  // normal duty cycle
  Settling,  // stopped on purpose, waiting for the tower to drain back
  LevelLow,  // settled reading confirmed low for 24 h -> inhibited
  Override,  // forced by pomona/pump/override
};

void controlInit();  // safe state. Call FIRST in setup(), before anything else.

// Call every loop with the latest sweep. Runs the duty cycle, the level
// settle check and the photoperiod. Cheap; no blocking.
void controlService(const Readings &r);

bool controlPumpOn();
bool controlLightOn();
const char *controlReasonStr();

// True when a decision changed since the last controlMarkPublished(), so the
// network layer knows to publish. Also forced true on connect (see
// controlForcePublish) because HA may have been driving in the meantime.
bool controlWantsPublish();
void controlMarkPublished();
void controlForcePublish();

// pomona/pump/override — "auto" | "on" | "off". Unknown payloads are ignored
// and leave the previous value in place. An override can never make the pump
// run while the level interlock is inhibiting it: firmware keeps the veto.
void controlSetOverride(const char *payload);

// pomona/control/mode — "establishment" | "established". Establishment is the
// wetter first-fortnight cycle; see docs/planting-plan.md. Defaults to
// establishment, which is the safer of the two for young transplants.
void controlSetMode(const char *payload);
