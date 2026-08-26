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
