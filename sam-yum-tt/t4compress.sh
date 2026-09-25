#!/bin/sh
# Build zlib 1.3.2 + bzip2 1.0.8 + compkern for the T425 with the INMOS toolset (Rev. 7 benchmark, 2026-09-24).
#   sam-yum-tt/t4compress.sh [DIR]   -> DIR/compserv.b4h (upload as COMPSERV.BTL), DIR/compt4test.b4h
# DIR must be SHORT (ilink's ISEARCH fails past ~100 characters; default /tmp/cb). Patches, all to copies:
#   zutil.h   drop the Z_U8 probe: icc lexes 64-bit literals even inside a false #if and rejects them
#   *.h *.c   drop #warning (unknown directive to icc)
#   deflate.c drop the __has_feature(memory_sanitizer) attribute block (same false-#if lexing)
#   -dZ_SOLO -dz_off_t=long: no <sys/types.h>, no long long; the allocator comes from compkern.c
set -eu
R=$(cd "$(dirname "$0")/.." && pwd)
D=${1:-/tmp/cb}
Z=$R/tools/rpm-src/zlib-1.3.2
B=$R/tools/rpm-src/bzip2-1.0.8
rm -rf "$D" && mkdir -p "$D" && cd "$D"
cp "$Z"/adler32.c "$Z"/crc32.c "$Z"/deflate.c "$Z"/trees.c "$Z"/zutil.c \
   "$Z"/zlib.h "$Z"/zconf.h "$Z"/deflate.h "$Z"/zutil.h "$Z"/crc32.h "$Z"/trees.h .
for f in blocksort huffman crctable randtable decompress bzlib; do cp "$B/$f.c" .; done
cp "$B/compress.c" bzcompress.c
cp "$B"/bzlib.h "$B"/bzlib_private.h .
cp "$R"/sam-yum-tt/src/compkern.[ch] "$R"/sam-yum-tt/src/compserv.c "$R"/sam-yum-tt/src/t4serv.[ch] "$R"/sam-yum-tt/src/t4adler.[ch] "$R"/sam-yum-tt/src/compt4test.c .
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
LIB="adler32 crc32 deflate trees zutil blocksort huffman crctable randtable decompress bzcompress bzlib compkern"
for f in $LIB t4serv t4adler compserv compt4test; do
    icc "$f.c" -t4 -dZ_SOLO -dz_off_t=long -dBZ_NO_STDIO -o "$f.t4h" > "$f.log" 2>&1 || { cat "$f.log"; exit 1; }
    grep -E '^(Serious|Error|Fatal)' "$f.log" && exit 1
done
for m in compserv compt4test; do
    extra=; [ "$m" = compserv ] && extra="t4serv.t4h t4adler.t4h"   # the server loop (main) is t4serv.c
    { echo "$m.t4h"; [ -n "$extra" ] && printf "%s\n" $extra; for f in $LIB; do echo "$f.t4h"; done; echo '#include startup.lnk'; } > "$m.lnk"
    ilink -f "$m.lnk" -t4 -h -o "$m.c4h"
    icollect "$m.c4h" -t -o "$m.b4h"
    [ -s "$m.b4h" ] || { echo "t4compress: $m.b4h not built" >&2; exit 1; }
done
ls -l "$D"/compserv.b4h "$D"/compt4test.b4h
