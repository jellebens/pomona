"""Self-learning brain tests — Bayesian dose response, Kalman pH signal,
settle detection, dynamic lockout, rebound-aware aiming, persistence."""

import json
from dataclasses import fields

import pytest

from demeter import adapt, engine
from demeter.config import AdaptiveConfig, Config
from demeter.engine import Ledger, Sample, Telemetry

T0 = 1_700_000_000.0
MIN = 60.0


def state(ph=8.2, ec=0.9, wtemp=20.0, online=True):
    telem = Telemetry(
        ph=Sample(ph, T0), ec=Sample(ec, T0), water_temp=Sample(wtemp, T0), unit_online=online
    )
    return telem, Ledger(), Config()


def feed(model, points, step=30.0):
    """points: list of (ts, ph) anchors; samples every `step` s, linear between anchors."""
    for (t0, v0), (t1, v1) in zip(points, points[1:]):
        t = t0
        while t < t1:
            adapt.ingest_ph(model, v0 + (v1 - v0) * (t - t0) / (t1 - t0), t)
            t += step
    adapt.ingest_ph(model, points[-1][1], points[-1][0])


def flat(model, ph, start, end, step=30.0):
    feed(model, [(start, ph), (end, ph)], step)


def confirmed(telem, ledger, cfg, now=T0):
    engine.decide(now, telem, ledger, cfg)
    later = now + cfg.confirm_minutes * MIN
    for name in ("ph", "ec", "water_temp"):
        s = getattr(telem, name)
        if s:
            setattr(telem, name, Sample(s.value, later))
    return engine.decide(later, telem, ledger, cfg), later


def tonight(ledger, cfg, ts=T0, ml=2.0, reagent="ph_down"):
    """The 2026-09-10 20:04Z dose: 8.25 -> 5.91 in 4 min, then ~flat."""
    m = ledger.model
    flat(m, 8.25, ts - 10 * MIN, ts)
    ledger.record(ts, reagent, ml)
    adapt.note_dose(ledger, ts, reagent, ml, None)
    feed(m, [(ts, 8.25), (ts + 1 * MIN, 8.2), (ts + 2 * MIN, 6.8), (ts + 3 * MIN, 6.15),
             (ts + 4 * MIN, 5.91), (ts + 6 * MIN, 5.95), (ts + 12 * MIN, 5.99)])
    return adapt.observe(ts + 12 * MIN, ledger, cfg)


# -- config is two rails ---------------------------------------------------------------


def test_adaptive_config_is_minimal():
    assert {f.name for f in fields(AdaptiveConfig)} == {"enabled", "ph_max_single_dose_ml"}


# -- Model A: sizing from the posterior --------------------------------------------------


@pytest.mark.parametrize(
    "ph, expected_ml, which",
    [
        (7.4, 0.8, "guard"),   # wide prior: the 97.5 % upper drop binds well before the mean
        (8.24, 1.2, "guard"),
        (9.5, 1.9, "guard"),   # room 4.1: 1.9 ml -> 2.07+2*1.16=4.39 no... see assertion below
    ],
)
def test_prior_dose_is_uncertainty_limited(ph, expected_ml, which):
    _, ledger, cfg = state()
    ml, note = adapt.plan_ph_dose(ph, ledger, cfg)
    assert which in note
    if ph == 9.5:
        assert 1.5 <= ml <= cfg.adaptive.ph_max_single_dose_ml
    else:
        assert ml == pytest.approx(expected_ml)
    # the guard is real: the upper predictive drop never eats the room
    mu, sd = ledger.model.predict_drop(ml, ledger.model.drop_sigma2())
    assert mu + adapt.GUARD_Z * sd <= ph - cfg.bands.ph_tol_low + 1e-9


def test_evidence_tightens_the_posterior_and_grows_the_dose():
    _, ledger, cfg = state()
    before, _ = adapt.plan_ph_dose(8.24, ledger, cfg)
    sd_before = ledger.model.k_sd
    resp = tonight(ledger, cfg)
    assert resp is not None and resp.learned and ledger.model.n == 1
    assert resp.drop == pytest.approx(2.3, abs=0.2)
    assert ledger.model.k_sd < sd_before
    after, note = adapt.plan_ph_dose(8.24, ledger, cfg)
    assert after > before
    assert after <= cfg.adaptive.ph_max_single_dose_ml
    assert "n=1" in note


def test_posterior_update_is_exact_for_one_observation():
    m = adapt.Model()
    m.update_response(2.0, 2.3, 0.02)
    # Kalman-form BLR: x=(2,-1), P x = (0.72, -0.09), S = 1.44+0.09+0.02 = 1.55
    # innovation = 2.3 - (2.6-0.4) = 0.1 -> k += 0.72/1.55*0.1, b += -0.09/1.55*0.1
    assert m.k == pytest.approx(1.3 + 0.0465, abs=1e-3)
    assert m.b == pytest.approx(0.4 - 0.0058, abs=1e-3)
    assert m.cov[0][0] < adapt.PRIOR_K_SD**2


def test_strong_reservoir_learned_means_smaller_dose():
    _, ledger, cfg = state()
    m = ledger.model
    for _ in range(3):  # 1 ml drops 2.0 pH, repeatedly
        m.update_response(1.0, 2.0, m.drop_sigma2())
    ml, note = adapt.plan_ph_dose(7.4, ledger, cfg)
    assert ml == pytest.approx(0.6) and "model" in note  # (1.2 + b) / ~2.0, floored to 0.1 ml
    # three doses of the same size pin k*1 - b tightly, k and b individually less so
    assert m.k > 1.8 and m.k_sd < adapt.PRIOR_K_SD / 2


def test_plan_respects_the_rails():
    _, ledger, cfg = state()
    m = ledger.model
    m.k, m.b, m.cov = 0.2, 0.0, [[1e-6, 0.0], [0.0, 1e-6]]  # a very weak, well-known reservoir
    assert adapt.plan_ph_dose(9.0, ledger, cfg)[0] == cfg.adaptive.ph_max_single_dose_ml
    m.k = 10.0
    assert adapt.plan_ph_dose(7.01, ledger, cfg)[0] == cfg.dosing.ph_fine_dose_ml


def test_outlier_is_not_learned():
    _, ledger, cfg = state()
    m = ledger.model
    m.cov = [[0.01, 0.0], [0.0, 0.01]]  # confident posterior
    assert adapt._learn_response(m, 1.0, 5.0, 3.0, cfg) is False  # 1 ml cannot drop 5 pH
    assert m.n == 0


def test_engine_sizes_the_full_tier_dose_and_explains():
    telem, ledger, cfg = state(ph=8.24)
    decision, _ = confirmed(telem, ledger, cfg)
    assert decision.action == engine.DOSE_PH_FULL
    (step,) = decision.steps
    assert step.ml == pytest.approx(1.2)
    assert step.command() == "ch1 fwd 2500"  # 1.2 ml @ 0.48 ml/s
    assert "predicted drop" in decision.reason


def test_fine_tier_stays_fixed():
    telem, ledger, cfg = state(ph=6.7)
    decision, _ = confirmed(telem, ledger, cfg)
    assert decision.action == engine.DOSE_PH_FINE and decision.steps[0].ml == 0.5


def test_adaptive_off_is_the_old_playbook():
    telem, ledger, cfg = state()
    cfg.adaptive.enabled = False
    assert adapt.plan_ph_dose(8.24, ledger, cfg)[0] == cfg.dosing.ph_full_dose_ml
    ledger.record(T0, "ph_down", 1.0)
    ledger.settled_dose_ts = T0
    assert adapt.lockout_remaining(T0 + 30 * MIN, ledger, cfg) == pytest.approx(30 * MIN)


# -- Model B: the Kalman filter on the pH signal --------------------------------------------


def test_filter_learns_the_probe_noise():
    m = adapt.Model()
    t, v = T0, 8.13
    while t <= T0 + 20 * MIN:  # the real probe's 0.11 sawtooth
        adapt.ingest_ph(m, v, t)
        v = 8.24 if v == 8.13 else 8.13
        t += 30
    assert 0.03 < m.noise_sd < 0.12
    assert abs(m.kf_level - 8.185) < 0.08
    assert m.no_response_eps() >= adapt.NO_RESPONSE_FLOOR


def test_filter_tracks_a_dose_and_finds_the_trough():
    _, ledger, cfg = state()
    resp = tonight(ledger, cfg)
    assert resp is not None
    assert resp.pre_ph == pytest.approx(8.25, abs=0.02)
    assert resp.post_ph == pytest.approx(5.93, abs=0.12)
    assert ledger.model.settle_s == pytest.approx(12 * MIN, abs=MIN)


def test_no_response_is_counted_not_learned():
    _, ledger, cfg = state()
    m = ledger.model
    flat(m, 8.2, T0 - 10 * MIN, T0)
    ledger.record(T0, "ph_down", 1.0)
    adapt.note_dose(ledger, T0, "ph_down", 1.0, None)
    flat(m, 8.2, T0, T0 + 6 * MIN)  # water in the line: nothing happened
    resp = adapt.observe(T0 + 6 * MIN, ledger, cfg)
    assert resp is not None and not resp.learned
    assert m.n == 0 and m.no_response_streak == 1 and m.settle_s is None


def test_three_no_responses_alert_instead_of_more_acid():
    telem, ledger, cfg = state(ph=8.2)
    ledger.model.no_response_streak = adapt.NO_RESPONSE_ALERT_STREAK
    decision, _ = confirmed(telem, ledger, cfg)
    assert decision.action == engine.ALERT
    assert "no pH response" in decision.reason


def test_foreign_dose_with_response_clears_streak_but_teaches_nothing():
    _, ledger, cfg = state()
    ledger.model.no_response_streak = 1
    resp = tonight(ledger, cfg, ml=0.0, reagent="foreign")
    assert resp is not None and not resp.learned
    assert ledger.model.no_response_streak == 0 and ledger.model.n == 0


def test_unsettled_pending_is_dropped_after_full_lockout():
    _, ledger, cfg = state()
    adapt.note_dose(ledger, T0, "ph_down", 1.0, 8.2)
    assert adapt.observe(T0 + 61 * MIN, ledger, cfg) is None
    assert ledger.pending is None and ledger.model.n == 0


# -- dynamic lockout ---------------------------------------------------------------------------


def test_lockout_releases_early_only_with_settled_response():
    _, ledger, cfg = state()
    m = ledger.model
    flat(m, 8.2, T0 - 10 * MIN, T0)
    ledger.record(T0, "ph_down", 1.0)
    adapt.note_dose(ledger, T0, "ph_down", 1.0, None)
    assert adapt.lockout_remaining(T0 + 4 * MIN, ledger, cfg) == pytest.approx(56 * MIN)
    flat(m, 8.2, T0, T0 + 6 * MIN)
    assert adapt.observe(T0 + 6 * MIN, ledger, cfg) is not None
    # no settle time learned from a no-response: default 10 min minimum
    assert adapt.lockout_remaining(T0 + 6 * MIN, ledger, cfg) == pytest.approx(4 * MIN)
    assert adapt.lockout_remaining(T0 + 10 * MIN, ledger, cfg) == 0.0


def test_lockout_minimum_tracks_the_learned_settle_time():
    _, ledger, cfg = state()
    tonight(ledger, cfg)  # settles ~12 min after the dose
    m = ledger.model
    assert m.lockout_min_s(3600) == pytest.approx(2 * m.settle_s)
    m.settle_s = 1 * MIN
    assert m.lockout_min_s(3600) == adapt.LOCKOUT_MIN_FLOOR_S
    m.settle_s = 3 * 3600
    assert m.lockout_min_s(3600) == 3600


def test_engine_redoses_after_early_release():
    """Tonight's case: a dose that did nothing settles -> re-dose at 10 min, not 60."""
    telem, ledger, cfg = state(ph=8.2)
    m = ledger.model
    engine.decide(T0 - 20 * MIN, telem, ledger, cfg)  # violation already confirmed
    flat(m, 8.2, T0 - 20 * MIN, T0)
    ledger.record(T0, "ph_down", 1.0)
    adapt.note_dose(ledger, T0, "ph_down", 1.0, None)
    flat(m, 8.2, T0, T0 + 12 * MIN)
    telem.ph = Sample(8.2, T0 + 12 * MIN)
    decision = engine.decide(T0 + 12 * MIN, telem, ledger, cfg)
    assert decision.action == engine.DOSE_PH_FULL
    assert decision.steps[0].ml == pytest.approx(1.2)
    assert m.no_response_streak == 1


# -- rebound-aware aiming ----------------------------------------------------------------------


def test_rebound_is_learned_and_lowers_the_aim():
    _, ledger, cfg = state()
    assert adapt.aim_ph(ledger, cfg) == cfg.bands.ph_target_high
    tonight(ledger, cfg)
    m = ledger.model
    # climb 1.2 pH/h from the trough for 40 min, no dose since
    feed(m, [(T0 + 12 * MIN, 5.99), (T0 + 52 * MIN, 5.99 + 0.8)])
    for k in range(13, 53, 2):
        adapt.observe(T0 + k * MIN, ledger, cfg)
    assert m.rebound_ph_h == pytest.approx(1.2, abs=0.4)
    assert adapt.aim_ph(ledger, cfg) < cfg.bands.ph_target_high
    m.rebound_ph_h = 0.4
    assert adapt.aim_ph(ledger, cfg) == pytest.approx(6.0)
    m.rebound_ph_h = 3.0
    assert adapt.aim_ph(ledger, cfg) == cfg.bands.ph_target_low


def test_rebound_not_measured_across_a_newer_dose():
    _, ledger, cfg = state()
    tonight(ledger, cfg)
    ledger.record(T0 + 14 * MIN, "foreign", 0.0)  # someone dosed again
    feed(ledger.model, [(T0 + 12 * MIN, 5.99), (T0 + 52 * MIN, 6.79)])
    adapt.observe(T0 + 52 * MIN, ledger, cfg)
    assert ledger.model.rebound_ph_h is None


# -- persistence --------------------------------------------------------------------------------


def test_ledger_v2_roundtrip_keeps_everything_learned():
    _, ledger, cfg = state()
    tonight(ledger, cfg)
    ledger.model.rebound_ph_h = 1.2
    ledger.record(T0 + 20 * MIN, "foreign", 0.0)
    adapt.note_dose(ledger, T0 + 20 * MIN, "foreign", 0.0, 6.0)

    payload = json.dumps(adapt.ledger_to_dict(ledger))
    restored = Ledger()
    adapt.ledger_from_dict(restored, json.loads(payload))
    assert restored.doses == ledger.doses
    assert restored.model == ledger.model
    assert restored.settled_dose_ts == T0
    assert restored.pending == ledger.pending
    assert [r.post_ph for r in restored.responses] == [r.post_ph for r in ledger.responses]


def test_ledger_v1_still_loads():
    restored = Ledger()
    adapt.ledger_from_dict(restored, {"v": 1, "doses": [[T0, "ph_down", 1.0]]})
    assert restored.doses == [(T0, "ph_down", 1.0)]
    assert restored.model == adapt.Model() and restored.pending is None


def test_unknown_model_fields_are_ignored():
    restored = Ledger()
    adapt.ledger_from_dict(
        restored, {"v": 2, "doses": [], "model": {"k": 1.1, "future_field": 42}}
    )
    assert restored.model.k == 1.1
