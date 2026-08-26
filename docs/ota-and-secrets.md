# Firmware security: secret storage + OTA updates

Design decided 2026-08-25 (Trello #243 OTA, #244 secrets). Goal: the GIGA
unit runs at the tower without USB access — firmware ships over WiFi — and
its credentials (WiFi PSK, MQTT) never appear in git, CI artifacts, or OTA
images.

## What the hardware gives us

The GIGA R1 WiFi has an onboard **ATECC608A secure element** (I²C, Arduino
[ArduinoECCX08](https://github.com/arduino-libraries/ArduinoECCX08)
library): up to 5 ECC private-key slots (keys are generated inside the
chip and can never be read out) plus small general data slots (~36–72
bytes each) that can be locked against plain reads. ⚠ The chip's one-time
**configuration lock is permanent** — configure deliberately, once.

Refs: [store page](https://store.arduino.cc/products/giga-r1-wifi) ·
[ECCX08 usage notes](https://fjebaker.github.io/notes/arduino/crypto-auth.html) ·
[Arduino forum: ATECC608A on the GIGA](https://forum.arduino.cc/t/any-sample-for-atecc608a/1203480).

## Secrets — layered design

Threat model: home LAN; physical access to the unit = the owner. The real
risks are secrets leaking through the repo, CI, or OTA artifacts.

1. **Layer 1 — now (with #229):** secrets live in a gitignored
   `firmware/*/secrets.h`; a committed `secrets.h.example` documents the
   shape. Protects the repo, not the device.
2. **Layer 2 — target (#244):** a one-time **provisioning sketch** writes
   WiFi SSID/PSK and MQTT credentials into ATECC608A data slots; the real
   firmware reads them at boot. The sketch binary then contains **no
   secrets at all**, which makes OTA images clean — they can be built in
   CI, stored, and served without leaking anything. Re-provisioning = run
   the provisioning sketch again over USB.
3. **Layer 3 — later, optional:** TLS client auth to mosquitto using an
   ECC key held in the ATECC608A (the key never leaves the chip; the
   broker gets a client cert). Replaces MQTT passwords entirely.

## OTA — two complementary flavors

Prereqs for both (one-time, over USB, documented in the bring-up):
update the mbed bootloader (`STM32H747_System → STM32H747_updateBootloader`)
and partition the QSPI flash (`STM32H747_System → QSPIFormat` — erases the
QSPI; do this before anything else stores data there).

1. **Dev push (bench):** Arduino IDE / arduino-cli **network-port OTA
   upload** — flash from the workstation over WiFi, no USB. Setup per
   [Arduino's GIGA OTA support article](https://support.arduino.cc/hc/en-us/articles/12370721200540-Configure-GIGA-R1-WiFi-Portenta-H7-and-Portenta-Machine-Control-for-Over-The-Air-OTA-uploads).
2. **Production pull (tower):** the
   [Arduino_Portenta_OTA](https://github.com/arduino-libraries/Arduino_Portenta_OTA)
   library (works on the GIGA despite the name): firmware periodically
   checks a version endpoint — comparing its own `POMONA_FW_VERSION`
   (semver, `firmware/libraries/PomonaVersion`, auto-bumped by
   `firmware/deploy.ps1` on every deploy) against the served version —
   downloads the `.ota` image over HTTP(S) into QSPI, verifies, and the
   bootloader applies it on reboot. The image is
   served from the cluster (a tiny HTTP server in `landingzones/pomona`,
   #221/#222 territory) — which puts firmware rollout on the same
   GitOps rails as everything else: merge → CI builds `.ota` → cluster
   serves → unit picks it up.

Failure safety: the update is staged in QSPI and applied by the
bootloader, so a failed download doesn't brick the unit; a bad-but-valid
image does boot, which is why **a full OTA cycle must be proven on the
bench before the unit is mounted** (#243 exit criterion). Keep the USB
path documented as the recovery of last resort.

## Order of work

1. #229 firmware v1 brings up WiFi/MQTT with Layer-1 secrets
   (`secrets.h`, gitignored — plumbing already in `firmware/`).
2. #243: bootloader + QSPI prep, dev push OTA working on the bench, then
   pull OTA from the cluster; full cycle proven before mounting.
3. #244: ATECC608A provisioning sketch, firmware reads secrets from the
   secure element, secrets deleted from `arduino_secrets.h`.
4. Later: TLS client auth via ATECC (Layer 3) when the MQTT broker side
   is ready for it.
