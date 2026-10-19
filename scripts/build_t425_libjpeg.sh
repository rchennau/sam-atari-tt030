#!/bin/sh
# libjpeg 8b with cjpeg/djpeg on the T425 (tt030-t425-kernel-ports W2), as two RPMs:
#   build/libjpeg-8b-<REL>.m68kmint.rpm      cjpeg djpeg jpegtran rdjpgcom wrjpgcom + /usr/lib/t425/libjpeg.btl
#                                            (+ SpareMiNT's README and man pages)
#   build/libjpeg-t425-on-8b-<REL>.m68kmint.rpm  /etc/t425/enabled/libjpeg   (held until the FR-5 gate)
# Built like SpareMiNT's spec (configure --enable-static, stripped, 64 KB stack: its `stack --fix=64k`
# becomes a linked `_stksize` object, mintlib's own knob). cjpeg.c/djpeg.c #include t4jpegfe.c and call
# it first: plain invocations go to jpegserv (libjpeg compiled unchanged for the T425, sam-yum-tt/t4jpeg.sh).
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
SRPM=$REPO/tools/rpm-src/x/libjpeg-8b-2
RT=$REPO/sam-yum-tt/src
W=/tmp/jpeg-t425-build
XBIN=$REPO/tools/cross-mint/usr/bin
CC="$XBIN/m68k-atari-mint-gcc"
rm -rf "$W" && mkdir -p "$W" && tar xzf "$SRPM/jpegsrc.v8b.tar.gz" -C "$W"
cd "$W/jpeg-8b"
python3 - "$RT" <<'PY'
import sys
for prog, flag in (("djpeg", "T4J_DJPEG"), ("cjpeg", "T4J_CJPEG")):
    p = f"{prog}.c"
    s = open(p, encoding="latin-1").read()
    inc = '#include "cdjpeg.h"'
    assert inc in s, f"{p}: cdjpeg.h include not found"
    s = s.replace(inc, inc + f'\n#ifdef T425\n#define {flag}\n#include "t4jpegfe.c"\n#endif', 1)
    call = "  progname = argv[0];\n"
    assert call in s, f"{p}: progname line not found"
    s = s.replace(call, call + f"#ifdef T425\n  if (t4_{prog}(argc, argv) == 0)\n    exit(EXIT_SUCCESS);\n#endif\n", 1)
    open(p, "w", encoding="latin-1").write(s)
PY
cp "$RT/t4jpegfe.c" .
OBJS=
# ttjob-ports FR-4: the shipped binary IS the ttjobd client (falls back to the direct T425 path, then the 68030);
# TTJOB=0 builds the old direct-only binary. Release = commits touching this port's inputs, so ci.sh never meets
# "same NVR, different bytes".
TJ=; [ "${TTJOB:-1}" = 1 ] && TJ="-DT4CALL_TTJOB -I$REPO/sam-job-tt"
mkdir -p "$REPO/build"   # a fresh CI worktree has none
REL=$(git -C "$REPO" rev-list --count HEAD -- scripts/build_t425_libjpeg.sh sam-yum-tt/src sam-job-tt/libttjob.c sam-job-tt/jobwire.h)
for c in t4call t4lock t4adler atwboot; do
    "$CC" -m68020-60 -O2 -c $TJ -I"$RT" -o "$W/$c.o" "$RT/$c.c"; OBJS="$OBJS $W/$c.o"
done
[ -n "$TJ" ] && { "$CC" -m68020-60 -O2 -c -I"$REPO/sam-job-tt" -I"$RT" -o "$W/libttjob.o" "$REPO/sam-job-tt/libttjob.c"; OBJS="$OBJS $W/libttjob.o"; }
printf 'long _stksize = 64L * 1024;   /* = SpareMiNT spec: stack --fix=64k */\n' > "$W/stksize.c"
"$CC" -c -o "$W/stksize.o" "$W/stksize.c"; OBJS="$OBJS $W/stksize.o"
PATH=$XBIN:$PATH CFLAGS="-O2 -m68020-60 -DT425 $TJ -I$RT" \
    ./configure --host=m68k-atari-mint --prefix=/usr --enable-static --disable-shared > "$W/configure.log" 2>&1 \
    || { tail -20 "$W/configure.log"; exit 1; }
# per-program link additions (LIBS would also reach libtool's libjpeg.la, which refuses plain objects)
PATH=$XBIN:$PATH make -s cjpeg_LDADD="libjpeg.la $OBJS" djpeg_LDADD="libjpeg.la $OBJS" \
    jpegtran_LDADD="libjpeg.la $W/stksize.o" rdjpgcom_LDADD="$W/stksize.o" wrjpgcom_LDADD="$W/stksize.o" \
    > "$W/make.log" 2>&1 || { tail -25 "$W/make.log"; exit 1; }
for b in cjpeg djpeg jpegtran rdjpgcom wrjpgcom; do "$XBIN/m68k-atari-mint-strip" "$b"; done
[ -n "$TJ" ] && { cp cjpeg "$REPO/build/cjpeg-ttjob"; cp djpeg "$REPO/build/djpeg-ttjob"; }   # test binaries (same bytes as the packaged cjpeg/djpeg)
sh "$REPO/sam-yum-tt/t4jpeg.sh" /tmp/jp > /dev/null
mkdir -p "$W/stock" && (cd "$W/stock" && 7z e -y /mnt/vault/tt030/m68kmint/libjpeg-8b-2.m68kmint.rpm > /dev/null \
    && cpio -idm --quiet < libjpeg-8b-2.m68kmint.cpio)
cd "$REPO"
python3 - "$W" "$REL" <<'PY'
import os, sys
sys.path.insert(0, "scripts")
import rpm_header as R
w, rel_ = sys.argv[1], sys.argv[2]
files = [(f"/usr/bin/{b}", 0o100755, open(f"{w}/jpeg-8b/{b}", "rb").read())
         for b in ("cjpeg", "djpeg", "jpegtran", "rdjpgcom", "wrjpgcom")]
stock = os.path.join(w, "stock")
for root, dirs, names in sorted(os.walk(stock)):
    for n in sorted(names):
        rel = "/" + os.path.relpath(os.path.join(root, n), stock)
        if rel.startswith(("/usr/doc/", "/usr/share/man/")):
            files.append((rel, 0o100644, open(os.path.join(root, n), "rb").read()))
files.append(("/usr/lib/t425/libjpeg.btl", 0o100644, open("/tmp/jp/jpegserv.b4h", "rb").read()))
out = [os.path.abspath(f"build/{n}-8b-{rel_}.m68kmint.rpm") for n in ("libjpeg", "libjpeg-t425-on")]
open(out[0], "wb").write(R.write_rpm("libjpeg", "8b", rel_, files,
    summary="libjpeg 8b tools; cjpeg/djpeg offload to the ATW800/2 T425 when libjpeg-t425-on is installed (sam)",
    provides=["libjpeg"]))
open(out[1], "wb").write(R.write_rpm("libjpeg-t425-on", "8b", rel_,
    [("/etc/t425/enabled/libjpeg", 0o100644, b"")], summary="turn on T425 offload for cjpeg/djpeg (after its FR-5 gate)",
    requires_=["libjpeg"]))
print(len(files), "files", file=sys.stderr)
print(*out)   # last line = the RPM paths (ci.sh build_rpms reads it)
PY
