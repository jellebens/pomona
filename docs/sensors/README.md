# Sensors — one doc per sensor

Everything about a sensor lives in its own doc: wiring, bench verification,
calibration (procedure + recorded values) and the assumptions its numbers
rest on. Shared material — ground rules, the combined pin map, the
first-power-on checklist — stays in [../wiring.md](../wiring.md).

| Sensor | Measures | Interface / pin | Status | Doc |
|---|---|---|---|---|
| CQRobot CQRSENYW003 probe | water level (top-up ladder) | frequency / D3 | ✅ wired + calibrated 2026-08-25 | [level-probe.md](level-probe.md) |
| Grove 10 cm level strip | water level (coarse range, optional) | I²C 0x77+0x78 | mount test pending | [level-strip.md](level-strip.md) |
| DS18B20 | water temperature | 1-Wire / D2 (via old pH module's T2) | ✅ wired + ice-bath verified 2026-08-25 | [water-temp.md](water-temp.md) |
| Grove TDS | EC / nutrients | analog / A0 | to wire; cal blocked on 1413 µS/cm fluid | [ec-tds.md](ec-tds.md) |
| DFRobot SEN0169-V2 (+ DFR0504) | pH (continuous) | analog / A1 | to wire; cal blocked on buffers | [ph.md](ph.md) |
| BME280 (STEMMA QT) | air temp / RH / pressure | I²C 0x76 | to wire — ⚠ strap SDO first | [air-bme280.md](air-bme280.md) |
| BH1750 (STEMMA QT) | light | I²C 0x23 | to wire | [light-bh1750.md](light-bh1750.md) |
| Fibaro Wall Plug Type E | pump power | Z-Wave → Home Assistant | to buy | [pump-power.md](pump-power.md) |
