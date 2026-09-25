#!/bin/sh
# Build tpnmtopng's encoder for the T425 (pngserv; tt030-t425-kernel-ports W2) with the INMOS toolset.
#   sam-yum-tt/t4png.sh [DIR]   -> DIR/pngserv.b4h (ships as /usr/lib/t425/tpnmtopng.btl), DIR/pngt4test.b4h
# DIR must be SHORT (default /tmp/pg). zlib 1.3.2 deflate side only, copies patched as t4compress.sh does:
#   zutil.h Z_U8 probe, deflate.c sanitizer block, #warning (icc lexes false #ifs); -dZ_SOLO -dz_off_t=long
#   (pngenc.c supplies zalloc/zfree).
set -eu
R=$(cd "$(dirname "$0")/.." && pwd)
D=${1:-/tmp/pg}
Z=$R/tools/rpm-src/zlib-1.3.2
rm -rf "$D" && mkdir -p "$D" && cd "$D"
cp "$Z"/adler32.c "$Z"/crc32.c "$Z"/deflate.c "$Z"/trees.c "$Z"/zutil.c "$Z"/zlib.h "$Z"/zconf.h \
   "$Z"/deflate.h "$Z"/zutil.h "$Z"/crc32.h "$Z"/trees.h .
cp "$R"/sam-yum-tt/src/pngenc.[ch] "$R"/sam-yum-tt/src/pngserv.c "$R"/sam-yum-tt/src/pngt4test.c \
   "$R"/sam-yum-tt/src/t4serv.[ch] "$R"/sam-yum-tt/src/t4adler.[ch] .
python3 - <<'PY'
import re
s = open("zutil.h").read()
s2 = re.sub(r"#if !defined\(Z_U8\) && !defined\(Z_SOLO\) && defined\(STDC\)\n.*?#  endif\n#endif\n", "", s, flags=re.S)
assert s2 != s, "zutil.h Z_U8 block not found"
open("zutil.h", "w").write(s2)
s = open("deflate.c").read()
blk = ('#if defined(__has_feature)\n#  if __has_feature(memory_sanitizer)\n'
       '     __attribute__((no_sanitize("memory")))\n#  endif\n#endif\n')
assert blk in s, "deflate.c sanitizer block not found"
open("deflate.c", "w").write(s.replace(blk, ""))
PY
sed -i '/^#warning/d' ./*.h ./*.c
. "$R/tools/d72uni-env.sh"
ISEARCH="$D/ $D72UNI/libs/"
export ISEARCH IBOARDSIZE
LIB="adler32 crc32 deflate trees zutil pngenc"
for f in $LIB t4serv t4adler pngserv pngt4test; do
    icc "$f.c" -t4 -dZ_SOLO -dz_off_t=long -o "$f.t4h" > "$f.log" 2>&1 || { cat "$f.log"; exit 1; }
    grep -E '^(Serious|Error|Fatal)' "$f.log" && exit 1
done
for m in pngserv pngt4test; do
    extra=; [ "$m" = pngserv ] && extra="t4serv.t4h t4adler.t4h"
    { echo "$m.t4h"; [ -n "$extra" ] && printf "%s\n" $extra; for f in $LIB; do echo "$f.t4h"; done; echo '#include startup.lnk'; } > "$m.lnk"
    ilink -f "$m.lnk" -t4 -h -o "$m.c4h"
    icollect "$m.c4h" -t -o "$m.b4h"
    [ -s "$m.b4h" ] || { echo "t4png: $m.b4h not built" >&2; exit 1; }
done
ls -l "$D"/pngserv.b4h "$D"/pngt4test.b4h
