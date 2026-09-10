"""MQTT wiring and the control loop.

Transport notes:
- v1 speaks the firmware's existing bench command channel
  ``pomona/dose/test`` ("chN fwd <ms> [speed]"), which hard-caps every run
  at 10 s and drives one channel at a time — the firmware-side rail. When
  the #224 firmware controller module lands a richer ml-based
  ``dose/request`` contract, only this module changes.
- The rolling dose ledger is persisted as retained JSON on
  ``pomona/demeter/ledger`` so a pod restart cannot forget the acid budget
  or the lockout clock. If no retained ledger exists at boot, Demeter
  assumes a dose just happened (conservative: one lockout period of
  patience costs nothing; a forgotten budget could overdose).
- A live (non-retained) ``pomona/dose/result`` we did not command means the
  owner dosed by hand: unknown ml, but it restarts the lockout clock.
"""

from __future__ import annotations

import json
import logging
import threading
import time

import paho.mqtt.client as mqtt

from . import __version__, engine, metrics
from .config import Config
from .engine import Decision, Ledger, Sample, Telemetry

log = logging.getLogger("demeter")


class Runtime:
    def __init__(self, cfg: Config):
        self.cfg = cfg
        self.base = cfg.mqtt.base_topic
        self.telem = Telemetry()
        self.ledger = Ledger()
        self.lock = threading.Lock()
        self._ledger_loaded = threading.Event()
        self._own_command_ts = 0.0

        self.client = mqtt.Client(
            mqtt.CallbackAPIVersion.VERSION2, client_id=cfg.mqtt.client_id
        )
        if cfg.mqtt.username:
            self.client.username_pw_set(cfg.mqtt.username, cfg.mqtt.password)
        self.client.will_set(
            f"{self.base}/demeter/status", "offline", qos=1, retain=True
        )
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message

    # -- MQTT callbacks ------------------------------------------------------

    def _on_connect(self, client, _userdata, _flags, reason_code, _properties):
        log.info("MQTT connected: %s", reason_code)
        topics = [
            (f"{self.base}/water/ph", 0),
            (f"{self.base}/water/ec_ms_cm", 0),
            (f"{self.base}/water/temp_c", 0),
            (f"{self.base}/unit/status", 1),
            (f"{self.base}/dose/result", 1),
            (f"{self.base}/demeter/ledger", 1),
        ]
        client.subscribe(topics)
        client.publish(f"{self.base}/demeter/status", "online", qos=1, retain=True)
        client.publish(f"{self.base}/demeter/mode", self.cfg.mode, qos=1, retain=True)

    def _on_message(self, _client, _userdata, msg):
        now = time.time()
        payload = msg.payload.decode("utf-8", "replace").strip()
        topic = msg.topic
        with self.lock:
            if topic == f"{self.base}/water/ph":
                self._reading("ph", payload, now)
            elif topic == f"{self.base}/water/ec_ms_cm":
                self._reading("ec", payload, now)
            elif topic == f"{self.base}/water/temp_c":
                self._reading("water_temp", payload, now)
            elif topic == f"{self.base}/unit/status":
                self.telem.unit_online = payload == "online"
                metrics.UNIT_ONLINE.set(1 if self.telem.unit_online else 0)
            elif topic == f"{self.base}/dose/result":
                self._dose_result(msg.retain, now)
            elif topic == f"{self.base}/demeter/ledger":
                self._load_ledger(payload)

    def _reading(self, name: str, payload: str, now: float) -> None:
        try:
            value = float(payload)
        except ValueError:
            log.warning("non-numeric %s payload: %r", name, payload)
            return
        setattr(self.telem, name, Sample(value, now))
        metrics.READING.labels(name).set(value)

    def _dose_result(self, retained: bool, now: float) -> None:
        # Retained replay on (re)connect is of unknown age — never a fresh
        # dose. Results within 30 s of our own command are our own echo
        # (already in the ledger).
        if retained or (now - self._own_command_ts) < 30.0:
            return
        log.info("foreign dose observed on dose/result — restarting lockout")
        self.ledger.record(now, "foreign", 0.0)
        self._publish_ledger()

    def _load_ledger(self, payload: str) -> None:
        if self._ledger_loaded.is_set():
            return  # only the first (retained) ledger at boot is trusted
        self._ledger_loaded.set()
        try:
            data = json.loads(payload)
            self.ledger.doses = [
                (float(ts), str(reagent), float(ml))
                for ts, reagent, ml in data.get("doses", [])
            ]
            log.info("ledger restored: %d dose(s) in window", len(self.ledger.doses))
        except (ValueError, TypeError) as exc:
            log.error("unreadable retained ledger (%s) — assuming fresh dose", exc)
            self.ledger.record(time.time(), "foreign", 0.0)

    def _publish_ledger(self) -> None:
        payload = json.dumps({"v": 1, "doses": self.ledger.doses})
        self.client.publish(f"{self.base}/demeter/ledger", payload, qos=1, retain=True)

    def _publish_decision(self, decision: Decision, executed: bool, now: float) -> None:
        payload = json.dumps(
            {
                "ts": int(now),
                "mode": self.cfg.mode,
                "action": decision.action,
                "condition": decision.condition,
                "reason": decision.reason,
                "executed": executed,
                "steps": [
                    {"command": s.command(), "reagent": s.reagent, "ml": s.ml}
                    for s in decision.steps
                ],
                "version": __version__,
            }
        )
        self.client.publish(f"{self.base}/demeter/decision", payload, qos=1, retain=True)

    # -- dosing --------------------------------------------------------------

    def _execute(self, decision: Decision) -> None:
        for step in decision.steps:
            if step.delay_before_s > 0:
                log.info("waiting %.0f s before %s", step.delay_before_s, step.reagent)
                time.sleep(step.delay_before_s)
            now = time.time()
            cmd = step.command()
            log.info("DOSING %s: %.2f ml -> %r", step.reagent, step.ml, cmd)
            with self.lock:
                self._own_command_ts = now
                # Record BEFORE the pump runs: if we crash mid-dose the ledger
                # over-counts (safe) rather than under-counts (unsafe).
                self.ledger.record(now, step.reagent, step.ml)
                self.ledger.prune(now)
                self._publish_ledger()
            info = self.client.publish(f"{self.base}/dose/test", cmd, qos=1)
            info.wait_for_publish(timeout=10)
            metrics.record_dose(step.reagent, step.ml, now)

    # -- main loop -----------------------------------------------------------

    def run(self) -> None:
        metrics.serve(self.cfg.metrics_port, self.cfg.mode)
        self.client.connect(self.cfg.mqtt.host, self.cfg.mqtt.port, keepalive=60)
        self.client.loop_start()

        # Give a retained ledger a moment to arrive; absent one, start locked.
        if not self._ledger_loaded.wait(timeout=10):
            with self.lock:
                if not self.ledger.doses:
                    log.info("no retained ledger — conservative boot lockout")
                    self.ledger.record(time.time(), "foreign", 0.0)
            self._ledger_loaded.set()

        log.info(
            "demeter %s up — mode=%s, interval=%.0fs, broker=%s:%d",
            __version__,
            self.cfg.mode,
            self.cfg.interval_seconds,
            self.cfg.mqtt.host,
            self.cfg.mqtt.port,
        )
        while True:
            started = time.time()
            try:
                self._cycle(started)
            except Exception:  # a bad cycle must never kill the controller
                log.exception("evaluation cycle failed")
            time.sleep(max(1.0, self.cfg.interval_seconds - (time.time() - started)))

    def _cycle(self, now: float) -> None:
        with self.lock:
            self.ledger.prune(now)
            decision = engine.decide(now, self.telem, self.ledger, self.cfg)
            self._update_gauges(now)

        metrics.DECISIONS.labels(decision.action, decision.condition or "-").inc()
        is_dose = decision.action in (
            engine.DOSE_PH_FULL,
            engine.DOSE_PH_FINE,
            engine.DOSE_NUTRIENTS,
        )
        metrics.WOULD_DOSE.set(1 if (is_dose and self.cfg.mode == "shadow") else 0)

        if decision.action == engine.NONE:
            return
        log.info(
            "decision: %s condition=%s reason=%s",
            decision.action,
            decision.condition,
            decision.reason,
        )
        if is_dose and self.cfg.mode == "active":
            self._publish_decision(decision, executed=True, now=now)
            self._execute(decision)
        else:
            self._publish_decision(decision, executed=False, now=now)

    def _update_gauges(self, now: float) -> None:
        for name in ("ph", "ec", "water_temp"):
            sample = getattr(self.telem, name)
            if sample is not None:
                metrics.READING_AGE.labels(name).set(now - sample.ts)
        for reagent in ("ph_down", "nutrient_a", "nutrient_b"):
            metrics.BUDGET_ML_24H.labels(reagent).set(self.ledger.ml_24h(reagent, now))
        last = self.ledger.last_dose_ts()
        lockout_s = self.cfg.dosing.lockout_minutes * 60
        remaining = max(0.0, lockout_s - (now - last)) if last is not None else 0.0
        metrics.LOCKOUT_REMAINING.set(remaining)
