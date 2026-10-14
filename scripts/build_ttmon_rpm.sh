#!/bin/bash
# Package ttmon (src/ttmon.c: health telemetry + the runtime frame read by scripts/tt_versions.py) as
# build/ttmon-<ver>-1.m68kmint.rpm: /usr/sbin/ttmon, /etc/rc.ttmon (started from mint.cnf). Prints the RPM path.
. "$(dirname "$0")/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
cd "$(dirname "$0")/.."
V=${1:-1.1}   # 1.1: adds the retained sam/node/atari-tt030/runtime frame
tools/cross-mint/usr/bin/m68k-atari-mint-gcc -m68020-60 -Os -s -Wall -o build/ttmon src/ttmon.c
python3 - "$V" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
v = sys.argv[1]
out = f"build/ttmon-{v}-1.m68kmint.rpm"
open(out, "wb").write(R.write_rpm("ttmon", v, "1", [
    ("/usr/sbin/ttmon", 0o100755, open("build/ttmon", "rb").read()),
    ("/etc/rc.ttmon", 0o100755, open("staging/HD10_OVERLAY/etc/rc.ttmon", "rb").read()),
], summary="TT030 telemetry to SAM over MQTT: health every 60 s, installed packages on change (sam)"))
print(out)
PY
