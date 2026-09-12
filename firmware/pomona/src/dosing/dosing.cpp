// Pomona firmware — dosing module implementation. See dosing.h.
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

#include <PomonaCalibration.h>

#include "../../config.h"
#include "../util/json.h"

static mbed::PwmOut *ch[4] = {nullptr, nullptr, nullptr, nullptr};
static const int chPins[4] = {PIN_DOSE_CH1, PIN_DOSE_CH2, PIN_DOSE_CH3,
                              PIN_DOSE_CH4};
static int runningCh = -1; // 0-3 while a timed run is active
static unsigned long runUntil = 0;
static unsigned long runStarted = 0;
static char lastEvent[224];
static bool eventPending = false;
static int lastUs[4] = {1472, 1472, 1472, 1472};

// The request being executed (v2): its ack is published when the run ends.
static char activeId[48] = "";
static char activeReagent[16] = "";
static float activeMl = 0.0f;
static uint32_t activeEpoch = 0;

// Rolling 24 h budget per channel: a small ring of (millis, ml).
struct DoseLog { uint32_t ms; float ml; };
static const int LOG_N = 32;
static DoseLog doseLog[4][LOG_N];
static int logHead[4] = {0, 0, 0, 0};
static const uint32_t DAY_MS = 24UL * 60UL * 60UL * 1000UL;

static float ml24h(int i) {
  const uint32_t now = millis();
  float sum = 0.0f;
  for (int k = 0; k < LOG_N; k++)
    if (doseLog[i][k].ml > 0.0f && (uint32_t)(now - doseLog[i][k].ms) < DAY_MS) sum += doseLog[i][k].ml;
  return sum;
}

static void logDose(int i, float ml) {
  doseLog[i][logHead[i]] = {millis(), ml};
  logHead[i] = (logHead[i] + 1) % LOG_N;
}

static void setEventRaw(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(lastEvent, sizeof(lastEvent), fmt, ap);
  va_end(ap);
  eventPending = true;
  Serial.print("[DOSE] ");
  Serial.println(lastEvent);
}

// A bench event: JSON WITHOUT an id (ADR-0008 rule 4: foreign to Demeter).
static void setBenchEvent(int n, const char *status, const char *detail, long ms) {
  setEventRaw("{\"status\":\"%s\",\"channel\":%d,\"ms\":%ld,\"reason\":\"bench %s\",\"ts\":0}", status, n, ms, detail);
}

// The ack of a v2 request.
static void setAck(const char *id, const char *reagent, const char *status, float ml, long ms, int channel,
                   const char *reason, uint32_t epoch) {
  setEventRaw("{\"id\":\"%s\",\"reagent\":\"%s\",\"status\":\"%s\",\"ml\":%.3f,\"ms\":%ld,\"channel\":%d,\"reason\":\"%s\",\"ts\":%lu}",
              id, reagent, status, (double)ml, ms, channel, reason, (unsigned long)epoch);
}

static void writeUs(int i, int us) {
  if (ch[i]) ch[i]->pulsewidth_us(us);
  lastUs[i] = us;
}

static void stopAll() {
  for (int i = 0; i < 4; i++) writeUs(i, 1472);
  runningCh = -1;
}

// Pulse values copied from DFRobot's own GravityPump library (it calls
// Servo.attach(pin) + write(0..180) with defaults): write(0)=544us max
// forward, write(90)=1472us stop, write(180)=2400us max reverse @ 50 Hz.
static int forwardUs(int speed) { return 1472 - (speed * 928) / 100; }
static int reverseUs(int speed) { return 1472 + (speed * 928) / 100; }

static void startRun(int i, int us, unsigned long ms) {
  stopAll(); // one channel at a time, always
  writeUs(i, us);
  runningCh = i;
  runStarted = millis();
  runUntil = runStarted + ms;
}

void dosingInit() {
  for (int i = 0; i < 4; i++) {
    ch[i] = new mbed::PwmOut(digitalPinToPinName(chPins[i]));
    ch[i]->period_ms(20);       // standard 50 Hz servo frame
    ch[i]->pulsewidth_us(1472); // DFRobot stop value from the first pulse
  }
  memset(doseLog, 0, sizeof(doseLog));
}

// ---- the bench channel (Serial only since 2.0.0) -----------------------

void dosingHandleCommand(const char *payload) {
  int n = 0;
  char dir[8] = "";
  long ms = 0;
  int speed = 100;
  int got = sscanf(payload, "ch%d %7s %ld %d", &n, dir, &ms, &speed);
  if (got < 2 || n < 1 || n > 4) {
    setEventRaw("{\"status\":\"refused\",\"reason\":\"bench bad command\",\"ts\":0}");
    return;
  }
  if (strcmp(dir, "stop") == 0) {
    stopAll();
    activeId[0] = '\0';
    setBenchEvent(n, "done", "stop", 0);
    return;
  }
  if (ms < 1) ms = 1;
  if (ms > (long)DOSE_TEST_MAX_MS) ms = DOSE_TEST_MAX_MS; // hard cap
  if (speed < 1) speed = 1;
  if (speed > 100) speed = 100;
  int us;
  if (strcmp(dir, "fwd") == 0) us = forwardUs(speed);
  else if (strcmp(dir, "rev") == 0) us = reverseUs(speed);
  else {
    setEventRaw("{\"status\":\"refused\",\"reason\":\"bench bad direction\",\"ts\":0}");
    return;
  }
  activeId[0] = '\0'; // a bench run acks without an id
  startRun(n - 1, us, (unsigned long)ms);
  setBenchEvent(n, "started", dir, ms);
}

// ---- the v2 request ----------------------------------------------------

static int channelOf(const char *reagent) {
  for (int i = 0; i < 4; i++)
    if (DOSER_CAL[i].reagent[0] && strcmp(DOSER_CAL[i].reagent, reagent) == 0) return i;
  return -1;
}

void dosingHandleRequest(const char *json, uint32_t epochNow) {
  char id[48] = "";
  char reagent[16] = "";
  char rate[8] = "full";
  float ml = 0.0f;
  if (!jsonGetString(json, "id", id, sizeof(id)) || id[0] == '\0') {
    setEventRaw("{\"status\":\"refused\",\"reason\":\"request without id\",\"ts\":%lu}", (unsigned long)epochNow);
    return;
  }
  if (!jsonGetString(json, "reagent", reagent, sizeof(reagent)) || !jsonGetNumber(json, "ml", &ml)) {
    setAck(id, reagent, "refused", ml, 0, 0, "malformed request", epochNow);
    return;
  }
  jsonGetString(json, "rate", rate, sizeof(rate));
  const int i = channelOf(reagent);
  if (i < 0) {
    setAck(id, reagent, "refused", ml, 0, 0, "unknown reagent", epochNow);
    return;
  }
  const int channel = i + 1;
  if (!(ml > 0.0f)) {
    setAck(id, reagent, "refused", ml, 0, channel, "ml must be positive", epochNow);
    return;
  }
  if (ml > DOSE_MAX_ML_PER_CMD[i]) {
    char why[48];
    snprintf(why, sizeof(why), "over per-command cap %.1f ml", (double)DOSE_MAX_ML_PER_CMD[i]);
    setAck(id, reagent, "refused", ml, 0, channel, why, epochNow);
    return;
  }
  if (ml24h(i) + ml > DOSE_MAX_ML_PER_24H[i]) {
    char why[64];
    snprintf(why, sizeof(why), "over 24h cap %.1f ml (%.1f used)", (double)DOSE_MAX_ML_PER_24H[i], (double)ml24h(i));
    setAck(id, reagent, "refused", ml, 0, channel, why, epochNow);
    return;
  }
  if (runningCh >= 0) {
    setAck(id, reagent, "refused", ml, 0, channel, "another channel is running", epochNow);
    return;
  }
  const bool slow = strcmp(rate, "slow") == 0;
  const float mlS = slow ? DOSER_CAL[i].slowMlS : DOSER_CAL[i].fullMlS;
  if (!(mlS > 0.0f)) {
    setAck(id, reagent, "refused", ml, 0, channel, "channel not calibrated", epochNow);
    return;
  }
  unsigned long ms = (unsigned long)(ml / mlS * 1000.0f + 0.5f);
  if (ms > DOSE_MAX_MS) {
    setAck(id, reagent, "refused", ml, (long)ms, channel, "over absolute ms cap", epochNow);
    return;
  }
  if (ms < 1) ms = 1;
  snprintf(activeId, sizeof(activeId), "%s", id);
  snprintf(activeReagent, sizeof(activeReagent), "%s", reagent);
  activeMl = ml;
  activeEpoch = epochNow;
  logDose(i, ml); // count it when it STARTS: a crash mid-dose over-counts, never under
  startRun(i, forwardUs(slow ? DOSER_CAL[i].slowSpeed : 100), ms);
  Serial.print("[DOSE] request ");
  Serial.print(id);
  Serial.print(": ");
  Serial.print(reagent);
  Serial.print(" ");
  Serial.print(ml, 2);
  Serial.print(" ml -> ch");
  Serial.print(channel);
  Serial.print(" ");
  Serial.print(ms);
  Serial.println(" ms");
}

void dosingService() {
  if (runningCh >= 0 && (long)(millis() - runUntil) >= 0) {
    const int done = runningCh;
    const long ran = (long)(millis() - runStarted);
    stopAll();
    if (activeId[0]) {
      setAck(activeId, activeReagent, "done", activeMl, ran, done + 1, "", activeEpoch);
      activeId[0] = '\0';
    } else {
      setBenchEvent(done + 1, "done", "run", ran);
    }
  }
}

const char *dosingTakeEvent() {
  if (!eventPending) return NULL;
  eventPending = false;
  return lastEvent;
}

size_t dosingMetaJson(char *out, size_t n) {
  size_t w = 0;
  w += snprintf(out + w, n - w, "\"dosers\":{");
  bool first = true;
  for (int i = 0; i < 4 && w < n; i++) {
    if (!DOSER_CAL[i].reagent[0]) continue;
    w += snprintf(out + w, n - w,
                  "%s\"%s\":{\"channel\":%d,\"ml_s_full\":%.2f,\"ml_s_slow\":%.2f,\"slow_speed\":%d,"
                  "\"max_ml_per_cmd\":%.1f,\"max_ml_per_24h\":%.1f}",
                  first ? "" : ",", DOSER_CAL[i].reagent, i + 1, (double)DOSER_CAL[i].fullMlS,
                  (double)DOSER_CAL[i].slowMlS, DOSER_CAL[i].slowSpeed, (double)DOSE_MAX_ML_PER_CMD[i],
                  (double)DOSE_MAX_ML_PER_24H[i]);
    first = false;
  }
  if (w < n) w += snprintf(out + w, n - w, "}");
  return w;
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
    Serial.print("us  24h ");
    Serial.print(ml24h(i), 2);
    Serial.print("/");
    Serial.print(DOSE_MAX_ML_PER_24H[i], 1);
    Serial.print(" ml");
    Serial.println(i == runningCh ? "  <RUNNING>" : "");
  }
}
