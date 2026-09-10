"""Demeter's self-learning brain — two rails in config, two small models inside.

Config (``adaptive:``): ``enabled`` and ``ph_max_single_dose_ml``. That is
all. Bands and the hard rails (24 h acid cap, firmware 10 s cap, the fixed
fine tier) stay where they were; nothing in here can loosen them.

Why these models and not an LLM: the loop actuates acid into a living
reservoir on 2–6 observations a day. It needs calibrated *uncertainty*, a
deterministic and unit-testable decision, and zero network dependencies.
That is the textbook domain of two classical AI models — Bayesian linear
regression for the dose response and a Kalman filter for the pH signal —
both a few hundred bytes of state, both explainable in one sentence each.
An LLM (Tethys/Cortana) stays the *advisor* that reads the same metrics;
it never sits on the actuation path.

Model A — dose response, Bayesian linear regression (Gaussian posterior)
    drop = k·ml − b + ε,  ε ~ N(0, σ²)
    k = pH per ml, b = what the buffer swallows first (the "knee"), both
    with a prior from the Tethys titration (~9.7 L, this pH-Down). Every
    settled dose with a real response is one conjugate update. The dose is
    sized so the posterior *mean* lands on the aim, then clipped so the
    posterior *97.5 % upper* drop stays above ph_tol_low. Uncertainty
    grows with ml² · Var(k), so extrapolating past what has been observed
    is penalised by the model itself — no hand-set safety factor, no trust
    region, no k_max. Doses that show no response are never learned from
    (a delivery failure looks like "the buffer ate 2 ml" and would poison
    the posterior); they re-dose after the early release and three in a
    row is an ALERT.

Model B — pH signal, Kalman filter (local linear trend)
    state [level, slope]; measurement noise R is estimated online from the
    innovations (the probe's own jitter is learned, not typed in). It gives:
    the pre-dose level, the *trough* after a dose (the dose's true effect,
    independent of the rebound that follows), "the fall has stopped" (slope
    no longer significantly negative → settled, which releases the lockout
    early), and the rebound rate (a significantly positive slope after the
    trough), which pulls the aim point from the top of the target band
    toward the bottom.

All learned state lives in ``Model`` and is persisted in the retained ledger
(v2). The constants below are structural (process noise, windows, z-scores),
not tuning knobs, and deliberately not in the config.
"""

from __future__ import annotations

import math
from dataclasses import asdict, dataclass, field, fields
from typing import TYPE_CHECKING

if TYPE_CHECKING:  # pragma: no cover — import cycle guard
    from .config import Config
    from .engine import Ledger

PH_DOWN = "ph_down"
FOREIGN = "foreign"

# -- structural constants (not config) -----------------------------------------
# Model A prior — the Tethys titration, deliberately wide.
PRIOR_K, PRIOR_K_SD = 1.3, 0.6  # pH per ml
PRIOR_B, PRIOR_B_SD = 0.4, 0.3  # pH swallowed by the buffer before pH moves
GUARD_Z = 2.0  # the guard uses the 97.5 % upper predictive drop
OUTLIER_Z = 4.0  # an observation this far off the predictive is not learned
# Model B — Kalman local linear trend, per-second process noise.
Q_LEVEL = 1e-6  # pH² / s: slow drift the model cannot explain
Q_LEVEL_DOSING = 1e-3  # pH² / s while a dose response is pending: the level is expected to move
Q_SLOPE = 1e-10  # (pH/s)² / s: the trend itself wanders slowly
R_INIT, R_FLOOR, R_CAP = 0.01, 0.0025, 0.0625  # measurement variance: sd 0.1 start, 0.05..0.25
R_EWMA = 0.05
DOSE_JUMP_VAR = 4.0  # level variance injected at a dose so the filter re-acquires fast
# Settling / lockout / rebound.
SETTLE_WINDOW_S = 5 * 60  # settled = the trough has not deepened for this long
LOCKOUT_MIN_FLOOR_S = 5 * 60
LOCKOUT_MIN_DEFAULT_S = 10 * 60  # until a settle time has been observed
EWMA = 0.3  # settle time and rebound smoothing
NO_RESPONSE_ALERT_STREAK = 3
NO_RESPONSE_FLOOR = 0.1  # pH: below this a "drop" is never a response, whatever the noise
RESOLUTION_ML = 0.1
REBOUND_HORIZON_H = 0.5  # aim low enough that the rebound needs this long to climb back
REBOUND_MIN_S = 15 * 60  # rebound is read only this long after the trough
REBOUND_MAX_S = 2 * 3600
RESPONSE_HISTORY = 20


@dataclass
class Model:
    """Everything Demeter has learned. Defaults = the prior / knows nothing."""

    # Model A: Gaussian posterior over [k, b]
    k: float = PRIOR_K
    b: float = PRIOR_B
    cov: list = field(default_factory=lambda: [[PRIOR_K_SD**2, 0.0], [0.0, PRIOR_B_SD**2]])
    n: int = 0
    # Model B: Kalman state
    kf_level: float | None = None
    kf_slope: float = 0.0
    kf_p: list = field(default_factory=lambda: [[1.0, 0.0], [0.0, 1e-6]])
    kf_r: float = R_INIT
    kf_ts: float | None = None
    # trough tracking for the pending dose
    trough_ph: float | None = None
    trough_ts: float | None = None
    # learned timings + bookkeeping
    settle_s: float | None = None
    rebound_ph_h: float | None = None
    no_response_streak: int = 0
    overshoots: int = 0

    # -- Model A -------------------------------------------------------------
    def predict_drop(self, ml: float, sigma2: float) -> tuple[float, float]:
        """Posterior predictive (mean, sd) of the pH drop for a dose of ml."""
        x = (ml, -1.0)
        mu = self.k * ml - self.b
        var = (
            x[0] * (self.cov[0][0] * x[0] + self.cov[0][1] * x[1])
            + x[1] * (self.cov[1][0] * x[0] + self.cov[1][1] * x[1])
            + sigma2
        )
        return mu, math.sqrt(max(var, 0.0))

    def update_response(self, ml: float, drop: float, sigma2: float) -> None:
        """One conjugate Bayesian update (the Kalman form of BLR)."""
        x = (ml, -1.0)
        px = (
            self.cov[0][0] * x[0] + self.cov[0][1] * x[1],
            self.cov[1][0] * x[0] + self.cov[1][1] * x[1],
        )
        s = x[0] * px[0] + x[1] * px[1] + sigma2
        gain = (px[0] / s, px[1] / s)
        innov = drop - (self.k * ml - self.b)
        self.k = round(self.k + gain[0] * innov, 4)
        self.b = round(self.b + gain[1] * innov, 4)
        self.cov = [
            [self.cov[0][0] - gain[0] * px[0], self.cov[0][1] - gain[0] * px[1]],
            [self.cov[1][0] - gain[1] * px[0], self.cov[1][1] - gain[1] * px[1]],
        ]
        self.n += 1

    @property
    def k_sd(self) -> float:
        return math.sqrt(max(self.cov[0][0], 0.0))

    # -- Model B derived quantities -------------------------------------------
    @property
    def noise_sd(self) -> float:
        return math.sqrt(self.kf_r)

    @property
    def slope_sd(self) -> float:
        return math.sqrt(max(self.kf_p[1][1], 0.0))

    def drop_sigma2(self) -> float:
        """Variance of a measured drop (pre and post each carry probe noise)."""
        return 2.0 * self.kf_r

    def no_response_eps(self) -> float:
        return max(GUARD_Z * math.sqrt(self.drop_sigma2()), NO_RESPONSE_FLOOR)

    def lockout_min_s(self, full_s: float) -> float:
        if self.settle_s is None:
            return min(LOCKOUT_MIN_DEFAULT_S, full_s)
        return min(max(2.0 * self.settle_s, LOCKOUT_MIN_FLOOR_S), full_s)


@dataclass
class PendingDose:
    ts: float
    reagent: str
    ml: float  # 0.0 when unknown (foreign)
    pre_ph: float | None


@dataclass
class Response:
    ts: float  # dose ts
    reagent: str
    ml: float
    pre_ph: float | None
    post_ph: float  # the trough
    settled_ts: float
    learned: bool  # contributed to the posterior

    @property
    def drop(self) -> float | None:
        return None if self.pre_ph is None else self.pre_ph - self.post_ph


def _ewma(current: float | None, obs: float, alpha: float = EWMA) -> float:
    return obs if current is None else current + alpha * (obs - current)


# -- Model B: feed every pH sample ------------------------------------------------


def ingest_ph(m: Model, value: float, ts: float) -> None:
    """Kalman predict + update with one pH sample; R adapts to the innovations."""
    if m.kf_level is None or m.kf_ts is None or ts < m.kf_ts:
        m.kf_level, m.kf_slope, m.kf_ts = value, 0.0, ts
        m.kf_p = [[m.kf_r, 0.0], [0.0, 1e-6]]
        return
    dt = max(ts - m.kf_ts, 1e-3)
    dosing = m.trough_ph is not None  # a response is pending: the level may step
    # predict
    level = m.kf_level + m.kf_slope * dt
    p = m.kf_p
    q_level = Q_LEVEL_DOSING if dosing else Q_LEVEL
    p00 = p[0][0] + dt * (p[0][1] + p[1][0]) + dt * dt * p[1][1] + q_level * dt
    p01 = p[0][1] + dt * p[1][1]
    p11 = p[1][1] + Q_SLOPE * dt
    # update (H = [1, 0])
    innov = value - level
    s = p00 + m.kf_r
    g0, g1 = p00 / s, p01 / s
    m.kf_level = level + g0 * innov
    m.kf_slope = m.kf_slope + g1 * innov
    m.kf_p = [[p00 - g0 * p00, p01 - g0 * p01], [p01 - g1 * p00, p11 - g1 * p01]]
    m.kf_ts = ts
    if dosing:
        # trough bookkeeping for the pending dose (see observe); R is not
        # adapted here — a step is not probe noise
        if m.kf_level < m.trough_ph:
            m.trough_ph, m.trough_ts = m.kf_level, ts
    else:
        # adaptive R: innovation variance minus what the state already explains
        r_obs = innov * innov - p00
        m.kf_r = min(R_CAP, max(R_FLOOR, m.kf_r + R_EWMA * (r_obs - m.kf_r)))


def current_ph(m: Model) -> float | None:
    return m.kf_level


# -- recording -------------------------------------------------------------------------


def note_dose(ledger: Ledger, ts: float, reagent: str, ml: float, pre_ph: float | None) -> None:
    """A dose just happened: track its response (supersedes a still-pending
    one — two doses inside one settle window cannot be told apart, so
    neither is learned from)."""
    m = ledger.model
    if pre_ph is None:
        pre_ph = m.kf_level
    ledger.pending = PendingDose(ts, reagent, ml, pre_ph)
    m.trough_ph = m.kf_level if m.kf_level is not None else pre_ph
    m.trough_ts = ts
    m.kf_p[0][0] += DOSE_JUMP_VAR  # the level is about to jump: let the filter follow


# -- observation step (once per evaluation cycle) ---------------------------------------


def observe(now: float, ledger: Ledger, cfg: Config) -> Response | None:
    """Settle the pending dose once its trough has not deepened for
    SETTLE_WINDOW_S; otherwise, in quiet periods, read the rebound.
    Returns the settled response for logging, else None."""
    if not cfg.adaptive.enabled:
        return None
    m = ledger.model
    p = ledger.pending
    if p is None:
        _learn_rebound(now, ledger)
        return None
    full_s = cfg.dosing.lockout_minutes * 60
    if now - p.ts > full_s:
        ledger.pending = None  # never settled inside the lockout: no lesson
        m.trough_ph = m.trough_ts = None
        return None
    if now - p.ts < SETTLE_WINDOW_S or m.trough_ph is None or m.trough_ts is None:
        return None
    if now - m.trough_ts < SETTLE_WINDOW_S:
        return None  # still deepening

    post_ph = m.trough_ph
    learned = False
    if p.pre_ph is not None:
        drop = p.pre_ph - post_ph
        if p.reagent == PH_DOWN:
            if drop < m.no_response_eps():
                m.no_response_streak += 1
            else:
                m.no_response_streak = 0
                learned = _learn_response(m, p.ml, drop, post_ph, cfg)
                m.settle_s = _ewma(m.settle_s, now - p.ts)
        elif p.reagent == FOREIGN and drop >= m.no_response_eps():
            m.no_response_streak = 0  # the line demonstrably delivers
            m.settle_s = _ewma(m.settle_s, now - p.ts)
    resp = Response(p.ts, p.reagent, p.ml, p.pre_ph, round(post_ph, 3), now, learned)
    ledger.responses.append(resp)
    del ledger.responses[:-RESPONSE_HISTORY]
    ledger.settled_dose_ts = p.ts
    ledger.pending = None
    m.trough_ph = None
    return resp


def _learn_response(m: Model, ml: float, drop: float, post_ph: float, cfg: Config) -> bool:
    if ml <= 0:
        return False
    sigma2 = m.drop_sigma2()
    mu, sd = m.predict_drop(ml, sigma2)
    if abs(drop - mu) > OUTLIER_Z * sd:
        return False  # not chemistry as the model knows it — leave the posterior alone
    m.update_response(ml, drop, sigma2)
    if post_ph < cfg.bands.ph_tol_low:
        m.overshoots += 1
    return True


def _learn_rebound(now: float, ledger: Ledger) -> None:
    """A significantly positive slope, read 15 min to 2 h after the last
    trough with no dose since, is the rebound rate."""
    m = ledger.model
    if not ledger.responses or m.kf_level is None:
        return
    r = ledger.responses[-1]
    if r.reagent not in (PH_DOWN, FOREIGN) or r.drop is None:
        return
    last = ledger.last_dose_ts()
    if last is not None and last > r.ts + 1.0:
        return
    since = now - r.settled_ts
    if since < REBOUND_MIN_S or since > REBOUND_MAX_S:
        return
    if m.kf_slope > GUARD_Z * m.slope_sd:
        m.rebound_ph_h = round(_ewma(m.rebound_ph_h, m.kf_slope * 3600.0), 4)


# -- lockout ---------------------------------------------------------------------------------


def lockout_remaining(now: float, ledger: Ledger, cfg: Config) -> float:
    """Seconds until dosing is allowed. Early release needs *evidence*: the
    most recent dose must have a settled response and the learned minimum
    must have elapsed."""
    last = ledger.last_dose_ts()
    if last is None:
        return 0.0
    full_s = cfg.dosing.lockout_minutes * 60
    elapsed = now - last
    if elapsed >= full_s:
        return 0.0
    if not cfg.adaptive.enabled:
        return full_s - elapsed
    settled = ledger.settled_dose_ts is not None and abs(ledger.settled_dose_ts - last) < 1.0
    if settled:
        return max(0.0, ledger.model.lockout_min_s(full_s) - elapsed)
    return full_s - elapsed


# -- planning ----------------------------------------------------------------------------------


def aim_ph(ledger: Ledger, cfg: Config) -> float:
    """Where a dose should land: the top of the band, pulled toward the
    bottom by however far the learned rebound climbs in REBOUND_HORIZON_H."""
    lo, hi = cfg.bands.ph_target_low, cfg.bands.ph_target_high
    rebound = ledger.model.rebound_ph_h or 0.0
    return min(max(hi - rebound * REBOUND_HORIZON_H, lo), hi)


def plan_ph_dose(ph: float, ledger: Ledger, cfg: Config) -> tuple[float, str]:
    """Size a pH-Down dose for a confirmed pH above ph_high_full.

    model : posterior mean drop == need   → (need + b) / k
    guard : largest ml whose 97.5 % upper predictive drop keeps pH >= ph_tol_low
    rails : <= ph_max_single_dose_ml, >= ph_fine_dose_ml, RESOLUTION_ML
    """
    a, d, m = cfg.adaptive, cfg.dosing, ledger.model
    if not a.enabled:
        return d.ph_full_dose_ml, f"fixed {d.ph_full_dose_ml:.1f} ml (adaptive off)"
    aim = aim_ph(ledger, cfg)
    need = ph - aim
    sigma2 = m.drop_sigma2()
    model_ml = (need + m.b) / m.k if m.k > 0.05 else a.ph_max_single_dose_ml
    room = ph - cfg.bands.ph_tol_low
    guard_ml = 0.0
    steps = int(round(a.ph_max_single_dose_ml / RESOLUTION_ML))
    for i in range(1, steps + 1):
        ml_i = i * RESOLUTION_ML
        mu, sd = m.predict_drop(ml_i, sigma2)
        if mu + GUARD_Z * sd <= room:
            guard_ml = ml_i
        else:
            break
    raw = min(model_ml, guard_ml)
    which = "model" if model_ml <= guard_ml else "guard"
    ml = min(raw, a.ph_max_single_dose_ml)
    ml = max(ml, d.ph_fine_dose_ml)
    ml = round(math.floor(ml / RESOLUTION_ML + 1e-9) * RESOLUTION_ML, 3)
    mu, sd = m.predict_drop(ml, sigma2)
    return ml, (
        f"{ml:.1f} ml by {which} (model {model_ml:.2f} / guard {guard_ml:.2f}); "
        f"k={m.k:.2f}±{m.k_sd:.2f} b={m.b:.2f} n={m.n}; predicted drop {mu:.2f}±{sd:.2f}; "
        f"aim {aim:.2f} (need -{need:.2f})"
    )


# -- ledger persistence (retained JSON, v2) --------------------------------------------------


def ledger_to_dict(ledger: Ledger) -> dict:
    return {
        "v": 2,
        "doses": ledger.doses,
        "pending": asdict(ledger.pending) if ledger.pending else None,
        "settled_dose_ts": ledger.settled_dose_ts,
        "responses": [asdict(r) for r in ledger.responses],
        "model": asdict(ledger.model),
    }


_INT_FIELDS = {"n", "no_response_streak", "overshoots"}
_MATRIX_FIELDS = {"cov", "kf_p"}


def ledger_from_dict(ledger: Ledger, data: dict) -> None:
    """Restore a v1 (doses only) or v2 ledger into ``ledger`` in place.
    Unknown model fields are ignored, missing ones keep their default, so
    the schema can grow. Raises ValueError/TypeError/KeyError on garbage."""
    ledger.doses = [
        (float(ts), str(reagent), float(ml)) for ts, reagent, ml in data.get("doses", [])
    ]
    if int(data.get("v", 1)) < 2:
        return
    p = data.get("pending")
    ledger.pending = (
        PendingDose(
            float(p["ts"]),
            str(p["reagent"]),
            float(p["ml"]),
            None if p.get("pre_ph") is None else float(p["pre_ph"]),
        )
        if p
        else None
    )
    s = data.get("settled_dose_ts")
    ledger.settled_dose_ts = None if s is None else float(s)
    ledger.responses = [
        Response(
            float(r["ts"]),
            str(r["reagent"]),
            float(r["ml"]),
            None if r.get("pre_ph") is None else float(r["pre_ph"]),
            float(r["post_ph"]),
            float(r["settled_ts"]),
            bool(r.get("learned", False)),
        )
        for r in data.get("responses", [])
    ]
    raw = data.get("model") or {}
    m = Model()
    for f in fields(Model):
        if f.name not in raw:
            continue
        v = raw[f.name]
        if v is None:
            setattr(m, f.name, None)
        elif f.name in _MATRIX_FIELDS:
            setattr(m, f.name, [[float(a) for a in row] for row in v])
        elif f.name in _INT_FIELDS:
            setattr(m, f.name, int(v))
        else:
            setattr(m, f.name, float(v))
    ledger.model = m
