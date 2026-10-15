#!/bin/bash
# build_llvm.sh — build resident transputer images using llvm-t800 (PMS tt030-ttjob-ports Phase 2).
# Produces sam-job-tt/build/jobllvm.btl (3 nodes) and sam-job-tt/build/jobllvm0.btl (SOLO).
. "$(cd "$(dirname "$0")" && git rev-parse --show-toplevel)/scripts/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
y=$root/sam-yum-tt/src
X=$root/src/x25519bench
llvm_bin=$root/tools/llvm-t800/bin
CLANG=$llvm_bin/clang
OBJCOPY=$llvm_bin/llvm-objcopy
MKBOOT=$llvm_bin/mkboot

# NFR-5 memory safety check: Root T425 <= 5M (0x80500000, VRAM from 0x80600000), TRAMs <= 4M (0x80400000)
ROOT_MEM="5M"
TRAM_MEM="4M"

# Ensure zlib/bzip2 sources in /tmp/cb are available without requiring INMOS tools
CB=/tmp/cb
if [ ! -f "$CB/compkern.c" ] || [ ! -f "$CB/bzlib.h" ]; then
    mkdir -p "$CB"
    Z=$root/tools/rpm-src/zlib-1.3.2
    B=$root/tools/rpm-src/bzip2-1.0.8
    cp "$Z"/adler32.c "$Z"/crc32.c "$Z"/deflate.c "$Z"/trees.c "$Z"/zutil.c "$Z"/inflate.c "$Z"/inftrees.c \
       "$Z"/inffast.c "$Z"/zlib.h "$Z"/zconf.h "$Z"/deflate.h "$Z"/zutil.h "$Z"/crc32.h "$Z"/trees.h \
       "$Z"/inflate.h "$Z"/inftrees.h "$Z"/inffast.h "$Z"/inffixed.h "$CB"/
    for f in blocksort huffman crctable randtable decompress bzlib; do cp "$B/$f.c" "$CB"/; done
    cp "$B/compress.c" "$CB"/bzcompress.c
    cp "$B"/bzlib.h "$B"/bzlib_private.h "$CB"/
    cp "$root"/sam-yum-tt/src/compkern.[ch] "$root"/sam-yum-tt/src/t4adler.[ch] "$CB"/
    python3 - <<'PY'
import re
s = open("/tmp/cb/zutil.h").read()
s2 = re.sub(r"#if !defined\(Z_U8\) && !defined\(Z_SOLO\) && defined\(STDC\)\n.*?#  endif\n#endif\n", "", s, flags=re.S)
if s2 != s:
    open("/tmp/cb/zutil.h", "w").write(s2)
s = open("/tmp/cb/deflate.c").read()
blk = ('#if defined(__has_feature)\n#  if __has_feature(memory_sanitizer)\n'
       '     __attribute__((no_sanitize("memory")))\n#  endif\n#endif\n')
if blk in s:
    open("/tmp/cb/deflate.c", "w").write(s.replace(blk, ""))
PY
    sed -i '/^#warning/d' "$CB"/*.h "$CB"/*.c 2>/dev/null || true
fi

w=/tmp/samjob_llvm
rm -rf "$w"
mkdir -p "$w" "$here/build"

INCS="-I$w -I$here/llvm -I$here -I$y -I$X/c25519 -I$X/fe32 -I$X/ed25519 -I$CB"
CFLAGS="-target t800 -mcpu=t425 -O2 -Wall -Wno-unused-function -Wno-unused-but-set-variable -DATARI -DZ_SOLO -Dz_off_t=long -DBZ_NO_STDIO $INCS"

# 1. Compile common kernel objects
$CLANG $CFLAGS -DJOB_COMPRESS -c "$here/llvm/jobnode.c" -o "$w/jobnode.o"
$CLANG $CFLAGS -DJOB_COMPRESS -DJOB_LHA -c "$here/llvm/jobexec.c" -o "$w/jobexec.o"

# lha (ttjob-ports FR-3, first port): the sources t4clang.sh prepares for llvm, lhaserv's hook renamed to lha_kernel
LH=/tmp/lh
[ -f "$LH/clang/lhaserv.c" ] || sh "$root/sam-yum-tt/t4clang.sh" lha "$LH" > /dev/null
LHAOBJS=
for f in lhaserv slide huf crcio maketree shuf dhuf maketbl larc; do
    $CLANG -target t800 -mcpu=t425 -O2 -w -std=gnu89 -DEUC -DSYSV_SYSTEM_DIR -DTZSET -DSUPPORT_LH6 \
        -Dt4_kernel=lha_kernel -Dt4_out_cap=lha_out_cap -I"$LH/clang" -I"$y" -c "$LH/clang/$f.c" -o "$w/lha_$f.o"
    LHAOBJS="$LHAOBJS $w/lha_$f.o"
done
$CLANG $CFLAGS -c "$here/llvm/chacha20.c" -o "$w/chacha20.o"
$CLANG $CFLAGS -c "$y/sha256.c" -o "$w/sha256.o"
$CLANG $CFLAGS -c "$y/t4adler.c" -o "$w/t4adler.o"
$CLANG $CFLAGS -c "$X/c25519/c25519.c" -o "$w/c25519.o"
$CLANG $CFLAGS -c "$X/fe32/f25519.c" -o "$w/f25519.o"
$CLANG $CFLAGS -c "$X/fe32/sha512_32.c" -o "$w/sha512_32.o"
$CLANG $CFLAGS -c "$X/ed25519/edsign.c" -o "$w/edsign.o"
$CLANG $CFLAGS -c "$X/ed25519/ed25519.c" -o "$w/ed25519.o"
$CLANG $CFLAGS -c "$X/ed25519/fprime.c" -o "$w/fprime.o"
$CLANG $CFLAGS -c "$X/ed25519/morph25519.c" -o "$w/morph25519.o"

LIB="adler32 crc32 deflate trees zutil inflate inftrees inffast blocksort huffman crctable randtable decompress bzcompress bzlib compkern"
for f in $LIB; do
    $CLANG $CFLAGS -c "$CB/$f.c" -o "$w/$f.o"
done

COMMON_OBJS="$w/jobnode.o $w/jobexec.o $w/chacha20.o $w/sha256.o $w/t4adler.o \
             $w/c25519.o $w/f25519.o $w/sha512_32.o $w/edsign.o $w/ed25519.o $w/fprime.o $w/morph25519.o \
             $w/adler32.o $w/crc32.o $w/deflate.o $w/trees.o $w/zutil.o $w/inflate.o $w/inftrees.o $w/inffast.o \
             $w/blocksort.o $w/huffman.o $w/crctable.o $w/randtable.o $w/decompress.o $w/bzcompress.o $w/bzlib.o $w/compkern.o $LHAOBJS"

# 2. Build worker2 (TRAM 2)
$CLANG $CFLAGS -c "$here/llvm/worker2.c" -o "$w/worker2.o"
$CLANG -target t800 -mcpu=t425 -mboard=atw800 -mmem=$TRAM_MEM -mhost=none "$w/worker2.o" $COMMON_OBJS -o "$w/worker2.elf"
$OBJCOPY -O binary "$w/worker2.elf" "$w/worker2.bin"
$MKBOOT "$w/worker2.bin" -o "$w/worker2.btl"

# 3. Build worker1 (TRAM 1)
$CLANG $CFLAGS -c "$here/llvm/worker1.c" -o "$w/worker1.o"
$CLANG -target t800 -mcpu=t425 -mboard=atw800 -mmem=$TRAM_MEM -mhost=none "$w/worker1.o" $COMMON_OBJS -o "$w/worker1.elf"
$OBJCOPY -O binary "$w/worker1.elf" "$w/worker1.bin"
$MKBOOT "$w/worker1.bin" -o "$w/worker1.btl"

# 4. Embed worker1 and worker2 images into C array header for Root
(cd "$w" && xxd -i worker1.btl > worker_imgs.h && xxd -i worker2.btl >> worker_imgs.h)

# 5. Build root / jobllvm.btl (3 nodes)
$CLANG $CFLAGS -c "$here/llvm/root.c" -o "$w/root.o"
$CLANG -target t800 -mcpu=t425 -mboard=atw800 -mmem=$ROOT_MEM -mhost=none "$w/root.o" $COMMON_OBJS -o "$w/jobllvm.elf"
$OBJCOPY -O binary "$w/jobllvm.elf" "$w/jobllvm.bin"
$MKBOOT "$w/jobllvm.bin" -o "$w/jobllvm.btl"
cp "$w/jobllvm.btl" "$here/build/"

# 6. Build root / jobllvm0.btl (SOLO)
$CLANG $CFLAGS -DSOLO -c "$here/llvm/root.c" -o "$w/roots.o"
$CLANG -target t800 -mcpu=t425 -mboard=atw800 -mmem=$ROOT_MEM -mhost=none "$w/roots.o" $COMMON_OBJS -o "$w/jobllvm0.elf"
$OBJCOPY -O binary "$w/jobllvm0.elf" "$w/jobllvm0.bin"
$MKBOOT "$w/jobllvm0.bin" -o "$w/jobllvm0.btl"
cp "$w/jobllvm0.btl" "$here/build/"

ls -l "$here/build/jobllvm.btl" "$here/build/jobllvm0.btl"
