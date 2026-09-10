"""Prometheus exposition — what Demeter saw, decided and dosed, when.

Served by prometheus_client's own daemon thread (zeus pattern: it answers
independently of the control loop, so the k8s probes hit it safely) and
scraped by kube-prometheus-stack via the landing zone's ServiceMonitor.

Series (prefix demeter_):
- demeter_info{version, mode}                 build/mode identity
- demeter_reading{metric}                     last pH / EC / water-temp seen
- demeter_reading_age_seconds{metric}         staleness of each reading
- demeter_unit_online                         1 while the GIGA's LWT says online
- demeter_decisions_total{action, condition}  every evaluation outcome
- demeter_doses_total{reagent}                doses executed (active mode)
- demeter_dosed_ml_total{reagent}             ml delivered, cumulative
- demeter_last_dose_timestamp_seconds{reagent} unix ts of the last dose
- demeter_budget_used_ml_24h{reagent}         rolling 24 h ml (acid cap watch)
- demeter_lockout_remaining_seconds           0 when dosing is allowed
- demeter_would_dose                          1 when shadow mode WOULD have dosed
- demeter_ph_sensitivity_ph_per_ml            posterior mean k (pH per ml)
- demeter_ph_sensitivity_sd                   posterior sd of k (shrinks with evidence)
- demeter_ph_buffer_knee_ph                   posterior mean b (pH the buffer swallows first)
- demeter_ph_sensitivity_observations         settled pH-Down responses that trained the posterior
- demeter_learned_noise_ph                    probe jitter sd (Kalman measurement noise)
- demeter_ph_filtered                         Kalman level estimate of pH
- demeter_ph_slope_per_hour                   Kalman slope estimate (pH/h)
- demeter_learned_settle_seconds              how long a dose takes to settle (drives the lockout)
- demeter_learned_rebound_ph_per_hour         post-dose pH climb rate (drives the aim point)
- demeter_aim_ph                              where the next dose aims to land
- demeter_last_dose_response_ph_drop          pre - post pH of the last settled dose
- demeter_no_response_streak                  consecutive doses that did nothing
- demeter_dose_response_pending               1 while the last dose's response is unsettled
- demeter_planned_ph_dose_ml                  what the next pH-Down dose would be (0 = none due)
"""

from __future__ import annotations

import time

from prometheus_client import Counter, Gauge, Info, start_http_server

from . import __version__

INFO = Info("demeter", "Demeter build/mode identity")
READING = Gauge("demeter_reading", "Last reservoir reading seen", ["metric"])
READING_AGE = Gauge(
    "demeter_reading_age_seconds", "Age of the last reading", ["metric"]
)
UNIT_ONLINE = Gauge("demeter_unit_online", "1 while pomona/unit/status is online")
DECISIONS = Counter(
    "demeter_decisions_total", "Evaluation outcomes", ["action", "condition"]
)
DOSES = Counter("demeter_doses_total", "Doses executed", ["reagent"])
DOSED_ML = Counter("demeter_dosed_ml_total", "Milliliters delivered", ["reagent"])
LAST_DOSE_TS = Gauge(
    "demeter_last_dose_timestamp_seconds", "Unix ts of the last dose", ["reagent"]
)
BUDGET_ML_24H = Gauge(
    "demeter_budget_used_ml_24h", "Rolling 24h ml delivered", ["reagent"]
)
LOCKOUT_REMAINING = Gauge(
    "demeter_lockout_remaining_seconds", "Seconds until dosing is allowed again"
)
WOULD_DOSE = Gauge(
    "demeter_would_dose", "1 when shadow mode would have dosed this cycle"
)
PH_SENSITIVITY = Gauge(
    "demeter_ph_sensitivity_ph_per_ml", "Learned (or prior) pH drop per ml past the knee"
)
PH_SENSITIVITY_OBS = Gauge(
    "demeter_ph_sensitivity_observations", "Settled pH-Down responses learned from"
)
LAST_RESPONSE_DROP = Gauge(
    "demeter_last_dose_response_ph_drop", "pre - post pH of the last settled dose"
)
NO_RESPONSE_STREAK = Gauge(
    "demeter_no_response_streak", "Consecutive doses that produced no pH response"
)
RESPONSE_PENDING = Gauge(
    "demeter_dose_response_pending", "1 while the last dose's response is unsettled"
)
PH_SENSITIVITY_SD = Gauge("demeter_ph_sensitivity_sd", "Posterior sd of k")
PH_BUFFER = Gauge("demeter_ph_buffer_knee_ph", "Posterior mean b: pH the buffer swallows first")
LEARNED_NOISE = Gauge("demeter_learned_noise_ph", "Learned probe jitter sd (Kalman R)")
PH_FILTERED = Gauge("demeter_ph_filtered", "Kalman level estimate of pH")
PH_SLOPE = Gauge("demeter_ph_slope_per_hour", "Kalman slope estimate of pH, per hour")
LEARNED_SETTLE_S = Gauge("demeter_learned_settle_seconds", "Learned dose settle time")
LEARNED_REBOUND = Gauge("demeter_learned_rebound_ph_per_hour", "Learned post-dose pH rebound")
AIM_PH = Gauge("demeter_aim_ph", "pH the next dose aims to land on")
PLANNED_PH_DOSE_ML = Gauge(
    "demeter_planned_ph_dose_ml", "Size of the pH-Down dose the planner would send now"
)


def serve(port: int, mode: str) -> None:
    INFO.info({"version": __version__, "mode": mode})
    start_http_server(port)


def record_dose(reagent: str, ml: float, ts: float | None = None) -> None:
    DOSES.labels(reagent).inc()
    DOSED_ML.labels(reagent).inc(ml)
    LAST_DOSE_TS.labels(reagent).set(ts if ts is not None else time.time())
