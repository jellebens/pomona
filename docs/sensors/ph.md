# pH (continuous) — DFRobot Gravity pH Pro SEN0169-V2 + DFR0504 isolator

**Status: to wire; calibration blocked on buffers.** Industrial probe rated
for 24/7 immersion. The PH-201H handheld pen is the calibration
cross-check. Target: pH ~6.0 (window 5.8–6.2) — see
[planting-plan.md](../planting-plan.md).

## Wiring

```
probe (in tank) ─BNC→ pH Pro transmitter board ─3-pin→ DFR0504 isolator ─3-pin→ GIGA
                                                                            (A1 + 3V3 + GND)
```

Analog out → **A1**. Gravity analog cables: black = GND, red = VCC (3V3),
blue = signal.

The DFR0504 galvanically isolates supply *and* signal. It is **not
optional**: the pH and [TDS](ec-tds.md) probes share the same 10 L of
water, and without isolation the TDS probe's excitation couples into the
pH reading (ground loop).

## Calibration (two-point)

Probe in **6.86** buffer → record `PH_V_NEUTRAL`; rinse; probe in **4.01**
buffer → record `PH_V_ACID` (constants in `bringup.ino`; slope/offset are
derived in the sketch). Recalibrate roughly monthly; cross-check any odd
reading with the PH-201H pen before believing it. Calibrate with the
[DS18B20](water-temp.md) in the same liquid.

| Date | PH_V_NEUTRAL (V @ 6.86) | PH_V_ACID (V @ 4.01) | Notes |
|---|---|---|---|
| _(pending first calibration)_ | | | |

## Care

The probe is a consumable (6–12 months). Store with the KCl cap on
whenever the system is drained. Leave the BNC capped until the analog
chain is verified (first-power-on checklist in [wiring.md](../wiring.md)).
