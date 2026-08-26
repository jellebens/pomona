#!/usr/bin/env bash
# Pomona firmware deploy — bump version, build, flash (Trello #229/#243).
# Linux/WSL twin of deploy.ps1 — keep the two scripts BEHAVIORALLY IN SYNC.
#
#   ./deploy.sh                       # bump patch, build+flash bringup
#   ./deploy.sh -s pomona             # other sketch
#   ./deploy.sh -b minor              # 0.1.x -> 0.2.0 (features)
#   ./deploy.sh -b major              # x.y.z -> (x+1).0.0 (breaking)
#   ./deploy.sh -b none               # rebuild/reflash without bumping
#   ./deploy.sh -p /dev/ttyACM1       # other port (default /dev/ttyACM0,
#                                     #   or set POMONA_PORT)
#
# The version lives in libraries/PomonaVersion/src/PomonaVersion.h and is
# the single source of truth — every sketch prints it at boot. Deploy ONLY
# through this script (or deploy.ps1 on Windows) so the number stays
# truthful. Commit the bumped file with the change it ships.
#
# WSL: the board's USB must be attached to WSL first (Windows side:
#   usbipd list && usbipd attach --wsl --busid <id>). Close any serial
# monitor first — it holds the port.

set -euo pipefail

SKETCH="bringup"
PORT="${POMONA_PORT:-/dev/ttyACM0}"
BUMP="patch"

usage() { grep '^#   ' "$0" | sed 's/^#   //'; exit 1; }

while getopts "s:p:b:h" opt; do
  case "$opt" in
    s) SKETCH="$OPTARG" ;;
    p) PORT="$OPTARG" ;;
    b) BUMP="$OPTARG" ;;
    *) usage ;;
  esac
done

case "$BUMP" in patch|minor|major|none) ;; *)
  echo "error: -b must be patch|minor|major|none" >&2; exit 1 ;;
esac

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "error: arduino-cli not found on PATH." >&2
  echo "Install the Linux CLI (the repo's bundled .exe is Windows-only):" >&2
  echo "  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh" >&2
  echo "then: arduino-cli core install arduino:mbed_giga  (+ libs, see README.md)" >&2
  exit 1
fi

FW="$(cd "$(dirname "$0")" && pwd)"
FQBN="arduino:mbed_giga:giga"
VERSION_FILE="$FW/libraries/PomonaVersion/src/PomonaVersion.h"

# --- bump ---------------------------------------------------------------
CURRENT=$(sed -n 's/#define POMONA_FW_VERSION "\([0-9]\+\.[0-9]\+\.[0-9]\+\)"/\1/p' "$VERSION_FILE")
if [ -z "$CURRENT" ]; then
  echo "error: no semver POMONA_FW_VERSION found in $VERSION_FILE" >&2
  exit 1
fi
IFS=. read -r MAJOR MINOR PATCH <<EOF
$CURRENT
EOF

case "$BUMP" in
  patch) PATCH=$((PATCH + 1)) ;;
  minor) MINOR=$((MINOR + 1)); PATCH=0 ;;
  major) MAJOR=$((MAJOR + 1)); MINOR=0; PATCH=0 ;;
  none) ;;
esac
NEW="$MAJOR.$MINOR.$PATCH"

if [ "$BUMP" != "none" ]; then
  sed -i "s/#define POMONA_FW_VERSION \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/#define POMONA_FW_VERSION \"$NEW\"/" "$VERSION_FILE"
  echo "version: $CURRENT -> $NEW"
else
  echo "version: $NEW (no bump)"
fi

# --- build + flash ------------------------------------------------------
if ! arduino-cli compile --fqbn "$FQBN" --libraries "$FW/libraries" "$FW/$SKETCH"; then
  echo "compile failed — version file was already bumped; fix and rerun with -b none" >&2
  exit 1
fi

if ! arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$FW/$SKETCH"; then
  echo "upload failed (serial monitor open? board attached via usbipd? on $PORT?) — rerun with -b none" >&2
  exit 1
fi

echo "deployed $SKETCH v$NEW to $PORT"
