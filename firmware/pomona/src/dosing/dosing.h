// Pomona firmware — dosing module (Trello #284 bench, #224 contract, #295 v2 wire).
//
// Drives the DFR0523 peristaltic channels. Two entry points:
//
//   * dosingHandleRequest(json, epoch) — the v2 contract (demeter ADR-0008
//     rule 4): an ml-based request {"id","reagent","ml","rate"} from Demeter.
//     The node converts ml with ITS OWN calibration (PomonaCalibration.h
//     DOSER_CAL), enforces ITS OWN rails (per command + rolling 24 h ml caps in
//     config.h, one channel at a time, an absolute ms cap) and acks every
//     request on dose/result: {"id","reagent","status":"done|refused|failed",
//     "ml","ms","channel","reason","ts"}. The unit stays safe against a
//     misbehaving controller — a refused dose never reaches the tank and
//     Demeter takes it back out of its ledger.
//   * dosingHandleCommand(line) — the bench channel "chN fwd|rev|stop [ms]
//     [speed]" for wiring and calibration, over USB Serial ONLY since 2.0.0
//     (no MQTT topic any more). Its result carries no id: a bench dose is a
//     foreign dose to Demeter and restarts its lockout, as it should.

#pragma once
#include <stddef.h>
#include <stdint.h>

void dosingInit();
void dosingService(); // stop timed runs; call every loop
// Bench: "chN stop" | "chN fwd <ms> [speed 1-100]" | "chN rev <ms> [speed]" (Serial only)
void dosingHandleCommand(const char *payload);
// v2 request JSON; epochNow = unix seconds (0 when the clock is unknown) for the ack's ts.
void dosingHandleRequest(const char *json, uint32_t epochNow);
// One-shot JSON line for dose/result (NULL when nothing new).
const char *dosingTakeEvent();
// The "dosers" fragment of sys/meta: per reagent channel, calibration, caps. Returns chars written.
size_t dosingMetaJson(char *out, size_t n);
// Print per-channel pin + pulse width + run state + 24 h budget to Serial ("dose status").
void dosingDebugStatus();
