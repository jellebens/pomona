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
- The ledger is v2 since 0.2.0: it also carries the adaptive state (pending
  dose response, settled responses, learned sensitivity, no-response
  streak — see adapt.py). A v1 ledger still loads; the adaptive fields
  simply start from their priors.
"""

from __future__ import annotations

import json
import logging
import threading
import time

import paho.mqtt.client as mqtt

from . import __version__, adapt, engine, metrics
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
        if name == "ph":
            adapt.ingest_ph(self.ledger.model, value, now)
        metrics.READING.labels(name).set(value)

    def _dose_result(self, retained: bool, now: float) -> None:
        # Retained replay on (re)connect is of unknown age — never a fresh
        # dose. Results within 30 s of our own command are our own echo
        # (already in the ledger).
        if retained or (now - self._own_command_ts) < 30.0:
            return
        log.info("foreign dose observed on dose/result — restarting lockout")
        self.ledger.record(now, "foreign", 0.0)
        adapt.note_dose(self.ledger, now, "foreign", 0.0, self._pre_ph(now))
        self._publish_ledger()

    def _pre_ph(self, now: float) -> float | None:
        """The pH a dose is judged against: the filtered level, if fresh."""
        s = self.telem.ph
        if s is None or (now - s.ts) > self.cfg.freshness_seconds:
            return None
        level = adapt.current_ph(self.ledger.model)
        return level if level is not None else s.value

    def _load_ledger(self, payload: str) -> None:
        if self._ledger_loaded.is_set():
            return  # only the first (retained) ledger at boot is trusted
        self._ledger_loaded.set()
        try:
            data = json.loads(payload)
            adapt.ledger_from_dict(self.ledger, data)
            m = self.ledger.model
            log.info(
                "ledger restored (v%s): %d dose(s) in window, posterior k=%.2f±%.2f b=%.2f (n=%d) "
                "noise sd=%.3f settle=%s rebound=%s/h, no-response streak=%d, pending=%s",
                data.get("v", 1),
                len(self.ledger.doses),
                m.k,
                m.k_sd,
                m.b,
                m.n,
                m.noise_sd,
                m.settle_s,
                m.rebound_ph_h,
                m.no_response_streak,
                "yes" if self.ledger.pending else "no",
            )
        except (ValueError, TypeError, KeyError) as exc:
            log.error("unreadable retained ledger (%s) — assuming fresh dose", exc)
            self.ledger.record(time.time(), "foreign", 0.0)

    def _publish_ledger(self) -> None:
        payload = json.dumps(adapt.ledger_to_dict(self.ledger))
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
                adapt.note_dose(self.ledger, now, step.reagent, step.ml, self._pre_ph(now))
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
                    boot = time.time()
                    self.ledger.record(boot, "foreign", 0.0)
                    adapt.note_dose(self.ledger, boot, "foreign", 0.0, None)
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
            settled = adapt.observe(now, self.ledger, self.cfg)
            if settled is not None:
                drop = settled.drop
                m = self.ledger.model
                log.info(
                    "dose response settled: %s %.2f ml, pH %s -> trough %.2f (drop %s), "
                    "learned=%s posterior k=%.2f±%.2f b=%.2f n=%d settle=%ss no-response streak=%d",
                    settled.reagent,
                    settled.ml,
                    "?" if settled.pre_ph is None else f"{settled.pre_ph:.2f}",
                    settled.post_ph,
                    "?" if drop is None else f"{drop:+.2f}",
                    settled.learned,
                    m.k,
                    m.k_sd,
                    m.b,
                    m.n,
                    None if m.settle_s is None else int(m.settle_s),
                    m.no_response_streak,
                )
                if drop is not None:
                    metrics.LAST_RESPONSE_DROP.set(drop)
                self._publish_ledger()
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
        metrics.LOCKOUT_REMAINING.set(adapt.lockout_remaining(now, self.ledger, self.cfg))
        m = self.ledger.model
        metrics.PH_SENSITIVITY.set(m.k)
        metrics.PH_SENSITIVITY_SD.set(m.k_sd)
        metrics.PH_BUFFER.set(m.b)
        metrics.PH_SENSITIVITY_OBS.set(m.n)
        metrics.NO_RESPONSE_STREAK.set(m.no_response_streak)
        metrics.RESPONSE_PENDING.set(1 if self.ledger.pending else 0)
        metrics.LEARNED_NOISE.set(m.noise_sd)
        metrics.LEARNED_SETTLE_S.set(m.settle_s or 0.0)
        metrics.LEARNED_REBOUND.set(m.rebound_ph_h or 0.0)
        metrics.PH_FILTERED.set(m.kf_level if m.kf_level is not None else 0.0)
        metrics.PH_SLOPE.set(m.kf_slope * 3600.0)
        metrics.AIM_PH.set(adapt.aim_ph(self.ledger, self.cfg))
        ph = self.telem.ph
        if ph is not None and ph.value > self.cfg.bands.ph_high_full:
            metrics.PLANNED_PH_DOSE_ML.set(adapt.plan_ph_dose(ph.value, self.ledger, self.cfg)[0])
        else:
            metrics.PLANNED_PH_DOSE_ML.set(0)
