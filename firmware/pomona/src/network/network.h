// Pomona firmware v1 — network module (Trello #229).
//
// WiFi + MQTT with non-blocking reconnect: networkService() maintains the
// connection with exponential backoff (NET_RETRY_MIN_MS..MAX) and never
// spins — the screen keeps updating while the network is down. Publishes
// per docs/mqtt.md: only metrics whose sensor answered; availability goes
// to pomona/unit/sensors, online/offline to pomona/unit/status (LWT).

#pragma once

#include "../sensors/sensors.h"

void networkInit();
void networkService(); // call every loop: reconnect state machine + poll
void networkPublish(const Readings &r); // no-op unless MQTT is connected

bool wifiConnected();
bool mqttConnected();

// Publish the control decisions (pump/light request + reason) — see
// src/control and docs/mqtt.md. No-op unless MQTT is connected; the unit keeps
// acting on its own decisions regardless of whether anyone hears them.
void networkPublishControl();

// Current UNIX epoch from NTP, or 0 if the clock has never been set. Only the
// photoperiod consults this; the pump duty cycle is millis()-only by design.
uint32_t networkEpochNow();
