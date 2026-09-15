#!/usr/bin/env bash
# release-lock.sh — atomic, cross-session lock for the pomona-0001 release flow.
#
# WHY: the firmware -> OTA -> Annona -> gitops release chain is long and stateful.
# Two Claude sessions (or two people) running it at once corrupts it — that is
# exactly what happened on 2026-09-15. This lock serialises the whole chain.
#
# HOW: the lock is a git ref `refs/locks/pomona-0001-release` on origin. Acquire
# pushes a PARENTLESS commit to create that ref; a second acquire pushes another
# parentless commit, which is a non-fast-forward update of an existing ref and
# the server rejects it atomically. So "create if absent" is enforced by the
# git server, across sessions and machines — no working-tree file, no TOCTOU.
# The ref's commit carries LOCK.json (holder, purpose, time, ttl) so `status`
# can show who holds it and whether it has gone stale.
#
# USAGE:
#   release-lock.sh acquire "<holder>" "<purpose>" [ttl_minutes]   # default ttl 120
#   release-lock.sh status
#   release-lock.sh release                                        # only the holder should
#   release-lock.sh steal   "<holder>" "<purpose>" [ttl_minutes]   # ONLY after status = STALE
#
# Exit codes: 0 ok / 2 held-by-other (live) / 3 not-held / 1 usage or git error.
set -euo pipefail

REF="refs/locks/pomona-0001-release"
REMOTE="${LOCK_REMOTE:-origin}"

die() { echo "release-lock: $*" >&2; exit 1; }
now_epoch() { date -u +%s; }
iso() { date -u +%FT%TZ; }

# Read the current lock's LOCK.json (empty string if the ref does not exist).
_read_lock() {
  local sha
  sha="$(git ls-remote "$REMOTE" "$REF" 2>/dev/null | awk '{print $1}')"
  [ -n "$sha" ] || return 0
  git fetch -q "$REMOTE" "$REF" 2>/dev/null || true
  git cat-file -p "${sha}:LOCK.json" 2>/dev/null || true
}

# Push a fresh parentless lock commit to $REF. Fails (non-zero) if the ref
# already exists, because the update is non-fast-forward. That IS the lock.
_push_lock() {
  local holder="$1" purpose="$2" ttl="$3" json blob tree commit
  json=$(printf '{\n  "holder": "%s",\n  "purpose": "%s",\n  "acquired_at": "%s",\n  "acquired_epoch": %s,\n  "ttl_minutes": %s,\n  "expires_epoch": %s\n}\n' \
    "$holder" "$purpose" "$(iso)" "$(now_epoch)" "$ttl" "$(( $(now_epoch) + ttl * 60 ))")
  blob=$(printf '%s' "$json" | git hash-object -w --stdin)
  tree=$(printf '100644 blob %s\tLOCK.json\n' "$blob" | git mktree)
  commit=$(git commit-tree "$tree" -m "lock: pomona-0001 release — $holder")
  git push -q "$REMOTE" "$commit:$REF"
}

cmd="${1:-status}"
case "$cmd" in
  status)
    body="$(_read_lock)"
    if [ -z "$body" ]; then echo "FREE — no lock held"; exit 3; fi
    exp=$(printf '%s' "$body" | grep -o '"expires_epoch": [0-9]*' | awk '{print $2}')
    if [ -n "$exp" ] && [ "$(now_epoch)" -gt "$exp" ]; then
      echo "STALE — lock expired; safe to steal:"; else echo "HELD:"; fi
    printf '%s\n' "$body"
    ;;
  acquire)
    holder="${2:?holder required, e.g. session id or name}"; purpose="${3:?purpose required}"; ttl="${4:-120}"
    if _push_lock "$holder" "$purpose" "$ttl" 2>/dev/null; then echo "ACQUIRED by $holder ($purpose)"; exit 0; fi
    # Rejected — someone holds it. Auto-steal only if the existing lock has expired.
    body="$(_read_lock)"
    exp=$(printf '%s' "$body" | grep -o '"expires_epoch": [0-9]*' | awk '{print $2}')
    if [ -n "$exp" ] && [ "$(now_epoch)" -gt "$exp" ]; then
      echo "existing lock is STALE — stealing" >&2
      git push -q "$REMOTE" ":$REF" 2>/dev/null || true
      _push_lock "$holder" "$purpose" "$ttl" && { echo "ACQUIRED (stole stale lock) by $holder"; exit 0; }
    fi
    echo "REFUSED — held by another session:" >&2; printf '%s\n' "$body" >&2; exit 2
    ;;
  release)
    body="$(_read_lock)"; [ -n "$body" ] || { echo "not held — nothing to release"; exit 3; }
    git push -q "$REMOTE" ":$REF" && echo "RELEASED"
    ;;
  steal)
    holder="${2:?holder required}"; purpose="${3:?purpose required}"; ttl="${4:-120}"
    git push -q "$REMOTE" ":$REF" 2>/dev/null || true
    _push_lock "$holder" "$purpose" "$ttl" && echo "STOLEN by $holder ($purpose)"
    ;;
  *) die "unknown command '$cmd' — use acquire|status|release|steal" ;;
esac
