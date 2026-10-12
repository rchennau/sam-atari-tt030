#!/bin/bash
# build.sh — build the ATW800/2 TRAM probes on fractal. Outputs build/linkprobe.btl and build/tramprobe.btl.
# The t4-hosted INMOS tools fail under long paths, so the build runs in a short /tmp dir.
# Run on the TT (no `kill -9` on a hung iserver — it crashed XaAES; use scripts/tt_reboot.sh --wait-back):
#   TRANSPUTER=1 IBOARDSIZE='#200000' /c/ATW800_2/atwtools/iserver.ttp -sb tramprobe.btl
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/.." && pwd)
w=/tmp/samtram-build; rm -rf "$w"; mkdir -p "$w" "$here/build"; cp "$here"/*.c "$here"/*.cfs "$w"/
cd "$w"; . "$root/tools/d72uni-env.sh"; export ISEARCH="$PWD/ $D72UNI/libs/"
icc linkprobe.c -t4 -o linkprobe.t4h
printf 'linkprobe.t4h\n#include startup.lnk\n' > linkprobe.lnk
ilink -f linkprobe.lnk -t4 -h -o linkprobe.c4h && icollect linkprobe.c4h -t -o linkprobe.btl
icc rootp.c -ta -o rootp.tco && ilink rootp.tco -o rootp.lku -f startup.lnk -ta
icc tramp.c -ta -o tramp.tco && ilink tramp.tco -o tramp.lku -f startrd.lnk -ta
icconf tramprobe.cfs && icollect tramprobe.cfb -o tramprobe.btl
cp linkprobe.btl tramprobe.btl "$here/build/"
ls -l "$here/build"
