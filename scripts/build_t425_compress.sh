#!/bin/sh
# Package tgzip / tbzip2 (gzip + bzip2 on the ATW800/2 T425, 68030 fallback; tt030-rpm-pipeline Rev. 7)
# as build/t425-compress-<ver>-1.m68kmint.rpm. Contents: /usr/bin/tgzip, /usr/bin/tbzip2 -> tgzip,
# /usr/lib/t425/compserv.btl (deflate, INMOS toolset via sam-yum-tt/t4compress.sh) and bzserv.btl (bzip2, llvm-t800).
set -eu
cd "$(dirname "$0")/.."
V=${1:-1.0}
CB=/tmp/cb
sam-yum-tt/t4compress.sh "$CB" > /dev/null
# bzserv.btl: the same kernels through llvm-t800 (scripts/setup_llvm_t800.sh), used by tbzip2 only. Unpatched
# zlib/bzip2 (the icc patches in t4compress.sh are not needed by clang) — the build measured on the TT.
LB=$CB/llvm && rm -rf "$LB" && mkdir -p "$LB"
Z=tools/rpm-src/zlib-1.3.2 B=tools/rpm-src/bzip2-1.0.8 S=sam-yum-tt/src
cp "$Z"/*.c "$Z"/*.h "$B"/*.h "$S"/compkern.[ch] "$S"/compserv.c "$S"/t4serv.[ch] "$S"/t4adler.[ch] "$LB"/
for f in blocksort huffman crctable randtable decompress bzlib; do cp "$B/$f.c" "$LB"/; done
cp "$B"/compress.c "$LB"/bzcompress.c
(cd "$LB" && PATH=$OLDPWD/tools/llvm-t800/bin:$PATH t800-cc -mcpu=t425 -mboard=atw800 -mmem=5M -mhost=none -O2 -w \
    -DZ_SOLO -DBZ_NO_STDIO -I. -o bzserv compserv.c t4serv.c t4adler.c adler32.c crc32.c deflate.c trees.c \
    zutil.c inflate.c inftrees.c inffast.c blocksort.c huffman.c crctable.c randtable.c decompress.c \
    bzcompress.c bzlib.c compkern.c)   # this order = the image timed on the TT (link layout)
tools/llvm-t800/bin/llvm-nm "$LB/bzserv.elf" | grep -q '80500000 A __ram_end' \
    || { echo "bzserv: not confined to 5 MB (NFR-2)" >&2; exit 1; }
PATH=$PWD/tools/cross-mint/usr/bin:$PATH m68k-atari-mint-gcc -m68020-60 -O2 -s -w \
    -DZ_SOLO -Dz_off_t=long -DBZ_NO_STDIO -I"$CB" -Isam-yum-tt/src -o build/tgzip \
    sam-yum-tt/src/tgzip.c sam-yum-tt/src/t4call.c sam-yum-tt/src/t4lock.c sam-yum-tt/src/t4adler.c sam-yum-tt/src/atwboot.c "$CB"/compkern.c "$CB"/adler32.c "$CB"/crc32.c \
    "$CB"/deflate.c "$CB"/trees.c "$CB"/zutil.c "$CB"/inflate.c "$CB"/inftrees.c "$CB"/inffast.c "$CB"/blocksort.c "$CB"/huffman.c "$CB"/crctable.c \
    "$CB"/randtable.c "$CB"/decompress.c "$CB"/bzcompress.c "$CB"/bzlib.c
python3 - "$V" "$CB" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
v, cb = sys.argv[1], sys.argv[2]
out = f"build/t425-compress-{v}-1.m68kmint.rpm"
open(out, "wb").write(R.write_rpm("t425-compress", v, "1", [
    ("/usr/bin/tgzip", 0o100755, open("build/tgzip", "rb").read()),
    ("/usr/bin/tbzip2", 0o160777, b"tgzip"),
    ("/usr/lib/t425/compserv.btl", 0o100644, open(f"{cb}/compserv.b4h", "rb").read()),
    ("/usr/lib/t425/bzserv.btl", 0o100644, open(f"{cb}/llvm/bzserv.btl", "rb").read()),
], summary="gzip/bzip2 compression on the ATW800/2 T425, 68030 fallback (sam)", requires_=[],
   description="compserv.btl (deflate) is built with the INMOS toolset; bzserv.btl (bzip2) with llvm-t800, "
               "whose runtime is Apache-2.0 WITH LLVM-exception (llvm-t800 LICENSE)."))
print(out)
PY
