#!/bin/bash
# jobtest_llvm.sh — NFR-2 correctness lane for llvm-t800 job queue.
# Runs JOBS pseudo-random jobs per op through job_exec:
#   1. Host reference built with host gcc
#   2. Transputer image built with clang under the t4 emulator (t800-run, T800_T4_FLAGS='-sm 23')
# Both digest listings must be identical. Exit 0 = pass.
. "$(cd "$(dirname "$0")" && git rev-parse --show-toplevel)/scripts/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
job=$(cd "$here/.." && pwd)
root=$(cd "$job/.." && pwd)
y=$root/sam-yum-tt/src
X=$root/src/x25519bench
CB=/tmp/cb

JOBS=${JOBS:-1000}
CJOBS=${CJOBS:-50}
export T800_T4_TIMEOUT=${T800_T4_TIMEOUT:-600}

# Ensure /tmp/cb is populated without requiring INMOS tools
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

w=/tmp/samjob_llvm_test
rm -rf "$w"
mkdir -p "$w"

cp "$job/emu/jobtest.c" "$job/llvm/jobexec.c" "$job/llvm/jobexec.h" "$job/llvm/chacha20."[ch] "$job/jobwire.h" "$y"/sha256.[ch] "$w/"
cp "$X"/c25519/c25519.[ch] "$X"/c25519/f25519.h "$X"/fe32/f25519.c "$X"/fe32/sha512_32.c "$X"/fe32/sha512.h \
   "$X"/ed25519/edsign.[ch] "$X"/ed25519/ed25519.[ch] "$X"/ed25519/fprime.[ch] "$X"/ed25519/morph25519.[ch] "$w/"
sed -i 's|"../jobwire.h"|"jobwire.h"|' "$w/jobexec.c"

CRYPTO="c25519 f25519 sha512_32 edsign ed25519 fprime morph25519 chacha20"
ZLIB="adler32 crc32 deflate trees zutil inflate inftrees inffast blocksort huffman crctable randtable decompress bzcompress bzlib compkern"
srcs=; for f in $ZLIB; do srcs="$srcs $CB/$f.c"; done

# 1. Host reference (gcc)
gcc -O2 -w -DJOBS="$JOBS" -DJOB_COMPRESS -DATARI -DZ_SOLO -Dz_off_t=long -DBZ_NO_STDIO \
    -I"$w" -I"$CB" -DFORCE_EMU64 -DCJOBS="$CJOBS" \
    -o "$w/jobtest.host" "$w/jobtest.c" "$w/jobexec.c" "$w/sha256.c" $(for f in $CRYPTO; do echo "$w/$f.c"; done) $srcs
"$w/jobtest.host" > "$w/host.out"

# 2. Clang under t4 emulator via t800-run
T800_T4_FLAGS='-sm 23' "$root/tools/llvm-t800/bin/t800-run" -mcpu=t425 -ext -O2 -lc -w \
    --defsym=__ram_end=0x80500000 --defsym=__stack_headroom=0x20000 \
    -DJOBS="$JOBS" -DCJOBS="$CJOBS" -DJOB_COMPRESS -DATARI -DZ_SOLO -Dz_off_t=long -DBZ_NO_STDIO \
    -I"$w" -I"$CB" \
    "$w/jobtest.c" "$w/jobexec.c" "$w/sha256.c" $(for f in $CRYPTO; do echo "$w/$f.c"; done) $srcs \
    -o "$w/jobtest_t4" | tr -d '\r' | grep -E '^(op|kat) ' > "$w/t4.out"

if diff -u "$w/host.out" "$w/t4.out"; then
    echo "emu: jobtest identical on host and t4-llvm ($JOBS jobs per op)"
    cat "$w/host.out"
else
    echo "emu: jobtest DIFFERS between host and t4-llvm" >&2
    exit 1
fi

! grep -E 'FAIL' "$w/host.out" && ! grep '^op ' "$w/host.out" | grep -v ' failed 0 ' || { echo "emu: some jobs failed (see 'failed' counts above)" >&2; exit 1; }
