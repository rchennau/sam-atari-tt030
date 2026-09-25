#!/bin/sh
# Package tgzip / tbzip2 (gzip + bzip2 on the ATW800/2 T425, 68030 fallback; tt030-rpm-pipeline Rev. 7)
# as build/t425-compress-<ver>-1.m68kmint.rpm. Contents: /usr/bin/tgzip, /usr/bin/tbzip2 -> tgzip,
# /usr/lib/t425/compserv.btl. The T425 image comes from sam-yum-tt/t4compress.sh (INMOS toolset).
set -eu
cd "$(dirname "$0")/.."
V=${1:-1.0}
CB=/tmp/cb
sam-yum-tt/t4compress.sh "$CB" > /dev/null
PATH=$PWD/tools/cross-mint/usr/bin:$PATH m68k-atari-mint-gcc -m68020-60 -O2 -s -w \
    -DZ_SOLO -Dz_off_t=long -DBZ_NO_STDIO -I"$CB" -Isam-yum-tt/src -o build/tgzip \
    sam-yum-tt/src/tgzip.c sam-yum-tt/src/t4call.c sam-yum-tt/src/t4lock.c sam-yum-tt/src/t4adler.c sam-yum-tt/src/atwboot.c "$CB"/compkern.c "$CB"/adler32.c "$CB"/crc32.c \
    "$CB"/deflate.c "$CB"/trees.c "$CB"/zutil.c "$CB"/blocksort.c "$CB"/huffman.c "$CB"/crctable.c \
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
], summary="gzip/bzip2 compression on the ATW800/2 T425, 68030 fallback (sam)", requires_=[]))
print(out)
PY
