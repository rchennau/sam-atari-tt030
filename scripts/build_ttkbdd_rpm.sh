#!/bin/bash
# Package the TT's remote keyboard daemon (src/ttkbdd.c, protocol v3 — must match scripts/ttkbd_send.py) as
# build/ttkbdd-<ver>-<rel>.m68kmint.rpm: /usr/sbin/ttkbdd, /etc/rc.ttkbdd (WiFi listener, started from mint.cnf).
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
grep -q "hello\[0\] = ${V%%.*};" src/ttkbdd.c || { echo "build_ttkbdd_rpm: src/ttkbdd.c does not speak v${V%%.*}" >&2; exit 1; }
cp build/ttkbdd.prg staging/HD10_OVERLAY/usr/sbin/ttkbdd
python3 - "$V" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
v = sys.argv[1]
out = f"build/ttkbdd-{v}-1.m68kmint.rpm"
open(out, "wb").write(R.write_rpm("ttkbdd", v, "1", [
    ("/usr/sbin/ttkbdd", 0o100755, open("build/ttkbdd.prg", "rb").read()),
    ("/etc/rc.ttkbdd", 0o100755, open("staging/HD10_OVERLAY/etc/rc.ttkbdd", "rb").read()),
], summary="TT030 remote keyboard + mouse daemon, protocol v3 (sam)",
   description="Injects keys and mouse from fractal (scripts/ttkbd_send.py) over WiFi or serial; Monocypher crypto. "
               "The sender key /etc/ttkbd.pub is not in the package."))
print(out)
PY
