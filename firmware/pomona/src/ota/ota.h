// Pomona firmware v1 — OTA module (basic slice of Trello #243).
//
// OTA re-flash over WiFi via Arduino_Portenta_OTA (works on the GIGA):
// a .ota image is downloaded over http(s) into the QSPI OTA partition,
// verified, and applied by the bootloader after reset. Triggered by an
// MQTT message on pomona/unit/ota_url (network.cpp). The classic
// ArduinoOTA "network port" upload does NOT support the GIGA — see
// docs/ota-and-secrets.md; the version-endpoint pull automation stays
// on #243.
//
// ⚠ One-time USB prereqs before this works on hardware (owner-run, #243):
// bootloader update (STM32H747_System -> STM32H747_manageBootloader) and
// QSPI partitioning (STM32H747_System -> QSPIFormat).

#pragma once

#include <Arduino.h>

void otaInit(); // capability probe (bootloader version), serial log only

// Download + stage + apply `url` (blocking; kicks the watchdog between
// steps). On success the unit REBOOTS and this never returns. On failure
// returns false and writes a short reason into err.
bool otaApplyFromUrl(const char *url, char *err, size_t errLen);
