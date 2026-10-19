#!/bin/sh
# tppmquant (netpbm ppmquant's default mode on the T425; tt030-t425-kernel-ports W2), as two RPMs:
#   build/tppmquant-1.0-<REL>.m68kmint.rpm      /usr/bin/tppmquant + /usr/lib/t425/tppmquant.btl
#   build/tppmquant-t425-on-1.0-<REL>.m68kmint.rpm  /etc/t425/enabled/tppmquant   (held until the FR-5 gate)
# Both sides compile the same prepended copies of ppmquant.c + libppm3.c (sam-yum-tt/t4ppmq.sh makes them)
# with pmshim.c and -DPPM_PACKCOLORS, so the T425 and the 68030 fallback give identical output.
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
RT=$REPO/sam-yum-tt/src
W=/tmp/tppmq-build
CC=$REPO/tools/cross-mint/usr/bin/m68k-atari-mint-gcc
# ttjob-ports FR-4: the shipped binary IS the ttjobd client (falls back to the direct T425 path, then the 68030);
# TTJOB=0 builds the old direct-only binary. Release = commits touching this port's inputs, so ci.sh never meets
# "same NVR, different bytes".
TJ=; TJS=; [ "${TTJOB:-1}" = 1 ] && { TJ="-DT4CALL_TTJOB -I$REPO/sam-job-tt"; TJS="$TJ $REPO/sam-job-tt/libttjob.c"; }
mkdir -p "$REPO/build"   # a fresh CI worktree has none
REL=$(git -C "$REPO" rev-list --count HEAD -- scripts/build_t425_ppmquant.sh sam-yum-tt/src sam-job-tt/libttjob.c sam-job-tt/jobwire.h)
sh "$REPO/sam-yum-tt/t4ppmq.sh" /tmp/pq > /dev/null
rm -rf "$W" && mkdir -p "$W" && cp /tmp/pq/*.c /tmp/pq/*.h "$W"/
rm -f "$W"/unistd.h "$W"/fcntl.h         # T425-only stubs (and /tmp/pq/sys is not copied)
cp "$RT/tppmquant.c" "$W"/
cd "$W"
"$CC" -m68020-60 -O2 -s -w $TJ -DPPM_PACKCOLORS -I. -I"$RT" -o tppmquant tppmquant.c ppmquant.c libppm3.c pmshim.c \
    "$RT/t4call.c" "$RT/t4lock.c" "$RT/t4adler.c" "$RT/atwboot.c" $TJS
[ -n "$TJ" ] && cp "$W/tppmquant" "$REPO/build/tppmquant-ttjob"   # test binary (same bytes as the packaged tppmquant)
cd "$REPO"
python3 - "$W" "$REL" <<'PY'
import os, sys
sys.path.insert(0, "scripts")
import rpm_header as R
w, rel_ = sys.argv[1], sys.argv[2]
out = [os.path.abspath(f"build/{n}-1.0-{rel_}.m68kmint.rpm") for n in ("tppmquant", "tppmquant-t425-on")]
open(out[0], "wb").write(R.write_rpm("tppmquant", "1.0", rel_, [
    ("/usr/bin/tppmquant", 0o100755, open(f"{w}/tppmquant", "rb").read()),
    ("/usr/lib/t425/tppmquant.btl", 0o100644, open("/tmp/pq/ppmqserv.b4h", "rb").read()),
], summary="ppmquant's default mode on the ATW800/2 T425, same result on the 68030 (sam)"))
open(out[1], "wb").write(R.write_rpm("tppmquant-t425-on", "1.0", rel_, [
    ("/etc/t425/enabled/tppmquant", 0o100644, b""),
], summary="turn on T425 offload for tppmquant (after its FR-5 gate)", requires_=["tppmquant"]))
print(*out)   # last line = the RPM paths (ci.sh build_rpms reads it)
PY
