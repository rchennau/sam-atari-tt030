#!/bin/sh
# tbc (GNU bc 1.05 for scripts on the T425; tt030-t425-kernel-ports W3), as two RPMs:
#   build/tbc-1.0-1.m68kmint.rpm          /usr/bin/tbc + /usr/lib/t425/tbc.btl
#   build/tbc-t425-on-1.0-1.m68kmint.rpm  /etc/t425/enabled/tbc   (held until the FR-5 gate)
# Both sides compile the same bcren.h-prepended bc sources (sam-yum-tt/t4bc.sh makes them) with bcshim.c,
# so the T425 and the 68030 fallback give identical output.
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
RT=$REPO/sam-yum-tt/src
CC=$REPO/tools/cross-mint/usr/bin/m68k-atari-mint-gcc
W=/tmp/tbc-build
sh "$REPO/sam-yum-tt/t4bc.sh" /tmp/bq > /dev/null
rm -rf "$W" && mkdir -p "$W" && cp /tmp/bq/*.c /tmp/bq/*.h "$W"/ && cp "$RT/tbc.c" "$W"/
cd "$W"
"$CC" -m68020-60 -O2 -s -w -std=gnu89 -I. -I"$RT" -o tbc tbc.c bc.c scan.c execute.c global.c load.c main.c \
    storage.c util.c number.c getopt.c getopt1.c bcshim.c \
    "$RT/t4call.c" "$RT/t4lock.c" "$RT/t4adler.c" "$RT/atwboot.c"
cd "$REPO"
python3 - "$W" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
w = sys.argv[1]
open("build/tbc-1.0-1.m68kmint.rpm", "wb").write(R.write_rpm("tbc", "1.0", "1", [
    ("/usr/bin/tbc", 0o100755, open(f"{w}/tbc", "rb").read()),
    ("/usr/lib/t425/tbc.btl", 0o100644, open("/tmp/bq/bcserv.b4h", "rb").read()),
], summary="GNU bc 1.05 for scripts on the ATW800/2 T425, same result on the 68030 (sam)"))
open("build/tbc-t425-on-1.0-1.m68kmint.rpm", "wb").write(R.write_rpm("tbc-t425-on", "1.0", "1", [
    ("/etc/t425/enabled/tbc", 0o100644, b""),
], summary="turn on T425 offload for tbc (after its FR-5 gate)", requires_=["tbc"]))
print("build/tbc-1.0-1.m68kmint.rpm build/tbc-t425-on-1.0-1.m68kmint.rpm")
PY
