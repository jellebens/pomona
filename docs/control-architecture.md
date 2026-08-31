# Control architecture — where the pump and light logic should live

Status: **design note, 2026-08-31.** Nothing here is built yet. Written after
the tower was planted and the first scheduling went live in Home Assistant.

## The question

The pump and light schedules currently run in HA. Should they move into the
GIGA firmware, driving relays directly?

**The owner's argument, and it is the strong one: the Arduino would then
contain all the logic.**

## Where the logic lives today, which is the problem

| Concern | Lives in |
|---|---|
| Sensing (level, EC, pH, temp, lux) | GIGA firmware |
| Pump + light schedule | HA package `pomona_schedule.yaml` |
| Level interlock and settle check | HA package `pomona_telemetry.yaml` |
| Actuation (mains switching) | Smart plugs, driven by HA |
| Alerting | HA |

Three systems and a broker sit between the sensor and the thing it should be
protecting. That split is the direct cause of the awkwardness in the settle
check: **the device holding the level reading cannot stop the pump**, so a
low-water response has to travel GIGA → MQTT → broker → HA → Z-Wave → plug,
and the "wait 5 minutes and re-read" dance exists partly because the round
trip makes anything faster untrustworthy.

It also means **an HA outage stops irrigation entirely**. For a living crop
that is a real single point of failure, and it is not one the plants can wait
out.

## Option A — leave it in HA

What exists now.

- ✅ Schedule changes are a YAML edit, no reflash.
- ✅ Rich alerting, history, dashboards, manual override for free.
- ✅ Mains switching stays inside certified smart plugs.
- ❌ Irrigation depends on WiFi + broker + HA + Z-Wave all being up.
- ❌ The interlock is slow and indirect, for the reasons above.
- ❌ Logic is smeared across three places.

## Option B — all logic in the GIGA

The owner's preference.

- ✅ **One place to reason about.** Debugging a watering fault means reading
  one firmware, not three systems.
- ✅ **No network dependency.** The tower keeps watering itself through a WiFi
  outage, a broker restart, or an HA upgrade.
- ✅ **The sensor and the actuator finally sit on the same device.** The
  settle check becomes a few lines of local code with an instant re-read
  rather than a five-minute distributed dance.
- ✅ Matches the project's own stated principle — the unit is *self-contained
  at the tower*.
- ❌ **Reboots.** This is the serious one, see below.
- ❌ Schedule changes mean a reflash, unless made configurable over MQTT and
  persisted to flash.
- ❌ Mains switching moves out of a certified plug and into a home-built relay.
- ❌ Loses HA history and alerting unless the unit keeps publishing what it
  decided.

## Option C — GIGA owns control, HA observes and overrides *(recommended)*

This gives the owner what they actually asked for — all the *control* logic in
one place — without throwing away observability:

- **The GIGA owns:** the schedule, the level interlock, the settle logic, and
  a safe state on boot. It decides, always, and it decides alone.
- **The GIGA publishes what it decided:** `pomona/pump/state`,
  `pomona/pump/reason` (`schedule` / `level_low` / `override` / `boot_safe`).
  HA and Grafana get full visibility without holding any authority.
- **The GIGA subscribes to an override:** `pomona/pump/override` = `auto` /
  `on` / `off`, non-retained, defaulting to `auto`. HA and the owner can force
  it — but **firmware keeps the safety veto**: an override can never make the
  pump run when the local interlock says no.
- **HA keeps** alerting, history, dashboards and the human-facing controls.

The principle: *whoever holds the sensor holds the cutoff; whoever holds the
calendar can only make requests.*

## ⚠ The reboot problem, which decides everything

The GIGA reboots for OTA, and this project has already seen the failure modes:
a wedged I²C bus boot-looping the unit, IWDG watchdog resets during OTA
decompress, and a corrupted OTA partition. Those are documented history, not
hypotheticals.

With control in firmware, a reboot loop means either a pump stuck off — a dead
crop in about two days — or a pump stuck on, which is a flooded floor.

Mitigations, all required before this ships:

1. **Relay defaults to OFF when unpowered.** Choose the module for this; do not
   rely on firmware to un-stick it.
2. **Set the safe state in `setup()` before anything else** — before WiFi,
   before I²C, before the display. The very first instruction after boot
   should put the relays in a known state.
3. **Never gate watering on WiFi.** The schedule must run from the local clock
   with no network in the loop. (An RTC or NTP-at-boot with a sane fallback —
   a GIGA that cannot reach NTP must still water.)
4. **Keep the smart plug upstream as a hard kill.** Cheap, independent of the
   firmware, and lets a human cut power without unplugging anything.
5. **Watchdog stays.** The existing 30 s IWDG is what turns a hang into a
   reboot rather than a permanently stuck relay.

## ⚠ Mains switching

The pump and the lamps are mains. A bare relay board next to a water tank is
the one part of this that can hurt someone.

Options in order of preference:

1. **Keep mains in the certified smart plugs**, and have the GIGA command them.
   — but this reintroduces the network dependency the whole exercise was meant
   to remove, so it defeats the point.
2. **A properly enclosed, opto-isolated relay module** in a closed box, mains
   terminals shrouded, cable glands, sited away from splash. Note the GIGA is
   3.3 V logic — the module must accept a 3.3 V trigger or get a level shifter.
3. ~~Bare relay board on the bench next to the tank~~ — no.

## Can HA listen to events and act on them?

Yes, and it needs no new plumbing.

- **MQTT trigger:** an HA automation with `platform: mqtt, topic: pomona/...`
  fires on *any* message the GIGA publishes. This is the natural path for
  GIGA-raised events — a level alarm, an OTA result, a sensor fault — and it
  reaches HA in about a second at QoS 1.
- **State triggers:** already in use, on the MQTT sensors in
  `pomona_telemetry.yaml`.
- **Webhooks** (`platform: webhook`, GIGA does an HTTP POST) work too, but are
  worse here: they need a URL and token baked into firmware, while the MQTT
  connection already exists and already has credentials.

And in the other direction, HA publishes to `pomona/pump/override` to ask for
something — which is exactly the Option C split.

## Suggested order of work

1. **Relay hardware first**, with the safe-state-when-unpowered behaviour
   verified on the bench before any firmware touches it.
2. **Local schedule in firmware**, still with the smart plug upstream. Run both
   for a week and compare against the HA schedule.
3. **Local interlock + settle logic**, now trivial because the level reading is
   in the same process.
4. **Publish decisions** (`pump/state`, `pump/reason`) and retire the HA
   scheduling package, keeping HA alerting and dashboards.
5. **Override topic** last — it is a convenience, not a requirement.

## Related

- [design.md](design.md) — the original "control is deferred" decision
- [sensors/level-probe.md](sensors/level-probe.md) — why the raw level ladder
  cannot be trusted mid-cycle
- `home-assitant` repo: `pomona-schedule.md`, `packages/pomona_schedule.yaml`,
  `packages/pomona_telemetry.yaml`
- Trello #224 (phase 2 — peristaltic dosing) is the natural sibling: if control
  moves into the GIGA, dosing pumps should land there too rather than adding a
  second control plane.
