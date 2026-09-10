"""The decision engine — a pure function over telemetry + ledger.

Encodes the dosing playbook that ran interim as the Tethys agent
(gitops .claude/agents/tethys.md, owner grant 2026-09-10), which is itself
the operational form of card #224's rails:

- pH > 7.0            -> 1 ml pH-Down, full speed (buffer knee: the first ml
                         eats ~-0.4 pH, past the knee ~-1.3 pH/ml — which is
                         exactly why doses are small and never stacked).
- 6.5 < pH <= 7.0     -> 0.5 ml pH-Down, slow (a full ml can overshoot < 5.4).
- acid hard cap       -> 4 ml pH-Down per rolling 24 h; cap hit + still high
                         is an alert, not a dose.
- EC < 0.7            -> 5 ml Nutrient A, wait >= 2 min, 5 ml Nutrient B
                         (never simultaneously — plumbing rule), max once/24 h.
- pH < 5.4 / EC > 1.1 / water hot -> no reagent exists for these: alert only
                         (fix is demin top-up / shading — a human).
- One corrective action per cycle, pH before EC.

Guards, all of which must hold before any dose:
- unit online and the triggering reading fresh (<= freshness_seconds);
- the violation confirmed continuously for >= confirm_minutes;
- >= lockout_minutes since ANY dose (ours or foreign — the probe needs an
  hour post-dose before readings are dosing-grade);
- the 24 h budget for that reagent not exhausted.

In doubt, don't dose: a skipped hour is free, an overdose is not.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

from .config import Config, PumpChannel

# Decision actions
NONE = "none"
DOSE_PH_FULL = "dose_ph_full"
DOSE_PH_FINE = "dose_ph_fine"
DOSE_NUTRIENTS = "dose_nutrients"
ALERT = "alert"  # condition present but no reagent for it (card territory)
BLOCKED = "blocked"  # dose warranted but a guard said no


@dataclass
class Sample:
    value: float
    ts: float  # unix seconds


@dataclass
class Telemetry:
    """Latest samples plus per-condition violation streak starts."""

    ph: Sample | None = None
    ec: Sample | None = None
    water_temp: Sample | None = None
    unit_online: bool = False
    # condition name -> ts the current uninterrupted violation streak began
    streaks: dict[str, float] = field(default_factory=dict)

    def update_streak(self, condition: str, violating: bool, ts: float) -> None:
        if violating:
            self.streaks.setdefault(condition, ts)
        else:
            self.streaks.pop(condition, None)

    def streak_age(self, condition: str, now: float) -> float:
        start = self.streaks.get(condition)
        return (now - start) if start is not None else 0.0


@dataclass
class Ledger:
    """Rolling dose history (persisted as the retained ledger topic)."""

    # (ts, reagent, ml) — reagent in {"ph_down", "nutrient_a", "nutrient_b",
    # "foreign"}; foreign = a dose/result event we did not command (owner or
    # Tethys): unknown ml, but it still restarts the lockout clock.
    doses: list[tuple[float, str, float]] = field(default_factory=list)

    def prune(self, now: float, horizon_s: float = 24 * 3600) -> None:
        self.doses = [d for d in self.doses if now - d[0] <= horizon_s]

    def last_dose_ts(self) -> float | None:
        return max((d[0] for d in self.doses), default=None)

    def ml_24h(self, reagent: str, now: float) -> float:
        return sum(ml for ts, r, ml in self.doses if r == reagent and now - ts <= 24 * 3600)

    def count_24h(self, reagent: str, now: float) -> int:
        return sum(1 for ts, r, _ in self.doses if r == reagent and now - ts <= 24 * 3600)

    def record(self, ts: float, reagent: str, ml: float) -> None:
        self.doses.append((ts, reagent, ml))


@dataclass
class DoseStep:
    reagent: str
    channel: int
    ms: int
    speed: int | None  # None = full speed (omitted in the command)
    ml: float
    delay_before_s: float = 0.0

    def command(self) -> str:
        cmd = f"ch{self.channel} fwd {self.ms}"
        if self.speed is not None:
            cmd += f" {self.speed}"
        return cmd


@dataclass
class Decision:
    action: str
    condition: str = ""
    reason: str = ""
    steps: list[DoseStep] = field(default_factory=list)


def dose_ms(channel: PumpChannel, ml: float, slow: bool) -> int:
    rate = channel.slow_ml_s if slow else channel.full_ml_s
    if rate <= 0:
        raise ValueError(f"channel {channel.channel}: no calibration for this speed")
    return int(round(ml / rate * 1000))


def _ph_step(cfg: Config, ml: float, slow: bool) -> DoseStep:
    ch = cfg.dosing.ph_down
    return DoseStep(
        reagent="ph_down",
        channel=ch.channel,
        ms=dose_ms(ch, ml, slow),
        speed=ch.slow_speed if slow else None,
        ml=ml,
    )


def _nutrient_steps(cfg: Config) -> list[DoseStep]:
    ml = cfg.dosing.nutrient_dose_ml
    a, b = cfg.dosing.nutrient_a, cfg.dosing.nutrient_b
    return [
        DoseStep("nutrient_a", a.channel, dose_ms(a, ml, slow=False), None, ml),
        DoseStep(
            "nutrient_b",
            b.channel,
            dose_ms(b, ml, slow=False),
            None,
            ml,
            delay_before_s=cfg.dosing.nutrient_gap_seconds,
        ),
    ]


def _fresh(sample: Sample | None, now: float, cfg: Config) -> bool:
    return sample is not None and (now - sample.ts) <= cfg.freshness_seconds


def _validate_steps(steps: list[DoseStep], cfg: Config) -> str | None:
    for step in steps:
        if step.ms <= 0 or step.ms > cfg.dosing.max_command_ms:
            return f"computed run {step.ms} ms outside (0, {cfg.dosing.max_command_ms}] for {step.reagent}"
        if not math.isfinite(step.ml) or step.ml <= 0:
            return f"bad ml for {step.reagent}"
    return None


def decide(now: float, telem: Telemetry, ledger: Ledger, cfg: Config) -> Decision:
    b = cfg.bands
    confirm_s = cfg.confirm_minutes * 60
    lockout_s = cfg.dosing.lockout_minutes * 60

    # -- update violation streaks from the current samples ------------------
    ph = telem.ph.value if _fresh(telem.ph, now, cfg) else None
    ec = telem.ec.value if _fresh(telem.ec, now, cfg) else None
    wtemp = telem.water_temp.value if _fresh(telem.water_temp, now, cfg) else None

    telem.update_streak("ph_high_full", ph is not None and ph > b.ph_high_full, now)
    telem.update_streak(
        "ph_high_fine", ph is not None and b.ph_tol_high < ph <= b.ph_high_full, now
    )
    telem.update_streak("ph_low", ph is not None and ph < b.ph_tol_low, now)
    telem.update_streak("ec_low", ec is not None and ec < b.ec_tol_low, now)
    telem.update_streak("ec_high", ec is not None and ec > b.ec_tol_high, now)
    telem.update_streak("wtemp_high", wtemp is not None and wtemp > b.water_temp_max_c, now)

    # -- alert-only conditions (no reagent exists; never dose for these) ----
    for cond in ("ph_low", "ec_high", "wtemp_high"):
        if telem.streak_age(cond, now) >= confirm_s:
            return Decision(ALERT, condition=cond, reason="no reagent for this — human action needed")

    # -- pick the (single) corrective action, pH before EC ------------------
    if telem.streak_age("ph_high_full", now) >= confirm_s:
        condition, steps = "ph_high_full", [_ph_step(cfg, cfg.dosing.ph_full_dose_ml, slow=False)]
    elif telem.streak_age("ph_high_fine", now) >= confirm_s:
        condition, steps = "ph_high_fine", [_ph_step(cfg, cfg.dosing.ph_fine_dose_ml, slow=True)]
    elif telem.streak_age("ec_low", now) >= confirm_s:
        condition, steps = "ec_low", _nutrient_steps(cfg)
    else:
        return Decision(NONE, reason="in band or unconfirmed")

    # -- guards --------------------------------------------------------------
    if not telem.unit_online:
        return Decision(BLOCKED, condition, "unit not online")
    if condition.startswith("ph") and ph is None:
        return Decision(BLOCKED, condition, "pH reading stale")
    if condition == "ec_low" and ec is None:
        return Decision(BLOCKED, condition, "EC reading stale")

    last = ledger.last_dose_ts()
    if last is not None and (now - last) < lockout_s:
        return Decision(BLOCKED, condition, f"lockout: {int(lockout_s - (now - last))} s remaining")

    if condition.startswith("ph"):
        want = sum(s.ml for s in steps)
        used = ledger.ml_24h("ph_down", now)
        if used + want > cfg.dosing.ph_daily_cap_ml + 1e-9:
            return Decision(
                ALERT,
                condition,
                f"acid cap: {used:.1f} ml of {cfg.dosing.ph_daily_cap_ml:.1f} ml/24h used and pH still high",
            )
    else:
        if ledger.count_24h("nutrient_a", now) >= cfg.dosing.nutrient_max_per_24h:
            return Decision(BLOCKED, condition, "nutrient budget: already dosed in the last 24 h")

    if err := _validate_steps(steps, cfg):
        return Decision(BLOCKED, condition, f"refusing dose: {err}")

    action = {
        "ph_high_full": DOSE_PH_FULL,
        "ph_high_fine": DOSE_PH_FINE,
        "ec_low": DOSE_NUTRIENTS,
    }[condition]
    return Decision(action, condition, "confirmed out of band", steps)
