#!/bin/bash
# ci.sh — TT030 side-repo CI (tt030-transputer-job-queue FR-9, operator MCQ 2026-10-13). Runs on fractal, no TT:
#   1. host test suites: scripts/test_*.py under pytest (excludes the two that need the TT or Hatari:
#      test_e2e_ping_rpm.py, test_hatari_ping_rpm.sh)
#   2. the job-queue emulated lane: sam-job-tt/emu/run_emu_tests.sh (libttjob + host-vs-t4 jobtest)
#   3. the resident transputer image builds (sam-job-tt/build_t4.sh)
#   4. the job-queue RPMs build (sam-job-tt/build_rpm.sh: ttjob; scripts/build_t425_compress.sh: t425-compress)
#   5. publish gate (FR-9): only when 1-4 all passed, each RPM whose name (NVR) the LAN mirror lacks is added with
#      tt_rpm.py add (signed index). A red run publishes nothing; a publish failure turns the run red.
# Exit 0 only if all pass. The result is logged centrally (sam_logger event tt030.ci.run) and written to
# ~/.local/state/tt030-ci/last.txt, so SAM can read it without asking. Run by sam-tt030-ci.service (iac/fractal).
set -uo pipefail
cd "$(dirname "$0")/.."
st=~/.local/state/tt030-ci; mkdir -p "$st"; log=$st/last.log; : > "$log"
rev=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
step() { local name=$1; shift; echo "== $name" | tee -a "$log"; if "$@" >> "$log" 2>&1; then echo "   ok" | tee -a "$log"; else echo "   FAIL" | tee -a "$log"; fails="$fails $name"; fi; }
fails=
step pytest /opt/sam/venv/bin/python3 -m pytest -q --ignore=scripts/test_e2e_ping_rpm.py scripts/
step emu    env JOBS="${JOBS:-1000}" sam-job-tt/emu/run_emu_tests.sh
step image  sam-job-tt/build_t4.sh
rpms=
build_rpms() { local r; r=$(sam-job-tt/build_rpm.sh | tail -1) && rpms="$r" && r=$(scripts/build_t425_compress.sh | tail -1) && rpms="$rpms $r"; }
step rpm    build_rpms
MIRROR=${TT_MIRROR:-/mnt/vault/tt030}
published=
publish() {
    local r
    for r in $rpms; do
        [ -e "$MIRROR/built/$(basename "$r")" ] && continue          # this NVR is already on the mirror
        /opt/sam/venv/bin/python3 scripts/tt_rpm.py add "$MIRROR" "$r" || return 1
        published="$published $(basename "$r")"
    done
}
if [ -z "$fails" ]; then step publish publish; else echo "== publish: skipped (red run)" | tee -a "$log"; fi
outcome=$([ -z "$fails" ] && echo pass || echo fail)
echo "$(date -u +%FT%TZ) rev=$rev outcome=$outcome failed=${fails:-none} published=${published:-none}" | sed "s/published= /published=/" | tee "$st/last.txt"
PYTHONPATH=/opt/sam/runtime/src /opt/sam/venv/bin/python3 -c "
from sam.utils.sam_logger import emit
emit('tt030.ci.run', outcome='$outcome', rev='$rev', failed='${fails:-none}', published='${published:-none}')" >/dev/null 2>&1 || echo "ci: central log emit failed (result still in $st)" >&2
[ -z "$fails" ]
