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
st=~/.local/state/tt030-ci; mkdir -p "$st"; log=$st/last.log; : > "$log"
rev=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
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
    local r
    for r in $rpms; do
        if [ -e "$MIRROR/built/$(basename "$r")" ]; then             # this NVR is already on the mirror:
            cmp -s "$r" "$MIRROR/built/$(basename "$r")" && continue    # same bytes (builds are reproducible), or
            echo "publish: $(basename "$r") on the mirror has different content: bump the version" >&2; return 1
        fi
        /opt/sam/venv/bin/python3 scripts/tt_rpm.py add "$MIRROR" "$r" || return 1
        published="$published $(basename "$r")"
    done
}
if [ -z "$fails" ]; then step publish publish; else echo "== publish: skipped (red run)" | tee -a "$log"; fi
outcome=$([ -z "$fails" ] && echo pass || echo fail)
# what the TT runs vs the mirror (tt_versions.py): reported, never part of the outcome (a TT that has not run
# yum update yet is not a code failure)
tt=$(/opt/sam/venv/bin/python3 scripts/tt_versions.py "$MIRROR" 2>&1 | tee -a "$log" | tail -1); tt=${tt#tt_versions: }
echo "$(date -u +%FT%TZ) rev=$rev outcome=$outcome failed=${fails:-none} published=${published:-none} tt=[$tt]" | sed "s/published= /published=/" | tee "$st/last.txt"
PYTHONPATH=/opt/sam/runtime/src /opt/sam/venv/bin/python3 -c "
from sam.utils.sam_logger import emit
emit('tt030.ci.run', outcome='$outcome', rev='$rev', failed='${fails:-none}', published='${published:-none}', tt='$tt')" >/dev/null 2>&1 || echo "ci: central log emit failed (result still in $st)" >&2
# a commit made during this run fired the path unit while the service was active, and systemd dropped that trigger:
# run again on the new HEAD instead of leaving it untested until the nightly (2026-10-14)
[ "$(git rev-parse --short HEAD 2>/dev/null)" != "$rev" ] && exec "$0" "$@"
[ -z "$fails" ]
exit
}
