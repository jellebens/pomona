// Pomona firmware v1 — display module (Trello #229).
//
// LVGL current-readings screen on the GIGA Display Shield (800x480 touch).
// One tile per metric; unavailable sensors show "--"; firmware version in
// the bottom-right corner; WiFi + MQTT status icons top-right (red down /
// green up, live). Blanks the backlight after DISPLAY_BLANK_TIMEOUT_MS
// idle and wakes on touch WITHOUT the waking touch pressing any UI
// element (#248). History graphs are card #223.

#pragma once

#include "../sensors/sensors.h"

void displayInit(); // panel + touch + backlight + BOOT SCREEN (visible at once)
void displayBootStatus(const char *line); // update the boot status line (renders now)
void displayShowMain(); // leave the boot screen, build + show the tiles UI
void displayService(); // call every loop: lv_timer_handler + idle blanking
void displayUpdate(const Readings &r); // refresh the metric tiles
void displayLinkStatus(bool wifiUp, bool mqttUp); // call every loop

// OTA progress view: full-screen "Updating firmware" with stage + countdown
// (the OTA blocks the loop, so these render synchronously when called).
void displayOtaScreen(const char *stage); // create the view / update the stage line
void displayOtaTick(uint32_t elapsedS, int remainingEstS); // countdown line
void displayRestoreMain(); // back to the tiles with the cached readings
