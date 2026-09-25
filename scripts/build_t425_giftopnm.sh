#!/bin/sh
# tgiftopnm as one RPM, build/tgiftopnm-1.0-1.m68kmint.rpm (/usr/bin/tgiftopnm). tt030-t425-kernel-ports W2: the T425 path
# was DECLINED at the FR-5 gate (switch on / off 0.80-1.18, perf.md); the win is the in-memory netpbm I/O
# (pmshim.c) on the 68030, so neither the server nor a -t425-on RPM ships. t4*.sh still builds the server
# for a re-measure.
# Both sides compile the same prepended copy of giftopnm.c (sam-yum-tt/t4gif.sh makes it) with pmshim.c
# and -DPPM_PACKCOLORS, so the T425 and the 68030 fallback give identical output.
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
RT=$REPO/sam-yum-tt/src
W=/tmp/tgif-build
CC=$REPO/tools/cross-mint/usr/bin/m68k-atari-mint-gcc
sh "$REPO/sam-yum-tt/t4gif.sh" /tmp/gf > /dev/null
rm -rf "$W" && mkdir -p "$W" && cp /tmp/gf/*.c /tmp/gf/*.h "$W"/
rm -f "$W"/unistd.h "$W"/fcntl.h         # T425-only stubs (and /tmp/gf/sys is not copied)
cp "$RT/tgiftopnm.c" "$W"/
cd "$W"
"$CC" -m68020-60 -O2 -s -w -DPPM_PACKCOLORS -I. -I"$RT" -o tgiftopnm tgiftopnm.c giftopnm.c pmshim.c \
    "$RT/t4call.c" "$RT/t4lock.c" "$RT/t4adler.c" "$RT/atwboot.c"
cd "$REPO"
python3 - "$W" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
w = sys.argv[1]
open("build/tgiftopnm-1.0-1.m68kmint.rpm", "wb").write(R.write_rpm("tgiftopnm", "1.0", "1", [
    ("/usr/bin/tgiftopnm", 0o100755, open(f"{w}/tgiftopnm", "rb").read()),
], summary="giftopnm in memory on the 68030, 0.4-0.5x stock's time (sam)"))
print("build/tgiftopnm-1.0-1.m68kmint.rpm")
PY
