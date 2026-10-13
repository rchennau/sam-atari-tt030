#!/bin/bash
# run_emu_tests.sh — emulated test lane for tt030-transputer-job-queue (no TT needed; runs on fractal).
#   1. libttjob host test against a fake ttjobd (../test_libttjob.c)
#   2. NFR-2: jobtest — JOBS pseudo-random jobs per op through job_exec, host gcc build vs INMOS icc -t4 build
#      under the t4 emulator; the two digest listings must be identical.
# Exit 0 = all pass. What this lane cannot cover (real link driver, FreeMiNT, TRAM no-FPU, timings): plan, Eng §5.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd); job=$(cd "$here/.." && pwd); root=$(cd "$job/.." && pwd); y=$root/sam-yum-tt/src
JOBS=${JOBS:-1000}
CB=/tmp/cb; "$root/sam-yum-tt/t4compress.sh" "$CB" > /dev/null
ZLIB="adler32 crc32 deflate trees zutil inflate inftrees inffast blocksort huffman crctable randtable decompress bzcompress bzlib compkern"
w=/tmp/samemu; rm -rf "$w"; mkdir -p "$w"
# 1. libttjob
cc -Wall -I"$job" -I"$y" "$job/test_libttjob.c" "$job/libttjob.c" "$y/t4adler.c" -o "$w/test_libttjob"
"$w/test_libttjob"
# 2a. host reference (-DATARI: gzip OS byte 05, as the T425 image and the 68030 build)
cp "$here/jobtest.c" "$job/t4/jobexec.c" "$job/t4/jobexec.h" "$job"/t4/chacha20.[ch] "$job/jobwire.h" "$y"/sha256.[ch] "$w/"
# crypto (Phase 3): xserv2's c25519 + 32-bit field maths + edsign + 32-bit SHA-512 (src/x25519bench)
X=$root/src/x25519bench
cp "$X"/c25519/c25519.[ch] "$X"/c25519/f25519.h "$X"/fe32/f25519.c "$X"/fe32/sha512_32.c "$X"/fe32/sha512.h \
   "$X"/ed25519/edsign.[ch] "$X"/ed25519/ed25519.[ch] "$X"/ed25519/fprime.[ch] "$X"/ed25519/morph25519.[ch] "$w/"
CRYPTO="c25519 f25519 sha512_32 edsign ed25519 fprime morph25519 chacha20"
sed -i 's|"../jobwire.h"|"jobwire.h"|' "$w/jobexec.c"
srcs=; for f in $ZLIB; do srcs="$srcs $CB/$f.c"; done
cc -O2 -w -DJOBS="$JOBS" -DJOB_COMPRESS -DATARI -DZ_SOLO -Dz_off_t=long -DBZ_NO_STDIO -I"$w" -I"$CB" \
   -DFORCE_EMU64 -DCJOBS="${CJOBS:-50}" \
   -o "$w/jobtest.host" "$w/jobtest.c" "$w/jobexec.c" "$w/sha256.c" $(for f in $CRYPTO; do echo "$w/$f.c"; done) $srcs
"$w/jobtest.host" > "$w/host.out"
# 2b. T425 build under t4 (8 MB: -sm 23, see sam-yum-tt friction F9)
cd "$w"; mkdir -p icc && cp "$X/icc/stdint.h" icc/
. "$root/tools/d72uni-env.sh"; export ISEARCH="$w/ $w/icc/ $CB/ $D72UNI/libs/"
cc4() { icc "$@" > icc.log 2>&1 || { cat icc.log; exit 1; }; ! grep -E '^(Serious|Error|Fatal)' icc.log; }
cp "$CB/deflate.c" deflate5.c
cc4 deflate5.c -t4 -dATARI -dZ_SOLO -dz_off_t=long -dBZ_NO_STDIO -o deflate5.t4h
cc4 jobtest.c -t4 -dJOBS="$JOBS" -dCJOBS="${CJOBS:-50}" -o jobtest.t4h
for f in $CRYPTO; do cc4 "$f.c" -t4 -o "$f.t4h"; done
# EMU_MUTANT=1: corrupt the T425 build only (SHA-256 output when n % 7 == 3) to prove the diff catches a platform
# difference — the run must then FAIL.
[ "${EMU_MUTANT:-0}" = 1 ] && sed -i 's/sha256_final(&s, out);/sha256_final(\&s, out); out[0] ^= (n % 7 == 3);/' jobexec.c
cc4 jobexec.c -t4 -dJOB_COMPRESS -o jobexec.t4h
cc4 sha256.c -t4 -o sha256.t4h
{ echo jobtest.t4h; echo jobexec.t4h; echo sha256.t4h; for f in $CRYPTO; do echo "$f.t4h"; done; for f in $ZLIB; do [ "$f" = deflate ] && echo deflate5.t4h || echo "$CB/$f.t4h"; done; echo '#include startup.lnk'; } > jobtest.lnk
ilink -f jobtest.lnk -t4 -h -o jobtest.c4h && icollect jobtest.c4h -t -o jobtest.b4h
t4 -sm 23 -sb jobtest.b4h | tr -d '\r' | grep -E '^(op|kat) ' > t4.out
if diff -u host.out t4.out; then
    echo "emu: jobtest identical on host and t4 ($JOBS jobs per op)"; cat host.out
else
    echo "emu: jobtest DIFFERS between host and t4" >&2; exit 1
fi
! grep -E 'FAIL' host.out && ! grep '^op ' host.out | grep -v ' failed 0 ' || { echo "emu: some jobs failed (see 'failed' counts above)" >&2; exit 1; }
