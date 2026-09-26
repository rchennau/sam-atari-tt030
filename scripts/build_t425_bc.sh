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
REL=${1:-2}
sh "$REPO/sam-yum-tt/t4bc.sh" /tmp/bq > /dev/null
# tbc.btl since 1.0-2: the same bc sources through llvm-t800 -Os (0.85 / 0.94 / 0.94 of the icc image on the TT,
# PMS tt030-llvm-t800-adoption FR-5). The source order below is the image timed on the TT.
rm -rf /tmp/bql && sh "$REPO/sam-yum-tt/t4bc.sh" /tmp/bql --copies && rm -rf /tmp/bql/sys
(cd /tmp/bql && PATH=$REPO/tools/llvm-t800/bin:$PATH t800-cc -mcpu=t425 -mboard=atw800 -mmem=5M -mhost=none -Os -w \
    -std=gnu89 -Dwarn=bc_warn -I. -o bcserv bcserv.c t4serv.c t4adler.c bc.c scan.c execute.c global.c load.c main.c \
    storage.c util.c number.c getopt.c getopt1.c bcshim.c)
"$REPO/tools/llvm-t800/bin/llvm-nm" /tmp/bql/bcserv.elf | grep -q '80500000 A __ram_end' \
    || { echo "bcserv: not confined to 5 MB (NFR-2)" >&2; exit 1; }
rm -rf "$W" && mkdir -p "$W" && cp /tmp/bq/*.c /tmp/bq/*.h "$W"/ && cp "$RT/tbc.c" "$W"/
cd "$W"
"$CC" -m68020-60 -O2 -s -w -std=gnu89 -I. -I"$RT" -o tbc tbc.c bc.c scan.c execute.c global.c load.c main.c \
    storage.c util.c number.c getopt.c getopt1.c bcshim.c \
    "$RT/t4call.c" "$RT/t4lock.c" "$RT/t4adler.c" "$RT/atwboot.c"
cd "$REPO"
python3 - "$W" "$REL" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
w = sys.argv[1]
open(f"build/tbc-1.0-{sys.argv[2]}.m68kmint.rpm", "wb").write(R.write_rpm("tbc", "1.0", sys.argv[2], [
    ("/usr/bin/tbc", 0o100755, open(f"{w}/tbc", "rb").read()),
    ("/usr/lib/t425/tbc.btl", 0o100644, open("/tmp/bql/bcserv.btl", "rb").read()),
], summary="GNU bc 1.05 for scripts on the ATW800/2 T425, same result on the 68030 (sam)",
   description="tbc.btl is built with llvm-t800, whose runtime is Apache-2.0 WITH LLVM-exception (llvm-t800 LICENSE)."))
open("build/tbc-t425-on-1.0-1.m68kmint.rpm", "wb").write(R.write_rpm("tbc-t425-on", "1.0", "1", [
    ("/etc/t425/enabled/tbc", 0o100644, b""),
], summary="turn on T425 offload for tbc (after its FR-5 gate)", requires_=["tbc"]))
print(f"build/tbc-1.0-{sys.argv[2]}.m68kmint.rpm (tbc-t425-on unchanged)")
PY
