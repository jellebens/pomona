#!/bin/bash
# pomona: firmware 2.1.0 (topic root ceres/, ADR-0012) — commit on card-297, PR develop, release master, tag v2.1.0.
set -e
cd ~/repos/pomona
rm -f .rename297.sh
git add -A .
git status --short | grep -v "^[AMRD] " || true
git commit -q -F - <<'MSG'
feat(firmware): #297 the topic root is ceres/ (ADR-0012: Demeter became Ceres) — 2.1.0

Every v2 topic is ceres/pomona-0001/…; the controller is Vertumnus
(vertumnus-pomona-0001), the config service Annona. 2.0.0 was never flashed.
Compiles clean (arduino-cli 1.5.1, mbed_giga 4.6.0; 750,700 B flash).

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
MSG
git push -q -u origin card-297 2>&1 | grep -v "^remote:" || true
cat > .pr.md <<'PR'
## Firmware 2.1.0 (#297): the topic root is `ceres/` (ADR-0012 — Demeter became Ceres)

Every v2 topic is `ceres/pomona-0001/…` (was `demeter/…` in 2.0.0, which was never flashed); the controller is Vertumnus (`vertumnus-pomona-0001`), the config service Annona. `docs/mqtt.md`, the control-architecture doc and the sketch README follow. Compiles clean. Not flashed — the owner's maintenance window with slice 3.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
PR
url=$(gh pr create --base develop --head card-297 --title "feat(firmware): #297 the topic root is ceres/ (ADR-0012) — 2.1.0" --body-file .pr.md); rm -f .pr.md; echo "$url"
num=${url##*/}
gh pr merge "$num" --merge --subject "Merge pull request #$num from jellebens/card-297 — firmware 2.1.0: the ceres/ root" 2>&1 | tail -1
git fetch -q origin
cat > .pr.md <<'PR'
## Release: firmware 2.1.0 — the ceres/ topic root (#297)

CHANGELOG [2.1.0]; tag `v2.1.0` after merge. Not flashed.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
PR
url=$(gh pr create --base master --head develop --title "release: firmware 2.1.0 — the ceres/ topic root (#297)" --body-file .pr.md); rm -f .pr.md; echo "$url"
num=${url##*/}
gh pr merge "$num" --merge --subject "Merge pull request #$num from jellebens/develop — release firmware 2.1.0" 2>&1 | tail -1
sha=$(gh pr view "$num" --json mergeCommit --jq .mergeCommit.oid)
git fetch -q origin master
git tag -a v2.1.0 "$sha" -m "pomona firmware 2.1.0 — the ceres/ topic root (ADR-0012)"
git push -q origin v2.1.0 2>&1 | grep -v "^remote:" || true
git checkout -q develop && git reset -q --hard origin/develop && git merge -q --no-edit origin/master && git push -q origin develop 2>&1 | grep -v "^remote:" || true
git branch -D card-297 >/dev/null 2>&1 || true; git push -q origin --delete card-297 2>&1 | grep -v "^remote:" || true
echo "release=$sha"
rm -f .rel297b.sh
