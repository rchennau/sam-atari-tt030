#!/bin/bash
# build.sh — build the ATW800/2 TRAM probes on the remote system management host. Outputs build/*.btl.
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
icc root2.c -ta -o root2.tco && ilink root2.tco -o root2.lku -f startup.lnk -ta
icc relay.c -ta -o relay.tco && ilink relay.tco -o relay.lku -f startrd.lnk -ta
icc t2.c -ta -o t2a.tco && ilink t2a.tco -o t2a.lku -f startrd.lnk -ta
icc t2.c -t8 -o t28.tco && ilink t28.tco -o t28.lku -f startrd.lnk -t8
for v in a 8; do sed "s/T2LKU/t2$v.lku/" tram2.cfs > tram2$v.cfs; icconf tram2$v.cfs && icollect tram2$v.cfb -o tram2$v.btl; done
for m in rootb:startup echo1:startrd echo2:startrd; do icc ${m%%:*}.c -ta -o ${m%%:*}.tco && ilink ${m%%:*}.tco -o ${m%%:*}.lku -f ${m##*:}.lnk -ta; done
icconf tramlink.cfs && icollect tramlink.cfb -o tramlink.btl
cp tramlink.btl linkprobe.btl tramprobe.btl tramchar.btl tramfpu.btl tramfps.btl tram2a.btl tram28.btl "$here/build/"
ls -l "$here/build"
