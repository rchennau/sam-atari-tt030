#!/bin/bash
# ci.sh — TT030 side-repo CI (tt030-transputer-job-queue FR-9, operator MCQ 2026-10-13). Runs on fractal, no TT:
#   1. host test suites: scripts/test_*.py under pytest (excludes the two that need the TT or Hatari:
#      test_e2e_ping_rpm.py, test_hatari_ping_rpm.sh)
#   2. the job-queue emulated lane: sam-job-tt/emu/run_emu_tests.sh (libttjob + host-vs-t4 jobtest)
#   3. the resident transputer image builds (sam-job-tt/build_t4.sh)
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
outcome=$([ -z "$fails" ] && echo pass || echo fail)
echo "$(date -u +%FT%TZ) rev=$rev outcome=$outcome failed=${fails:-none}" | tee "$st/last.txt"
PYTHONPATH=/opt/sam/runtime/src /opt/sam/venv/bin/python3 -c "
from sam.utils.sam_logger import emit
emit('tt030.ci.run', outcome='$outcome', rev='$rev', failed='${fails:-none}')" >/dev/null 2>&1 || echo "ci: central log emit failed (result still in $st)" >&2
[ -z "$fails" ]
