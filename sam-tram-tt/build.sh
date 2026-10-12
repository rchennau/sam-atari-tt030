#!/bin/bash
# build.sh — build the ATW800/2 TRAM probes on fractal. Outputs build/{linkprobe,tramprobe,tramchar,tramfpu}.btl.
# The t4-hosted INMOS tools fail under long paths, so the build runs in a short /tmp dir.
# Run on the TT (no `kill -9` on a hung iserver — it crashed XaAES; use scripts/tt_reboot.sh --wait-back):
#   TRANSPUTER=1 IBOARDSIZE='#200000' /c/ATW800_2/atwtools/iserver.ttp -sb tramprobe.btl
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/.." && pwd)
w=/tmp/samtram-build; rm -rf "$w"; mkdir -p "$w" "$here/build"; cp "$here"/*.c "$here"/*.h "$here"/*.cfs "$w"/
cd "$w"; . "$root/tools/d72uni-env.sh"; export ISEARCH="$PWD/ $D72UNI/libs/"
icc linkprobe.c -t4 -o linkprobe.t4h
printf 'linkprobe.t4h\n#include startup.lnk\n' > linkprobe.lnk
ilink -f linkprobe.lnk -t4 -h -o linkprobe.c4h && icollect linkprobe.c4h -t -o linkprobe.btl
icc rootp.c -ta -o rootp.tco && ilink rootp.tco -o rootp.lku -f startup.lnk -ta
icc tramp.c -ta -o tramp.tco && ilink tramp.tco -o tramp.lku -f startrd.lnk -ta
icconf tramprobe.cfs && icollect tramprobe.cfb -o tramprobe.btl
icc rootc.c -ta -o rootc.tco && ilink rootc.tco -o rootc.lku -f startup.lnk -ta
icc tramc.c -ta -o tramc.tco && ilink tramc.tco -o tramc.lku -f startrd.lnk -ta
icconf tramchar.cfs && icollect tramchar.cfb -o tramchar.btl
icc rootf.c -ta -o rootf.tco && ilink rootf.tco -o rootf.lku -f startup.lnk -ta
icc tramf.c -t8 -o tramf.tco && ilink tramf.tco -o tramf.lku -f startrd.lnk -t8
sed "s/tramf.lku/tramfs.lku/" tramfpu.cfs > tramfps.cfs
icc tramf.c -ta -o tramfs.tco && ilink tramfs.tco -o tramfs.lku -f startrd.lnk -ta
icconf tramfps.cfs && icollect tramfps.cfb -o tramfps.btl
icconf tramfpu.cfs && icollect tramfpu.cfb -o tramfpu.btl
cp linkprobe.btl tramprobe.btl tramchar.btl tramfpu.btl tramfps.btl "$here/build/"
ls -l "$here/build"
