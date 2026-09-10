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


def serve(port: int, mode: str) -> None:
    INFO.info({"version": __version__, "mode": mode})
    start_http_server(port)


def record_dose(reagent: str, ml: float, ts: float | None = None) -> None:
    DOSES.labels(reagent).inc()
    DOSED_ML.labels(reagent).inc(ml)
    LAST_DOSE_TS.labels(reagent).set(ts if ts is not None else time.time())
