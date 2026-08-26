# Pump power — Fibaro Wall Plug Type E

**Status: to buy** (BE Type E version, with power metering).

Nothing wires to the GIGA: the plug joins Home Assistant via **zwave_js**
(HA already runs it), sitting between the pump's dumb power pack and the
wall socket.

## Role

- Pump-alive monitoring: pump dead (or dry-running — load drops) = watts
  change = alert. This is the second line of defence behind the
  [level probe](level-probe.md)'s refill ladder (see its assumption A3 —
  the probe is blind below 8.2 L).
- The plug can also SWITCH, so HA could cycle the pump on a schedule —
  owner to decide whether that's wanted or counts as
  automation-later (#224 territory).

## Integration

HA entity → the same MQTT/Influx path as the rest once #222 defines the
topics; until then it's visible in HA directly.
