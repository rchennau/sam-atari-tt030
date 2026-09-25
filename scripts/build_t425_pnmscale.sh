#!/bin/sh
# tpnmscale as one RPM, build/tpnmscale-1.0-1.m68kmint.rpm (/usr/bin/tpnmscale). tt030-t425-kernel-ports W2: the T425 path
# was DECLINED at the FR-5 gate (switch on / off 0.80-1.18, perf.md); the win is the in-memory netpbm I/O
# (pmshim.c) on the 68030, so neither the server nor a -t425-on RPM ships. t4*.sh still builds the server
# for a re-measure.
# Both sides compile the same prepended copy of pnmscale.c (sam-yum-tt/t4pnms.sh makes it) with pmshim.c
# and -DPPM_PACKCOLORS, so the T425 and the 68030 fallback give identical output. -ffloat-store: pnmscale's
# scale is float; without it the 68882 keeps the quotient in extended precision and the fixed-point scale
# can differ from the T425's single-precision one.
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
RT=$REPO/sam-yum-tt/src
W=/tmp/tpnms-build
CC=$REPO/tools/cross-mint/usr/bin/m68k-atari-mint-gcc
sh "$REPO/sam-yum-tt/t4pnms.sh" /tmp/ps > /dev/null
rm -rf "$W" && mkdir -p "$W" && cp /tmp/ps/*.c /tmp/ps/*.h "$W"/
rm -f "$W"/unistd.h "$W"/fcntl.h         # T425-only stubs (and /tmp/ps/sys is not copied)
cp "$RT/tpnmscale.c" "$W"/
cd "$W"
"$CC" -m68020-60 -O2 -ffloat-store -s -w -DPPM_PACKCOLORS -I. -I"$RT" -o tpnmscale tpnmscale.c pnmscale.c pmshim.c \
    "$RT/t4call.c" "$RT/t4lock.c" "$RT/t4adler.c" "$RT/atwboot.c" -lm
cd "$REPO"
python3 - "$W" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
w = sys.argv[1]
open("build/tpnmscale-1.0-1.m68kmint.rpm", "wb").write(R.write_rpm("tpnmscale", "1.0", "1", [
    ("/usr/bin/tpnmscale", 0o100755, open(f"{w}/tpnmscale", "rb").read()),
], summary="pnmscale -xsize/-ysize/-xysize in memory on the 68030, 0.4-0.5x stock's time (sam)"))
print("build/tpnmscale-1.0-1.m68kmint.rpm")
PY
