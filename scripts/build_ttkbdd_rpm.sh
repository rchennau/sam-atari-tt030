#!/bin/bash
# Package the TT's remote keyboard daemon (src/ttkbdd.c, protocol v3 — must match scripts/ttkbd_send.py) as
# build/ttkbdd-<ver>-<rel>.m68kmint.rpm: /usr/sbin/ttkbdd, /etc/rc.ttkbdd (WiFi listener, started from mint.cnf).
# FR-13: /usr/bin/ttkbd-request + /opt/ttkbd/ttkbd.prg (TT desktop icon: signed start request to the host).
# /etc/ttkbd.pub (the operator's sender key) stays outside the package. Stripped, so the build is reproducible.
# Also refreshes staging/HD10_OVERLAY/usr/sbin/ttkbdd, so a new card image cannot ship an older protocol than the
# sender (board decd9851: the overlay copy stayed v1 after the source moved to v3). Prints the RPM path.
. "$(dirname "$0")/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
cd "$(dirname "$0")/.."
V=${1:-3.0}   # the wire protocol version this build speaks
M=tools/monocypher-4.0.3/src
tools/cross-mint/usr/bin/m68k-atari-mint-gcc -m68020-60 -O2 -s -o build/ttkbdd.prg src/ttkbdd.c \
    $M/monocypher.c $M/optional/monocypher-ed25519.c -I$M -I$M/optional
tools/cross-mint/usr/bin/m68k-atari-mint-gcc -m68020-60 -O2 -s -o build/ttkbd.prg src/ttkbdreq.c -lgem
grep -q "hello\[0\] = ${V%%.*};" src/ttkbdd.c || { echo "build_ttkbdd_rpm: src/ttkbdd.c does not speak v${V%%.*}" >&2; exit 1; }
cp build/ttkbdd.prg staging/HD10_OVERLAY/usr/sbin/ttkbdd
# Release = commits touching this package's inputs: any input change gets a new NVR, so CI's publish gate never
# meets "same NVR, different bytes" (ttkbdd-3.0-1 kept CI red from 2026-10-15, 3c7ee96 changed a string). 2026-10-18.
REL=$(git rev-list --count HEAD -- src/ttkbdd.c src/ttkbdreq.c src/ttkbd-request.sh staging/HD10_OVERLAY/etc/rc.ttkbdd scripts/build_ttkbdd_rpm.sh scripts/rpm_header.py)
python3 - "$V" "$REL" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
v = sys.argv[1]
rel = sys.argv[2]
out = f"build/ttkbdd-{v}-{rel}.m68kmint.rpm"
open(out, "wb").write(R.write_rpm("ttkbdd", v, rel, [
    ("/usr/sbin/ttkbdd", 0o100755, open("build/ttkbdd.prg", "rb").read()),
    ("/etc/rc.ttkbdd", 0o100755, open("staging/HD10_OVERLAY/etc/rc.ttkbdd", "rb").read()),
    ("/usr/bin/ttkbd-request", 0o100755, open("src/ttkbd-request.sh", "rb").read()),
    ("/opt/ttkbd/ttkbd.prg", 0o100755, open("build/ttkbd.prg", "rb").read()),
], summary="TT030 remote keyboard + mouse daemon, protocol v3 (sam)",
   description="Injects keys and mouse from the remote system management host (scripts/ttkbd_send.py) over WiFi or serial; Monocypher crypto. "
               "The sender key /etc/ttkbd.pub is not in the package."))
print(out)
PY
