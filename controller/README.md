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

## State across restarts

The rolling 24 h dose ledger is retained JSON on `pomona/demeter/ledger`;
a restarted pod reloads it before it may dose. No retained ledger → Demeter
assumes a dose just happened and waits out one lockout (conservative by
construction). A live `pomona/dose/result` it did not command (owner manual
dose) restarts the lockout clock.

## Metrics (Prometheus, :9000)

`demeter_doses_total{reagent}`, `demeter_dosed_ml_total{reagent}`,
`demeter_last_dose_timestamp_seconds{reagent}`,
`demeter_budget_used_ml_24h{reagent}`, `demeter_lockout_remaining_seconds`,
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
  -t jellebens/pomona-demeter:0.1.0 --push controller/
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
