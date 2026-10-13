#!/bin/bash
# build_t4.sh — build the resident transputer image (tt030-transputer-job-queue FR-1) into build/jobfull.btl.
# T425 node: jobwire + SHA-256 + compression kernels (compkern, built -t4 from sam-yum-tt/t4compress.sh's patched
# zlib/bzip2 in /tmp/cb). TRAM workers: jobwire + SHA-256, -ta (no FPU). INMOS tools run under t4 and fail under
# long paths, so everything builds in short /tmp dirs.
. "$(cd "$(dirname "$0")" && git rev-parse --show-toplevel)/scripts/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/.." && pwd); y=$root/sam-yum-tt/src
CB=/tmp/cb; "$root/sam-yum-tt/t4compress.sh" "$CB" > /dev/null
LIB="adler32 crc32 deflate trees zutil inflate inftrees inffast blocksort huffman crctable randtable decompress bzcompress bzlib compkern"
w=/tmp/samjob; rm -rf "$w"; mkdir -p "$w" "$here/build"
cp "$here"/t4/*.c "$here"/t4/*.h "$here"/t4/*.cfs "$here"/jobwire.h "$y"/sha256.[ch] "$y"/t4adler.[ch] "$w"/
# crypto (Phase 3): xserv2's c25519 + 32-bit field maths + edsign + 32-bit SHA-512, and the icc stdint shim
X=$root/src/x25519bench
cp "$X"/c25519/c25519.[ch] "$X"/c25519/f25519.h "$X"/fe32/f25519.c "$X"/fe32/sha512_32.c "$X"/fe32/sha512.h \
   "$X"/ed25519/edsign.[ch] "$X"/ed25519/ed25519.[ch] "$X"/ed25519/fprime.[ch] "$X"/ed25519/morph25519.[ch] "$w"/
mkdir -p "$w/icc" && cp "$X/icc/stdint.h" "$w/icc/"
CRYPTO="c25519 f25519 sha512_32 edsign ed25519 fprime morph25519 chacha20"
sed -i 's|"../jobwire.h"|"jobwire.h"|' "$w"/*.c
cd "$w"; . "$root/tools/d72uni-env.sh"; export ISEARCH="$PWD/ $PWD/icc/ $CB/ $D72UNI/libs/"
cc() { icc "$1" "${@:2}" > "$1.log" 2>&1 || { cat "$1.log"; exit 1; }; ! grep -E '^(Serious|Error|Fatal)' "$1.log"; }
# TRAM workers (-ta)
for f in jobnode sha256 t4adler worker1 worker2 $CRYPTO; do cc "$f.c" -ta -o "$f.tco"; done
# Phase 4: the TRAMs carry the compression kernels too (NODE_ANY jobs may land on them): compkern + zlib/bzip2 -ta
cc jobexec.c -ta -dJOB_COMPRESS -o jobexec.tco
cp "$CB/deflate.c" deflate5a.c; cc deflate5a.c -ta -dATARI -dZ_SOLO -dz_off_t=long -dBZ_NO_STDIO -o deflate5a.tco
ZTA=; for f in $LIB; do [ "$f" = deflate ] && { ZTA="$ZTA deflate5a.tco"; continue; }; cp "$CB/$f.c" "${f}a.c"; cc "${f}a.c" -ta -dZ_SOLO -dz_off_t=long -dBZ_NO_STDIO -o "${f}a.tco"; ZTA="$ZTA ${f}a.tco"; done
for m in worker1 worker2; do
    { echo "$m.tco"; for f in jobnode jobexec sha256 t4adler $CRYPTO; do echo "$f.tco"; done; for f in $ZTA; do echo "$f"; done; echo '#include startrd.lnk'; } > "$m.lnk"
    ilink -f "$m.lnk" -ta -o "$m.lku"
done
# T425 root (-t4, with compression)
cp jobnode.c jobnodec.c; cp jobexec.c jobexecc.c; for f in $CRYPTO; do cp "$f.c" "${f}4.c"; cc "${f}4.c" -t4 -o "${f}4.t4h"; done; cp sha256.c sha2564.c; cp t4adler.c t4adler4.c
# zlib picks the gzip OS byte from compile-time defines: the 68030 build writes 05 (ATARI), the T425 build 03.
# Rebuild deflate with -dATARI so the T425's gzip output is byte-identical to the 68030's (Phase 2 finding).
cp "$CB/deflate.c" deflate5.c; cc deflate5.c -t4 -dATARI -dZ_SOLO -dz_off_t=long -dBZ_NO_STDIO -o deflate5.t4h
cc root.c -t4 -o root.t4h; cc jobnodec.c -t4 -o jobnodec.t4h; cc jobexecc.c -t4 -dJOB_COMPRESS -o jobexecc.t4h; cc sha2564.c -t4 -o sha2564.t4h; cc t4adler4.c -t4 -o t4adler4.t4h
{ echo root.t4h; echo jobnodec.t4h; echo jobexecc.t4h; for f in $CRYPTO; do echo "${f}4.t4h"; done; echo sha2564.t4h; echo t4adler4.t4h; for f in $LIB; do [ "$f" = deflate ] && echo deflate5.t4h || echo "$CB/$f.t4h"; done; echo '#include startrd.lnk'; } > root.lnk
ilink -f root.lnk -t4 -o root.lku
icconf jobfull.cfs && icollect jobfull.cfb -o jobfull.btl
cp jobfull.btl "$here/build/"; ls -l "$here/build/jobfull.btl"
