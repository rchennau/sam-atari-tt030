#!/bin/sh
# tpnmtopng (raw P5/P6 -> PNG on the T425; tt030-t425-kernel-ports W2), as two RPMs:
#   build/tpnmtopng-1.0-<REL>.m68kmint.rpm      /usr/bin/tpnmtopng + /usr/lib/t425/tpnmtopng.btl
#   build/tpnmtopng-t425-on-1.0-<REL>.m68kmint.rpm  /etc/t425/enabled/tpnmtopng   (held until the FR-5 gate)
# Both sides compile pngenc.c + the pinned zlib 1.3.2 (deflate side), so the T425 and the 68030 give
# identical files.
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
RT=$REPO/sam-yum-tt/src
Z=$REPO/tools/rpm-src/zlib-1.3.2
CC=$REPO/tools/cross-mint/usr/bin/m68k-atari-mint-gcc
W=/tmp/tpng-build
sh "$REPO/sam-yum-tt/t4png.sh" /tmp/pg > /dev/null
rm -rf "$W" && mkdir -p "$W"
# ttjob-ports FR-4: the shipped binary IS the ttjobd client (falls back to the direct T425 path, then the 68030);
# TTJOB=0 builds the old direct-only binary. Release = commits touching this port's inputs, so ci.sh never meets
# "same NVR, different bytes".
TJ=; TJS=; [ "${TTJOB:-1}" = 1 ] && { TJ="-DT4CALL_TTJOB -I$REPO/sam-job-tt"; TJS="$TJ $REPO/sam-job-tt/libttjob.c"; }
mkdir -p "$REPO/build"   # a fresh CI worktree has none
REL=$(git -C "$REPO" rev-list --count HEAD -- scripts/build_t425_pnmtopng.sh sam-yum-tt/src sam-job-tt/libttjob.c sam-job-tt/jobwire.h)
"$CC" -m68020-60 -O2 -s -w $TJ -I"$RT" -I"$Z" -o "$W/tpnmtopng" "$RT/tpnmtopng.c" "$RT/pngenc.c" \
    "$Z"/adler32.c "$Z"/crc32.c "$Z"/deflate.c "$Z"/trees.c "$Z"/zutil.c \
    "$RT/t4call.c" "$RT/t4lock.c" "$RT/t4adler.c" "$RT/atwboot.c" $TJS
[ -n "$TJ" ] && cp "$W/tpnmtopng" "$REPO/build/tpnmtopng-ttjob"   # test binary (same bytes as the packaged tpnmtopng)
python3 - "$W" "$REL" <<'PY'
import os, sys
sys.path.insert(0, "scripts")
import rpm_header as R
w, rel_ = sys.argv[1], sys.argv[2]
out = [os.path.abspath(f"build/{n}-1.0-{rel_}.m68kmint.rpm") for n in ("tpnmtopng", "tpnmtopng-t425-on")]
open(out[0], "wb").write(R.write_rpm("tpnmtopng", "1.0", rel_, [
    ("/usr/bin/tpnmtopng", 0o100755, open(f"{w}/tpnmtopng", "rb").read()),
    ("/usr/lib/t425/tpnmtopng.btl", 0o100644, open("/tmp/pg/pngserv.b4h", "rb").read()),
], summary="pnmtopng's default mode on the ATW800/2 T425, same result on the 68030 (sam)"))
open(out[1], "wb").write(R.write_rpm("tpnmtopng-t425-on", "1.0", rel_, [
    ("/etc/t425/enabled/tpnmtopng", 0o100644, b""),
], summary="turn on T425 offload for tpnmtopng (after its FR-5 gate)", requires_=["tpnmtopng"]))
print(*out)   # last line = the RPM paths (ci.sh build_rpms reads it)
PY
