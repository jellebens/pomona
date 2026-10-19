# ATECC608A provisioning & slot policy (#244)

Design note for Layer 2 of [ota-and-secrets.md](ota-and-secrets.md): move
`WIFI_PASS`, the WiFi SSID and `MQTT_PASS` off the gitignored `secrets.h` and
into the GIGA's onboard **ATECC608A** secure element, so the compiled sketch and
every OTA image contain **no secrets at all**. Provisioning sketch:
[`../firmware/provision/provision.ino`](../firmware/provision/provision.ino).

**Hard requirement (owner, 2026-09-15): the credential slots must stay
overwritable in the field** — a WiFi change (new SSID and/or PSK) or an MQTT
password rotation must be a USB re-provision, never a chip swap or a rebuild.

## The one irreversible decision

The ATECC608A has a **Config zone** (slot policies) and a **Data zone** (the 16
slots). Provisioning **locks both, permanently**. After the lock:

- slot *policies* can never change — what is writable/readable later is fixed now;
- a slot whose `SlotConfig.WriteConfig = Always` **stays clear-writable forever**,
  even though the zone is locked. That bit is what satisfies the overwrite
  requirement.

The chip is soldered to the GIGA, so a wrong lock **bricks the secure element on
that board**. Therefore: **validate the config on a spare GIGA first, and the
provisioning sketch never locks without an explicit typed confirmation.**

## The unavoidable trade-off (be honest about it)

The firmware must read the WiFi PSK and MQTT password back in the clear at boot to
hand them to `WiFi.begin()` and `mqtt.setUsernamePassword()`. *The firmware reading
the value IS an I²C read.* So a stored plaintext credential **cannot** be both
firmware-readable and I²C-unreadable. Consequences:

- Layer 2 credential slots are `IsSecret = 0` (clear-readable) by necessity. Their
  security benefit is **"not in the repo, the binary, or the OTA image"** — exactly
  the leak vectors the threat model (home LAN, physical access = owner) cares
  about. It is **not** on-device unreadability; anyone with the board and an I²C
  probe can read them, same trust boundary as holding the tower in your hand.
- The only way to on-device unreadability is **Layer 3** (below): store a private
  key that is *used* by the chip and *never read out*. That is the endgame, and it
  removes passwords entirely.

## Slot map (the STOCK ECCX08 default TLS config — no custom bytes)

We write the library's **unmodified default TLS config** and lock that. This is the
single most important risk decision: the default is a proven, public 128-byte
config, and in it **slots 8–15 are already open clear read/write data slots**
(`SlotConfig = 0x0000`) while slot 0 is a P-256 private-key slot. So we need to
hand-author **zero** config bytes — the brick risk of a wrong custom config is
gone, and we still get exactly the slots we need.

Slot sizes are fixed by the chip: slots 0–7 = 36 B, slot 8 = 416 B, slots 9–15 =
72 B. Each credential is stored as `[1 byte length][bytes]`.

| Slot | Holds | Size | In the default config | Why here |
|---|---|---|---|---|
| 0 | **Reserved: TLS P-256 private key** (Layer 3) | 36 B | private-key slot, GenKey-able | Exists in the default; key slots CANNOT be added after the lock, so Layer 3's key must live here from day one. GenKey-regenerable → Layer 3 "rotation" is re-GenKey + new cert, no password ever. |
| 8 | **Reserved: device certificate** (Layer 3) | 416 B | open clear r/w | Big enough for the signed client cert when Layer 3 lands. |
| 9 | WiFi **PSK** | 72 B | open clear r/w (Always) | ≤63-char WPA2 passphrase + length fits. Overwritable in the field. |
| 10 | **MQTT password** | 72 B | open clear r/w (Always) | Rotation = write this slot over USB + update the broker. |
| 11 | WiFi **SSID** | 72 B | open clear r/w (Always) | A full network change (SSID+PSK) is then chip-only, no reflash. |

`SlotConfig = 0x0000` means clear read + clear write always — writable forever even
after the zone lock (that satisfies the overwrite requirement) and readable so the
firmware can use the value at boot (the trade-off above). MQTT username, host and
port stay non-secret in `config.h`.

**Reserving slots 0 and 8 now is the whole point of thinking ahead:** you cannot
add a private-key slot after the config lock, so if Layer 3 is ever wanted, its
slots must exist from this provisioning. They cost nothing unused, and the default
config already provides them — no reason not to.

## Provisioning flow (the sketch)

One USB sketch, menu over Serial at 115200. Credentials are typed in over Serial,
so they are **never** in the sketch binary.

- `status` — print the chip serial, lock state, and the stored lengths.
- `provision` — **only if the config zone is unlocked.** Writes the custom config,
  prints it back for review, and requires the operator to type `LOCK` to perform
  the irreversible config+data lock. Refuses otherwise.
- `set ssid|psk|mqtt` — write a credential slot. Works **both** for first-time
  fill and for every later overwrite (the WiFi-change / rotation path), because
  those slots are `WriteConfig = Always`.
- `verify` — read the clear slots back and print their lengths (not their values).

First provisioning: `status` → `provision` (→ type `LOCK`) → `set ssid` → `set
psk` → `set mqtt` → `verify`.
Later WiFi change: `set ssid` → `set psk` → `verify`. No lock, no rebuild.
Later MQTT rotation: `set mqtt`, then update the broker (see the rotation
checklist), in the lock-safe order.

## Firmware read-path (separate step, described here for #244)

`src/network/network.cpp` stops using the `WIFI_PASS` / `MQTT_PASS` macros and
instead, at boot: `ECCX08.begin()`, read slot 11 → SSID, slot 9 → PSK, slot 10 →
MQTT password (each `[len][bytes]`). If the chip is absent or unprovisioned, fail
loudly on the boot screen rather than falling back to a compiled default (there is
no default any more). `secrets.h` and its `#include` are deleted;
`secrets.h.example` and the docs lose Layer 1. This is the change that makes OTA
images secret-free.

## Layer 3 (later, optional) — the real fix

Switch MQTT auth to mutual TLS: the ATECC holds the P-256 private key (slot 0,
never leaves the chip), EMQX runs a TLS listener (8883) and authenticates the
device by its client certificate (slot 8), signed by the lab CA. Then there is **no
password to store, rotate, or leak** — today's exposure class becomes impossible.
Rotation = GenKey a new key + reissue the cert + update the broker's trust. The
cost is broker-side (CA, per-device certs, cert-based authn), which is why it is
staged after Layer 2. Its own card.

## Validation checklist before locking the tower's chip

1. Run `provision` on a **spare GIGA**, lock it, and confirm `set`/`verify` and a
   real WiFi+MQTT connect all work from the locked chip.
2. Dump the locked config (`ECCX08.readConfiguration()` in `status`) and confirm it
   equals the stock default and that slots 9/10/11 read back `SlotConfig = 0x0000`
   (clear r/w). Because we write the unmodified default, this is a confirmation, not
   a bespoke-config audit — but still do it before trusting the tower's chip.
3. Only then provision the tower, over USB, and delete `secrets.h` from the build.

## Gotchas

- **The lock is forever and the chip is soldered.** Never lock unvalidated, even
  though we write the stock default — a locked chip is a locked chip.
- The sketch writes the library's **unmodified default TLS config**, so there are
  no bespoke config bytes to get wrong. If a future revision ever DOES customise
  the config, the whole brick-risk calculus changes — validate exhaustively then.
- WPA2 PSKs can be up to 63 chars; slot 9's 72 B covers `[len]` + 63. A 64-char
  raw PSK hex would also fit. Enterprise WiFi (EAP) is out of scope.
- `set` on a locked chip only works because slots 8–15 are `Always` in the default;
  never move a credential onto slot 0–7 (several are key/secret slots) or it becomes
  unwritable or unreadable after the lock.
