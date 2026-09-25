#!/bin/sh
# Build libjpeg 8b for the T425 (jpegserv; tt030-t425-kernel-ports W2) with the INMOS toolset.
#   sam-yum-tt/t4jpeg.sh [DIR]   -> DIR/jpegserv.b4h (ships as /usr/lib/t425/libjpeg.btl), DIR/jpegt4test.b4h
# DIR must be SHORT (ilink's ISEARCH limit; default /tmp/jp). The SpareMiNT source is copied, never edited.
#   jconfig.h  = libjpeg's own jconfig.st (Atari ST): its settings shape declarations and memory-pool
#                alignment only, not the output; RIGHT_SHIFT_IS_UNSIGNED stays off, so
#   -fs        icc's "make right-shifts arithmetic": libjpeg's integer DCT needs arithmetic shifts
#   jmemnobs.c the malloc-only memory manager (no temp files: there are none on the T425)
#   NO_GETENV  jmemmgr.c reads getenv("JPEGMEM"); on a transputer getenv() is an SP_GETENV request to the
#              HOST over the link. t4 (the emulator) is a host server and answers; on the real TT nothing
#              answers after boot, so the request's bytes came back as a garbage reply and the server hung
#              (found 2026-09-24 by printing the raw reply: 0a 00 20 07 00 "JPEGMEM").
set -eu
R=$(cd "$(dirname "$0")/.." && pwd)
D=${1:-/tmp/jp}
J=$R/tools/rpm-src/x/libjpeg-8b-2/jpeg-8b
rm -rf "$D" && mkdir -p "$D" && cd "$D"
LIB="jaricom jcapimin jcapistd jcarith jccoefct jccolor jcdctmgr jchuff jcinit jcmainct jcmarker jcmaster
 jcomapi jcparam jcprepct jcsample jctrans jdapimin jdapistd jdarith jdatadst jdatasrc jdcoefct jdcolor
 jddctmgr jdhuff jdinput jdmainct jdmarker jdmaster jdmerge jdpostct jdsample jdtrans jerror jfdctflt
 jfdctfst jfdctint jidctflt jidctfst jidctint jquant1 jquant2 jutils jmemmgr jmemnobs rdswitch"
for f in $LIB; do cp "$J/$f.c" .; done
cp "$J"/*.h .
cp "$J/jconfig.st" jconfig.h
cp "$R"/sam-yum-tt/src/jpegserv.c "$R"/sam-yum-tt/src/jpegt4test.c "$R"/sam-yum-tt/src/t4serv.[ch] \
   "$R"/sam-yum-tt/src/t4adler.[ch] .
. "$R/tools/d72uni-env.sh"
ISEARCH="$D/ $D72UNI/libs/"
export ISEARCH IBOARDSIZE
for f in $LIB jpegserv jpegt4test t4serv t4adler; do
    icc "$f.c" -t4 -fs -dNO_GETENV -o "$f.t4h" > "$f.log" 2>&1 || { cat "$f.log"; exit 1; }
    grep -E '^(Serious|Error|Fatal)' "$f.log" && exit 1
done
for m in jpegserv jpegt4test; do
    extra=jpegserv.t4h; [ "$m" = jpegserv ] && extra="t4serv.t4h t4adler.t4h"   # the test calls the kernel directly
    { echo "$m.t4h"; [ -n "$extra" ] && printf "%s\n" $extra; for f in $LIB; do echo "$f.t4h"; done; echo '#include startup.lnk'; } > "$m.lnk"
    ilink -f "$m.lnk" -t4 -h -o "$m.c4h"
    icollect "$m.c4h" -t -o "$m.b4h"
    [ -s "$m.b4h" ] || { echo "t4jpeg: $m.b4h not built" >&2; exit 1; }
done
ls -l "$D"/jpegserv.b4h "$D"/jpegt4test.b4h
