// Pomona firmware — dosing bench module (Trello #284). See dosing.h.
//
// DFR0523 control is standard servo PPM: 500-1400us forward (500 = max
// speed), 1400-1600us stop, 1600-2500us reverse (2500 = max reverse).
//
// Pulses are generated with mbed PwmOut directly (explicit 20 ms period,
// pulsewidth in us). The Arduino Servo library was tried first and left the
// pin near-constantly HIGH on the GIGA (measured ~3 V average on D4 where a
// servo signal averages 0.1-0.4 V) — bench session 2026-09-03.

#include "dosing.h"

#include <Arduino.h>
#include <mbed.h>
#include <pinDefinitions.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../config.h"

static mbed::PwmOut *ch[4] = {nullptr, nullptr, nullptr, nullptr};
static const int chPins[4] = {PIN_DOSE_CH1, PIN_DOSE_CH2, PIN_DOSE_CH3,
                              PIN_DOSE_CH4};
static int runningCh = -1; // 0-3 while a timed run is active
static unsigned long runUntil = 0;
static char lastEvent[96];
static bool eventPending = false;
static int lastUs[4] = {1472, 1472, 1472, 1472};

static void setEvent(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(lastEvent, sizeof(lastEvent), fmt, ap);
  va_end(ap);
  eventPending = true;
  // Serial mirror of pomona/dose/result, tagged per channel: [PUMP001]..[PUMP004]
  if (lastEvent[0] == 'c' && lastEvent[1] == 'h' && lastEvent[2] >= '1' &&
      lastEvent[2] <= '4') {
    Serial.print("[PUMP00");
    Serial.print(lastEvent[2]);
    Serial.print("] ");
  } else {
    Serial.print("[PUMP] ");
  }
  Serial.println(lastEvent);
}

static void writeUs(int i, int us) {
  if (ch[i]) ch[i]->pulsewidth_us(us);
  lastUs[i] = us;
}

static void stopAll() {
  for (int i = 0; i < 4; i++) writeUs(i, 1472);
  runningCh = -1;
}

void dosingInit() {
  for (int i = 0; i < 4; i++) {
    ch[i] = new mbed::PwmOut(digitalPinToPinName(chPins[i]));
    ch[i]->period_ms(20);       // standard 50 Hz servo frame
    ch[i]->pulsewidth_us(1472); // DFRobot stop value from the first pulse
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

  // Pulse values copied from DFRobot's own GravityPump library (it calls
  // Servo.attach(pin) + write(0..180) with defaults): write(0)=544us max
  // forward, write(90)=1472us stop, write(180)=2400us max reverse @ 50 Hz.
  int us;
  if (strcmp(dir, "fwd") == 0)
    us = 1472 - (speed * 928) / 100; // 100 -> 544us = DFRobot max forward
  else if (strcmp(dir, "rev") == 0)
    us = 1472 + (speed * 928) / 100; // 100 -> 2400us = DFRobot max reverse
  else {
    setEvent("bad direction: %.20s", dir);
    return;
  }

  stopAll(); // one channel at a time, always
  writeUs(n - 1, us);
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

void dosingDebugStatus() {
  Serial.println("dose status:");
  for (int i = 0; i < 4; i++) {
    Serial.print("  ch");
    Serial.print(i + 1);
    Serial.print(" pin D");
    Serial.print(chPins[i]);
    Serial.print(" pulse ");
    Serial.print(lastUs[i]);
    Serial.print("us");
    Serial.println(i == runningCh ? "  <RUNNING>" : "");
  }
}
