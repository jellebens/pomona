// Pomona firmware v1 — display module (Trello #229).
//
// LVGL current-readings screen on the GIGA Display Shield (800x480 touch).
// One tile per metric; unavailable sensors show "--"; firmware version in
// the bottom-right corner; WiFi + MQTT status icons top-right (red down /
// green up, live). Blanks the backlight after DISPLAY_BLANK_TIMEOUT_MS
// idle and wakes on touch WITHOUT the waking touch pressing any UI
// element (#248). History graphs are card #223.

#pragma once

#include "sensors.h"

void displayInit(); // bring up panel + touch + backlight + build the screen
void displayService(); // call every loop: lv_timer_handler + idle blanking
void displayUpdate(const Readings &r); // refresh the metric tiles
void displayLinkStatus(bool wifiUp, bool mqttUp); // call every loop
