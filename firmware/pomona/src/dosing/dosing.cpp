// Pomona firmware — dosing bench module (Trello #284). See dosing.h.
//
// DFR0523 control is standard servo PPM (DFRobot's own examples drive it
// with the Servo library): 500-1400us forward (500 = max speed),
// 1400-1600us stop, 1600-2500us reverse (2500 = max reverse speed).

#include "dosing.h"

#include <Arduino.h>
#include <Servo.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../config.h"

static Servo ch[4];
static const int chPins[4] = {PIN_DOSE_CH1, PIN_DOSE_CH2, PIN_DOSE_CH3,
                              PIN_DOSE_CH4};
static int runningCh = -1; // 0-3 while a timed run is active
static unsigned long runUntil = 0;
static char lastEvent[96];
static bool eventPending = false;

static void setEvent(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(lastEvent, sizeof(lastEvent), fmt, ap);
  va_end(ap);
  eventPending = true;
}

static void stopAll() {
  for (int i = 0; i < 4; i++) ch[i].writeMicroseconds(1500);
  runningCh = -1;
}

void dosingInit() {
  for (int i = 0; i < 4; i++) {
    ch[i].attach(chPins[i]);
    ch[i].writeMicroseconds(1500); // explicit stop from the first pulse
  }
}

void dosingHandleCommand(const char *payload) {
  int n = 0;
  char dir[8] = "";
  long ms = 0;
  int speed = 100;
  int got = sscanf(payload, "ch%d %7s %ld %d", &n, dir, &ms, &speed);
  if (got < 2 || n < 1 || n > 4) {
    setEvent("bad command: %.60s", payload);
    return;
  }
  if (strcmp(dir, "stop") == 0) {
    stopAll();
    setEvent("ch%d stop", n);
    return;
  }
  if (ms < 1) ms = 1;
  if (ms > (long)DOSE_TEST_MAX_MS) ms = DOSE_TEST_MAX_MS; // hard cap
  if (speed < 1) speed = 1;
  if (speed > 100) speed = 100;

  int us;
  if (strcmp(dir, "fwd") == 0)
    us = 1400 - 9 * speed; // 100 -> 500us = max forward
  else if (strcmp(dir, "rev") == 0)
    us = 1600 + 9 * speed; // 100 -> 2500us = max reverse
  else {
    setEvent("bad direction: %.20s", dir);
    return;
  }

  stopAll(); // one channel at a time, always
  ch[n - 1].writeMicroseconds(us);
  runningCh = n - 1;
  runUntil = millis() + (unsigned long)ms;
  setEvent("ch%d %s %ldms speed%d", n, dir, ms, speed);
}

void dosingService() {
  if (runningCh >= 0 && (long)(millis() - runUntil) >= 0) {
    int done = runningCh;
    stopAll();
    setEvent("ch%d done", done + 1);
  }
}

const char *dosingTakeEvent() {
  if (!eventPending) return NULL;
  eventPending = false;
  return lastEvent;
}
