# Demeter — the pomona autodosing controller (cards #278/#224)

Demeter is the k3s chemistry brain: a small Python service that watches the
reservoir over MQTT and corrects it with the calibrated DFR0523 dosing pumps
— **pH-Down** when the water drifts alkaline (the tap water's nightly CO₂
rebound), **Nutrient A then B** when the EC sags. It productionizes the
playbook the interim Tethys agent ran hourly (gitops
`.claude/agents/tethys.md`), with the same hard rails; card #224 remains the
design of record and Demeter is its k3s half.

```
GIGA firmware ──pomona/water/{ph,ec_ms_cm,temp_c}──> EMQX mqtt.lab.local
     ▲                                                  │
     │ pomona/dose/test  "chN fwd <ms> [speed]"         ▼
     └───────────────────────────────────── demeter (ns pomona, k3s)
        firmware rails: 10 s hard cap per command,      │
        one channel at a time, explicit stop at boot    ▼
                                               Prometheus /metrics :9000
```

## The rails (engine.py — enforced in code, tested in tests/)

| situation | action |
|---|---|
| pH > 7.0, confirmed ≥10 min | 1 ml pH-Down full speed (`ch1 fwd 2083`) |
| 6.5 < pH ≤ 7.0, confirmed | 0.5 ml pH-Down slow (`ch1 fwd 4545 50`) |
| acid ≥ 4 ml / rolling 24 h | STOP + alert — never exceed the cap |
| EC < 0.7, confirmed | 5 ml A (`ch2`), wait 2 min, 5 ml B (`ch3`); max once/24 h |
| pH < 5.4 / EC > 1.1 / water > 26.5 °C | alert only — no reagent for these |
| any dose (ours or foreign) < 60 min ago | locked out (probe not dosing-grade) |
| readings stale (> 2 min) or unit offline | never dose |

One corrective action per cycle, pH before EC. In doubt, don't dose.

## Modes

- **`shadow`** (default): full evaluation, decisions published to
  `pomona/demeter/decision` + `demeter_would_dose` metric — **no dose
  command is ever sent**. Run this against live water until its decisions
  match what Tethys/the owner would have done.
- **`active`**: doses for real. Flip only via the gitops values file
  (`landingzones/pomona`), and retire the Tethys hourly task at the same
  moment — two brains dosing one tank is the one topology that must never
  exist (the lockout on foreign doses is the belt-and-braces for exactly
  that transition window).

## The self-learning brain (0.2.0 — `adapt.py`)

The first live evening (2026-09-10) showed the fixed playbook's blind spot:
at pH 8.2 every hourly 1 ml landed on a regenerated buffer and did nothing,
the 4 ml cap would have been spent by morning with pH still above 8, and a
manual dose whose line was still full of water showed no response — which
the fixed 60 min lockout turned into an hour of waiting. Demeter now learns.
The config has exactly two rails for it, `adaptive.enabled` and
`adaptive.ph_max_single_dose_ml`; everything else comes from two small,
deterministic AI models whose whole state is a few hundred bytes in the
retained ledger:

| model | what it is | what it gives |
|---|---|---|
| **A — dose response** | Bayesian linear regression `drop = k·ml − b + ε` with a Gaussian posterior over `k` (pH per ml) and `b` (what the buffer swallows first, the "knee"); prior = the Tethys titration, wide | the dose is sized so the posterior *mean* drop lands on the aim, then clipped so the *97.5 % upper* predictive drop keeps pH ≥ `ph_tol_low`. Uncertainty grows with `ml²·Var(k)`, so extrapolating beyond observed doses is penalised by the model itself — no safety factor, no trust region, no `k_max` |
| **B — pH signal** | Kalman filter, local linear trend `[level, slope]`, measurement noise estimated online from the innovations | the pre-dose level; the **trough** after a dose (its true effect, independent of the rebound); "the fall has stopped" (slope no longer significantly negative for 5 min → settled → the lockout releases at 2× the learned settle time, floor 5 min, ceiling `lockout_minutes`); the **rebound** rate (a significantly positive slope 15 min–2 h after the trough), which pulls the aim from the top of the target band toward the bottom |

Why not an LLM: the loop puts acid into a living reservoir on 2–6
observations a day; it needs calibrated uncertainty, a deterministic and
unit-testable decision, and no network dependency. Tethys/Cortana remain
the *advisors* reading the same metrics; nothing with a prompt sits on the
actuation path.

A dose with no measurable response (drop below `2·sd` of the probe noise,
floor 0.1 pH) is **never learned from** — a delivery failure looks exactly
like "the buffer ate 2 ml" and would poison the posterior — it re-doses
after the early release, and three in a row is an **ALERT** (unprimed
line, empty bottle, dead pump). An observation more than 4 sd off the
predictive is likewise ignored. The fine tier (6.5–7.0 → 0.5 ml slow) is
deliberately still fixed; `adaptive.enabled: false` restores 0.1.0
behaviour exactly. Structural constants (prior, process noise, windows,
z-scores) live at the top of `adapt.py` and are not tuning knobs.

## State across restarts

The rolling 24 h dose ledger (v2: plus the pending response, settled responses,
learned sensitivity and no-response streak) is retained JSON on `pomona/demeter/ledger`;
a restarted pod reloads it before it may dose. No retained ledger → Demeter
assumes a dose just happened and waits out one lockout (conservative by
construction). A live `pomona/dose/result` it did not command (owner manual
dose) restarts the lockout clock.

## Metrics (Prometheus, :9000)

`demeter_doses_total{reagent}`, `demeter_dosed_ml_total{reagent}`,
`demeter_last_dose_timestamp_seconds{reagent}`,
`demeter_budget_used_ml_24h{reagent}`, `demeter_lockout_remaining_seconds`,
`demeter_ph_sensitivity_ph_per_ml` (±`_sd`), `demeter_ph_buffer_knee_ph`,
`demeter_ph_sensitivity_observations`, `demeter_learned_noise_ph`,
`demeter_ph_filtered`, `demeter_ph_slope_per_hour`, `demeter_learned_settle_seconds`,
`demeter_learned_rebound_ph_per_hour`, `demeter_aim_ph`,
`demeter_last_dose_response_ph_drop`, `demeter_no_response_streak`,
`demeter_dose_response_pending`, `demeter_planned_ph_dose_ml`,
`demeter_decisions_total{action,condition}`, `demeter_reading{metric}` +
`demeter_reading_age_seconds{metric}`, `demeter_unit_online`,
`demeter_would_dose`. Scraped by kube-prometheus-stack via the landing
zone's ServiceMonitor; every dose is also visible as a step in the InfluxDB
pH/EC series.

## Dev

```sh
python3 -m venv .venv && .venv/bin/pip install -e '.[dev]'
.venv/bin/pytest tests/
```

## Image (arm64 — the cluster requires it)

```sh
docker buildx build --platform linux/arm64 --provenance=false \
  -t jellebens/pomona-demeter:0.2.0 --push controller/
```

## Deploy

Chart: gitops `landingzones/pomona` (namespace `pomona`, Argo app `pomona`).
Broker credentials are the dedicated **`pomona-demeter`** EMQX user
(subscribe `pomona/#`, publish only `pomona/dose/test` + `pomona/demeter/#`)
sealed into `.config/<env>/pomona.yaml` — see the landing zone README's
owner runbook. v1 speaks the firmware bench channel `pomona/dose/test`
(hard-capped at 10 s per command by `DOSE_TEST_MAX_MS`); the richer ml-based
`dose/request` contract arrives with the #224 firmware controller module and
only `runtime.py` changes.
