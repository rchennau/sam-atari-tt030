#!/bin/bash
# Package ttmon (src/ttmon.c: health telemetry + the runtime frame read by scripts/tt_versions.py) as
# build/ttmon-<ver>-<rel>.m68kmint.rpm (rel = commits on its inputs): /usr/sbin/ttmon, /etc/rc.ttmon (started from mint.cnf). Prints the RPM path.
. "$(dirname "$0")/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
cd "$(dirname "$0")/.."
V=${1:-1.2}   # 1.2: runtime frame in the SAM schema sam.node.runtime/v1 (QoS 1); 1.1 added the frame
tools/cross-mint/usr/bin/m68k-atari-mint-gcc -m68020-60 -Os -s -Wall -o build/ttmon src/ttmon.c
# Release = commits touching this package's inputs: any input change gets a new NVR, so CI's publish gate never
# meets "same NVR, different bytes" (ttkbdd-3.0-1 kept CI red from 2026-10-15, 3c7ee96 changed a string). 2026-10-18.
REL=$(git rev-list --count HEAD -- src/ttmon.c staging/HD10_OVERLAY/etc/rc.ttmon scripts/build_ttmon_rpm.sh scripts/rpm_header.py)
python3 - "$V" "$REL" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
v = sys.argv[1]
rel = sys.argv[2]
out = f"build/ttmon-{v}-{rel}.m68kmint.rpm"
open(out, "wb").write(R.write_rpm("ttmon", v, rel, [
    ("/usr/sbin/ttmon", 0o100755, open("build/ttmon", "rb").read()),
    ("/etc/rc.ttmon", 0o100755, open("staging/HD10_OVERLAY/etc/rc.ttmon", "rb").read()),
], summary="TT030 telemetry to SAM over MQTT: health every 60 s, installed packages on change (sam)"))
print(out)
PY
