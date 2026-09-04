// Pomona firmware — dosing bench module (Trello #284).
//
// BENCH/BRING-UP ONLY: drives the DFR0523 peristaltic channels from explicit
// pomona/dose/test commands so the pumps can be wired, 3.3V-verified and
// ml/s-calibrated. No autonomous dosing lives here — the #224 controller
// (quanta, lockout, daily budget, band logic) is a later, separate module.
// Every command is hard-capped at DOSE_TEST_MAX_MS and only one channel runs
// at a time.

#pragma once
#include <stddef.h>

void dosingInit();
void dosingService(); // stop timed runs; call every loop
// Payload: "chN stop" | "chN fwd <ms> [speed 1-100]" | "chN rev <ms> [speed]"
void dosingHandleCommand(const char *payload);
// One-shot event line for pomona/dose/result (NULL when nothing new).
const char *dosingTakeEvent();
// Print per-channel pin + pulse width + run state to Serial ("dose status").
void dosingDebugStatus();
