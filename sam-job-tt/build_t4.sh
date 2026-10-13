#!/bin/bash
# build_t4.sh — build the resident transputer image (tt030-transputer-job-queue FR-1) into build/jobfull.btl.
# INMOS tools run under t4 and fail under long paths, so the build happens in /tmp/samjob.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/.." && pwd); y=$root/sam-yum-tt/src
w=/tmp/samjob; rm -rf "$w"; mkdir -p "$w" "$here/build"
cp "$here"/t4/*.c "$here"/t4/*.h "$here"/t4/*.cfs "$here"/jobwire.h "$y"/sha256.[ch] "$y"/t4adler.[ch] "$w"/
mkdir -p "$w/inc" && cp "$here/jobwire.h" "$w/inc/" && sed -i 's|"../jobwire.h"|"jobwire.h"|' "$w"/*.c
cd "$w"; . "$root/tools/d72uni-env.sh"; export ISEARCH="$PWD/ $D72UNI/libs/"
for f in jobnode sha256 t4adler root worker1 worker2; do icc "$f.c" -ta -o "$f.tco" > "$f.log" 2>&1 || { cat "$f.log"; exit 1; }; grep -E 'Error|Serious' "$f.log" && exit 1 || true; done
for m in root worker1 worker2; do ilink "$m.tco" jobnode.tco sha256.tco t4adler.tco -o "$m.lku" -f startrd.lnk -ta; done
icconf jobfull.cfs && icollect jobfull.cfb -o jobfull.btl
cp jobfull.btl "$here/build/"; ls -l "$here/build/jobfull.btl"
