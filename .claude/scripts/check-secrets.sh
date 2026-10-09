#!/usr/bin/env bash
# check-secrets.sh — the credential gate of the pomona-0001 release flow (Phase 3, step 0).
#
# WHY: the firmware carries its WiFi and broker passwords from secrets.h, a gitignored file per
# checkout, and nothing ties that file to what the broker (or the access point) holds. On
# 2026-10-09 firmware 2.3.2 was built from a stale secrets.h: the node flashed fine, then the
# broker refused unit-pomona-0001 (bad_username_or_password). The OTA trigger only arrives over
# MQTT, so no corrected image could reach it — only a USB reflash or a broker-side password
# change could. Recovery took a power cycle and an owner-approved credential change.
#
# HOW: a build may only use the passwords the node last connected with. Those are embedded,
# verbatim, in the binary of the image the node runs now (~/ota-tools/build-<running>/pomona.ino.bin).
# This script checks that WIFI_PASS and MQTT_PASS from secrets.h both occur in that binary.
# It NEVER prints a password — only found / NOT found and the length.
#
# USAGE:
#   check-secrets.sh <reference.bin> [secrets.h]
#     <reference.bin>  the .bin of the image the node runs NOW (sys/meta fw_version)
#     [secrets.h]      default: firmware/pomona/secrets.h of this checkout
# EXIT: 0 = both passwords match the running image; 1 = a mismatch or a placeholder — STOP;
#       2 = cannot check (missing file / unparseable) — STOP.
set -u
ref=${1:-}
root=$(git rev-parse --show-toplevel 2>/dev/null || pwd)
sec=${2:-$root/firmware/pomona/secrets.h}
[ -n "$ref" ] || { echo "usage: check-secrets.sh <reference.bin> [secrets.h]" >&2; exit 2; }
[ -f "$ref" ] || { echo "no reference binary at $ref — build dirs of flashed images must be kept (~/ota-tools/build-<ver>)" >&2; exit 2; }
[ -f "$sec" ] || { echo "no secrets.h at $sec" >&2; exit 2; }
python3 - "$ref" "$sec" <<'PY'
import re, sys
ref, sec = sys.argv[1], sys.argv[2]
text = open(sec, encoding="utf-8").read()
blob = open(ref, "rb").read()
rc = 0
for key in ("WIFI_PASS", "MQTT_PASS"):
    m = re.search(r'#define\s+' + key + r'\s+"([^"]*)"', text) or re.search(key + r'\s*(?:\[\])?\s*=\s*"([^"]*)"', text)
    if not m:
        print(f"{key}: not found in {sec}"); sys.exit(2)
    pw = m.group(1).encode()
    if not pw or pw == b"changeme":
        print(f"{key}: placeholder in {sec} — fill in the real value"); rc = 1; continue
    found = pw in blob
    print(f"{key} ({len(pw)} chars): {'matches' if found else 'NOT in'} the running image {ref}")
    if not found:
        rc = 1
if rc:
    print("STOP: this secrets.h is not what the node connects with. Ask the owner for the current one — never "
          "lift a password out of a binary or shell history, never change the broker's password on your own.")
sys.exit(rc)
PY
