#!/bin/bash
# ci.sh — TT030 side-repo CI (tt030-transputer-job-queue FR-9, operator MCQ 2026-10-13). Runs on the remote system management host, no TT:
#   1. host test suites: scripts/test_*.py under pytest (excludes the two that need the TT or Hatari:
#      test_e2e_ping_rpm.py, test_hatari_ping_rpm.sh)
#   2. the job-queue emulated lane: sam-job-tt/emu/run_emu_tests.sh (libttjob + host-vs-t4 jobtest)
#   3. the resident transputer image builds (sam-job-tt/build_t4.sh)
#   4. the TT RPMs build: ttjob (sam-job-tt/build_rpm.sh), t425-compress, ttmon, dropbear, toswin2, ttkbdd (scripts/build_*_rpm.sh)
#   5. publish gate (FR-9): only when 1-4 all passed, each RPM whose name (NVR) the LAN mirror lacks is added with
#      tt_rpm.py add (signed index); an NVR already there with different bytes fails the run (bump the version). A red run publishes nothing; a publish failure turns the run red.
#   6. report what the TT runs vs the mirror (scripts/tt_versions.py; not part of the outcome)
# Exit 0 only if all pass. The result is logged centrally (sam_logger event tt030.ci.run) and written to
# ~/.local/state/tt030-ci/last.txt, so SAM can read it without asking. Run by sam-tt030-ci.service (iac/fractal).
set -uo pipefail
{   # the whole script is one block, so bash parses it before running: editing ci.sh during a run garbled that run (2026-10-13)
cd "$(dirname "$0")/.."
st=~/.local/state/tt030-ci; mkdir -p "$st/runs"; log=$st/last.log
# Build what the gate landed (operator 2026-10-18: the side repo lands through SAM's tenant lane, land/<name> ->
# origin/main), never the local checkout's HEAD. CI_REF=<ref> builds something else as a preview: it never publishes.
# One run at a time: two overlapping runs share the t4 lane and the build dirs (2026-10-19 01:35Z: a red emu step).
exec 9>"$st/lock"; flock -n 9 || { echo "ci: another run holds $st/lock; this one skipped"; exit 0; }
git fetch -q origin main || { echo "ci: git fetch origin main failed" >&2; exit 1; }
# sam-tt030-ci.service runs THIS checkout's ci.sh: fast-forward it so the next run uses the landed script (bash has
# parsed this whole block already). A checkout that cannot fast-forward is said, and this run still builds origin/main.
[ -z "${CI_REF:-}" ] && [ "$(git symbolic-ref -q --short HEAD)" = main ] && { git merge -q --ff-only origin/main 2>/dev/null || echo "ci: checkout $(git rev-parse --short HEAD) cannot fast-forward to origin/main: the service keeps running this ci.sh" >&2; }
ref=${CI_REF:-origin/main}
rev=$(git rev-parse --short "$ref^{commit}") || { echo "ci: no such ref $ref" >&2; exit 1; }
# The timer polls every 5 min: a rev already built is skipped unless the last run is 20 h old (the daily drift run)
# or CI_FORCE=1. Said out loud, so a skip never reads like a run.
if [ -z "${CI_REF:-}${CI_FORCE:-}" ] && grep -q " rev=$rev " "$st/last.txt" 2>/dev/null && [ -n "$(find "$st/last.txt" -mmin -1200)" ]; then
    echo "ci: origin/main $rev already built ($(cut -d' ' -f3 "$st/last.txt")); nothing to do"; exit 0; fi
: > "$log"
# Build the COMMITTED tree, never the working tree agents edit in (2026-10-18 audit): a detached worktree of $ref,
# with the gitignored toolchains in tools/ linked in. Uncommitted edits can no longer be built or published, and a
# build that rewrites a tracked file (build_ttkbdd_rpm.sh refreshes the card overlay) fails the run below instead of
# leaving the agents' tree dirty.
src=$(pwd); wt=$(mktemp -d /tmp/tt030-ci-wt.XXXXXX)
cleanup() { cd "$src"; git worktree remove --force "$wt" >/dev/null 2>&1; rm -rf "$wt"; git worktree prune; }
trap cleanup EXIT
git worktree add -q --detach "$wt" "$ref" && ln -s "$(cd "$(git rev-parse --git-common-dir)/.." && pwd)/tools" "$wt/tools" && cd "$wt" || { echo "ci: no clean worktree" >&2; exit 1; }
step() { local name=$1; shift; echo "== $name" | tee -a "$log"; if "$@" >> "$log" 2>&1; then echo "   ok" | tee -a "$log"; else echo "   FAIL" | tee -a "$log"; fails="$fails $name"; fi; }
fails=
step pytest /opt/sam/venv/bin/python3 -m pytest -q --ignore=scripts/test_e2e_ping_rpm.py scripts/
step emu    env JOBS="${JOBS:-1000}" sam-job-tt/emu/run_emu_tests.sh
step image  sam-job-tt/build_t4.sh
rpms=
build_rpms() { local r; r=$(sam-job-tt/build_rpm.sh | tail -1) && rpms="$r" && r=$(scripts/build_t425_compress.sh | tail -1) && rpms="$rpms $r" && r=$(scripts/build_ttmon_rpm.sh | tail -1) && rpms="$rpms $r" && r=$(scripts/build_dropbear_rpm.sh | tail -1) && rpms="$rpms $r" && r=$(scripts/build_toswin2_rpm.sh | tail -1) && rpms="$rpms $r" && r=$(scripts/build_ttkbdd_rpm.sh | tail -1) && rpms="$rpms $r"; }
step rpm    build_rpms
MIRROR=${TT_MIRROR:-/mnt/vault/tt030}
published=
publish() {
    local r new= bad=
    for r in $rpms; do                                                  # pass 1: decide everything, publish nothing
        if [ -e "$MIRROR/built/$(basename "$r")" ]; then             # this NVR is already on the mirror:
            cmp -s "$r" "$MIRROR/built/$(basename "$r")" && continue    # same bytes (builds are reproducible), or
            echo "publish: $(basename "$r") on the mirror has different content: bump the version" >&2; bad=1; continue
        fi
        new="$new $r"
    done
    [ -n "$bad" ] && { echo "publish: nothing published (all or none; 2026-10-18 audit: red runs used to publish the RPMs before the first mismatch)" >&2; return 1; }
    for r in $new; do                                                   # pass 2
        /opt/sam/venv/bin/python3 scripts/tt_rpm.py add "$MIRROR" "$r" || return 1
        published="$published $(basename "$r")"
    done
}
clean() { local d; d=$(git status --porcelain --untracked-files=no); [ -z "$d" ] && return 0
    echo "a build changed tracked files (commit the refreshed copies):"; echo "$d"; git diff --stat; return 1; }
step clean  clean
if [ -n "${CI_REF:-}" ]; then echo "== publish: skipped (CI_REF preview, not origin/main)" | tee -a "$log"
elif [ -z "$fails" ]; then step publish publish; else echo "== publish: skipped (red run)" | tee -a "$log"; fi
outcome=$([ -z "$fails" ] && echo pass || echo fail)
# what the TT runs vs the mirror (tt_versions.py): reported, never part of the outcome (a TT that has not run
# yum update yet is not a code failure)
tt=$(/opt/sam/venv/bin/python3 scripts/tt_versions.py "$MIRROR" 2>&1 | tee -a "$log" | tail -1); tt=${tt#tt_versions: }
echo "$(date -u +%FT%TZ) rev=$rev outcome=$outcome failed=${fails:-none} published=${published:-none} tt=[$tt]" | sed "s/published= /published=/" | tee "$st/last.txt"
PYTHONPATH=/opt/sam/runtime/src /opt/sam/venv/bin/python3 -c "
from sam.utils.sam_logger import emit
emit('tt030.ci.run', outcome='$outcome', rev='$rev', failed='${fails:-none}', published='${published:-none}', tt='$tt')" >/dev/null 2>&1 || echo "ci: central log emit failed (result still in $st)" >&2
cp "$log" "$st/runs/$(date -u +%Y%m%dT%H%M%SZ)-$rev-$outcome.log"; ls -1t "$st/runs" | tail -n +51 | sed "s|^|$st/runs/|" | xargs -r rm -f   # keep 50
[ -z "$fails" ]
exit
}
