"""Configuration loading for Demeter.

YAML file with ``${VAR}`` environment expansion (zeus pattern: secrets come
in as env vars from the sealed secret, everything else is plain config).
Defaults mirror the Tethys playbook and the 2026-09-09 DFR0523 calibration
(docs/dosing/dfr0523.md); the gitops values file overrides per environment.
"""

from __future__ import annotations

import os
import re
from dataclasses import dataclass, field

import yaml

_ENV_RE = re.compile(r"\$\{([A-Z0-9_]+)\}")


def _expand_env(value):
    if isinstance(value, str):
        return _ENV_RE.sub(lambda m: os.environ.get(m.group(1), ""), value)
    if isinstance(value, dict):
        return {k: _expand_env(v) for k, v in value.items()}
    if isinstance(value, list):
        return [_expand_env(v) for v in value]
    return value


@dataclass
class MqttConfig:
    host: str = "mqtt.lab.local"
    port: int = 1883
    client_id: str = "pomona-demeter"
    username: str = ""
    password: str = ""
    base_topic: str = "pomona"


@dataclass
class PumpChannel:
    """One calibrated DFR0523 channel.

    ml/s constants are per-channel AND per-speed (the speed->flow curve is
    nonlinear — never interpolate; docs/dosing/dfr0523.md).
    """

    channel: int = 0
    full_ml_s: float = 0.0
    slow_ml_s: float = 0.0
    slow_speed: int = 50


@dataclass
class Bands:
    ph_target_low: float = 5.8
    ph_target_high: float = 6.2
    ph_tol_low: float = 5.4
    ph_tol_high: float = 6.5
    ph_high_full: float = 7.0  # above this: full 1 ml dose; tol_high..this: fine dose
    ec_target_low: float = 0.8
    ec_target_high: float = 1.0
    ec_tol_low: float = 0.7
    ec_tol_high: float = 1.1
    water_temp_max_c: float = 26.5


@dataclass
class DosingConfig:
    lockout_minutes: float = 60.0
    ph_full_dose_ml: float = 1.0
    ph_fine_dose_ml: float = 0.5
    ph_daily_cap_ml: float = 4.0
    nutrient_dose_ml: float = 5.0
    nutrient_gap_seconds: float = 120.0
    nutrient_max_per_24h: int = 1
    # Firmware DOSE_TEST_MAX_MS — a computed run longer than this is a config
    # error, not something to clamp-and-send (a clamped dose is a WRONG dose).
    max_command_ms: int = 10000
    ph_down: PumpChannel = field(
        default_factory=lambda: PumpChannel(1, full_ml_s=0.48, slow_ml_s=0.11)
    )
    nutrient_a: PumpChannel = field(
        default_factory=lambda: PumpChannel(2, full_ml_s=0.66, slow_ml_s=0.24)
    )
    nutrient_b: PumpChannel = field(
        default_factory=lambda: PumpChannel(3, full_ml_s=0.60, slow_ml_s=0.11)
    )


@dataclass
class AdaptiveConfig:
    """The self-learning brain (demeter/adapt.py). Two rails, no tuning:
    everything else — sensitivity, probe noise, settle time, rebound, how
    big a dose may grow — is observed from the reservoir and persisted in
    the retained ledger."""

    enabled: bool = True
    ph_max_single_dose_ml: float = 2.0  # no single pH-Down dose ever exceeds this


@dataclass
class Config:
    mode: str = "shadow"  # shadow | active
    interval_seconds: float = 60.0
    freshness_seconds: float = 120.0
    confirm_minutes: float = 10.0
    metrics_port: int = 9000
    mqtt: MqttConfig = field(default_factory=MqttConfig)
    bands: Bands = field(default_factory=Bands)
    dosing: DosingConfig = field(default_factory=DosingConfig)
    adaptive: AdaptiveConfig = field(default_factory=AdaptiveConfig)


def _apply(obj, data: dict):
    for key, value in (data or {}).items():
        if not hasattr(obj, key):
            raise KeyError(f"unknown config key: {key}")
        current = getattr(obj, key)
        if isinstance(value, dict) and not isinstance(current, (int, float, str)):
            _apply(current, value)
        elif isinstance(current, bool):
            if isinstance(value, str):
                value = value.strip().lower() in ("1", "true", "yes", "on")
            setattr(obj, key, bool(value))
        else:
            setattr(obj, key, type(current)(value) if current is not None else value)


def load(path: str) -> Config:
    with open(path, "r", encoding="utf-8") as fh:
        raw = _expand_env(yaml.safe_load(fh) or {})
    cfg = Config()
    for section in ("mqtt", "bands", "dosing", "adaptive"):
        if section in raw:
            _apply(getattr(cfg, section), raw.pop(section))
    _apply(cfg, raw)
    if cfg.mode not in ("shadow", "active"):
        raise ValueError(f"mode must be shadow|active, got {cfg.mode!r}")
    return cfg
