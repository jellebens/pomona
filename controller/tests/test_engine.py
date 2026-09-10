"""Decision-engine tests — the playbook rails, mechanically checked."""

import pytest

from demeter import engine
from demeter.config import Config
from demeter.engine import Ledger, Sample, Telemetry

T0 = 1_700_000_000.0
CONFIRM_S = 10 * 60
LOCKOUT_S = 60 * 60


def make_state(ph=6.0, ec=0.9, wtemp=20.0, online=True, age=0.0):
    telem = Telemetry(
        ph=Sample(ph, T0 - age),
        ec=Sample(ec, T0 - age),
        water_temp=Sample(wtemp, T0 - age),
        unit_online=online,
    )
    return telem, Ledger(), Config()


def confirm(telem, ledger, cfg, now=T0):
    """Run decide() at streak start and again after the confirm window."""
    engine.decide(now, telem, ledger, cfg)
    # keep samples fresh at the later evaluation
    later = now + CONFIRM_S
    for name in ("ph", "ec", "water_temp"):
        sample = getattr(telem, name)
        if sample:
            setattr(telem, name, Sample(sample.value, later))
    return engine.decide(later, telem, ledger, cfg), later


def test_in_band_is_quiet():
    telem, ledger, cfg = make_state()
    assert engine.decide(T0, telem, ledger, cfg).action == engine.NONE


def test_ph_high_needs_confirmation():
    telem, ledger, cfg = make_state(ph=7.4)
    first = engine.decide(T0, telem, ledger, cfg)
    assert first.action == engine.NONE  # not yet confirmed


def test_ph_above_7_doses_full_speed_sized_by_the_brain():
    telem, ledger, cfg = make_state(ph=7.4)
    decision, _ = confirm(telem, ledger, cfg)
    assert decision.action == engine.DOSE_PH_FULL
    (step,) = decision.steps
    # nothing learned yet: the wide prior's 97.5 % upper drop binds at 0.8 ml
    assert step.ml == 0.8
    assert step.command() == "ch1 fwd 1667"  # 0.8 ml @ 0.48 ml/s
    cfg.adaptive.enabled = False
    decision, _ = confirm(telem, ledger, cfg)
    assert decision.steps[0].command() == "ch1 fwd 2083"  # the fixed 1 ml playbook


def test_ph_marginal_doses_half_ml_slow():
    telem, ledger, cfg = make_state(ph=6.7)
    decision, _ = confirm(telem, ledger, cfg)
    assert decision.action == engine.DOSE_PH_FINE
    (step,) = decision.steps
    assert step.command() == "ch1 fwd 4545 50"  # 0.5 ml @ 0.11 ml/s
    assert step.ml == 0.5


def test_ec_low_doses_a_then_b_with_gap():
    telem, ledger, cfg = make_state(ec=0.6)
    decision, _ = confirm(telem, ledger, cfg)
    assert decision.action == engine.DOSE_NUTRIENTS
    a, b = decision.steps
    assert (a.reagent, a.command()) == ("nutrient_a", "ch2 fwd 7576")
    assert (b.reagent, b.command()) == ("nutrient_b", "ch3 fwd 8333")
    assert b.delay_before_s == 120


def test_ph_takes_priority_over_ec():
    telem, ledger, cfg = make_state(ph=7.4, ec=0.6)
    decision, _ = confirm(telem, ledger, cfg)
    assert decision.action == engine.DOSE_PH_FULL


def test_lockout_blocks_dose():
    telem, ledger, cfg = make_state(ph=7.4)
    ledger.record(T0, "foreign", 0.0)
    decision, _ = confirm(telem, ledger, cfg)
    assert decision.action == engine.BLOCKED
    assert "lockout" in decision.reason


def test_lockout_expiry_allows_dose():
    telem, ledger, cfg = make_state(ph=7.4)
    ledger.record(T0 - LOCKOUT_S - CONFIRM_S, "foreign", 0.0)
    decision, _ = confirm(telem, ledger, cfg)
    assert decision.action == engine.DOSE_PH_FULL


def test_acid_cap_alerts_instead_of_dosing():
    telem, ledger, cfg = make_state(ph=7.4)
    # 4 ml already delivered in window, oldest well past lockout
    for i in range(4):
        ledger.record(T0 - LOCKOUT_S - CONFIRM_S - i * 3600, "ph_down", 1.0)
    decision, _ = confirm(telem, ledger, cfg)
    assert decision.action == engine.ALERT
    assert "acid cap" in decision.reason


def test_nutrient_budget_once_per_24h():
    telem, ledger, cfg = make_state(ec=0.6)
    ledger.record(T0 - 5 * 3600, "nutrient_a", 5.0)
    ledger.record(T0 - 5 * 3600 + 120, "nutrient_b", 5.0)
    decision, _ = confirm(telem, ledger, cfg)
    assert decision.action == engine.BLOCKED
    assert "budget" in decision.reason


def test_stale_reading_never_doses():
    telem, ledger, cfg = make_state(ph=7.4)
    confirm(telem, ledger, cfg)  # builds the streak
    telem.ph = Sample(7.4, T0)  # now stale relative to the late evaluation
    decision = engine.decide(T0 + CONFIRM_S + 600, telem, ledger, cfg)
    assert decision.action == engine.NONE  # stale sample resets the streak


def test_unit_offline_blocks():
    telem, ledger, cfg = make_state(ph=7.4, online=False)
    decision, _ = confirm(telem, ledger, cfg)
    assert decision.action == engine.BLOCKED
    assert "online" in decision.reason


@pytest.mark.parametrize(
    "kwargs, condition",
    [({"ph": 5.0}, "ph_low"), ({"ec": 1.5}, "ec_high"), ({"wtemp": 28.0}, "wtemp_high")],
)
def test_no_reagent_conditions_alert_only(kwargs, condition):
    telem, ledger, cfg = make_state(**kwargs)
    decision, _ = confirm(telem, ledger, cfg)
    assert decision.action == engine.ALERT
    assert decision.condition == condition
    assert not decision.steps


def test_command_ms_never_exceeds_firmware_cap():
    telem, ledger, cfg = make_state(ec=0.6)
    cfg.dosing.nutrient_dose_ml = 50.0  # would need ~76 s on ch2
    decision, _ = confirm(telem, ledger, cfg)
    assert decision.action == engine.BLOCKED
    assert "refusing dose" in decision.reason
