#!/bin/sh
# tppmquant (netpbm ppmquant's default mode on the T425; tt030-t425-kernel-ports W2), as two RPMs:
#   build/tppmquant-1.0-1.m68kmint.rpm          /usr/bin/tppmquant + /usr/lib/t425/tppmquant.btl
#   build/tppmquant-t425-on-1.0-1.m68kmint.rpm  /etc/t425/enabled/tppmquant   (held until the FR-5 gate)
# Both sides compile the same prepended copies of ppmquant.c + libppm3.c (sam-yum-tt/t4ppmq.sh makes them)
# with pmshim.c and -DPPM_PACKCOLORS, so the T425 and the 68030 fallback give identical output.
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
RT=$REPO/sam-yum-tt/src
W=/tmp/tppmq-build
CC=$REPO/tools/cross-mint/usr/bin/m68k-atari-mint-gcc
sh "$REPO/sam-yum-tt/t4ppmq.sh" /tmp/pq > /dev/null
rm -rf "$W" && mkdir -p "$W" && cp /tmp/pq/*.c /tmp/pq/*.h "$W"/
rm -f "$W"/unistd.h "$W"/fcntl.h         # T425-only stubs (and /tmp/pq/sys is not copied)
cp "$RT/tppmquant.c" "$W"/
cd "$W"
"$CC" -m68020-60 -O2 -s -w -DPPM_PACKCOLORS -I. -I"$RT" -o tppmquant tppmquant.c ppmquant.c libppm3.c pmshim.c \
    "$RT/t4call.c" "$RT/t4lock.c" "$RT/t4adler.c" "$RT/atwboot.c"
cd "$REPO"
python3 - "$W" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
w = sys.argv[1]
open("build/tppmquant-1.0-1.m68kmint.rpm", "wb").write(R.write_rpm("tppmquant", "1.0", "1", [
    ("/usr/bin/tppmquant", 0o100755, open(f"{w}/tppmquant", "rb").read()),
    ("/usr/lib/t425/tppmquant.btl", 0o100644, open("/tmp/pq/ppmqserv.b4h", "rb").read()),
], summary="ppmquant's default mode on the ATW800/2 T425, same result on the 68030 (sam)"))
open("build/tppmquant-t425-on-1.0-1.m68kmint.rpm", "wb").write(R.write_rpm("tppmquant-t425-on", "1.0", "1", [
    ("/etc/t425/enabled/tppmquant", 0o100644, b""),
], summary="turn on T425 offload for tppmquant (after its FR-5 gate)", requires_=["tppmquant"]))
print("build/tppmquant-1.0-1.m68kmint.rpm build/tppmquant-t425-on-1.0-1.m68kmint.rpm")
PY
