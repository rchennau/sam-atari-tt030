#!/bin/bash
# llvm_fr5_fe_gate.sh — PMS tt030-llvm-t800-adoption FR-5 for cjpeg, tppmquant, tpnmtopng through their real front ends
# (the 1.44 MB cases t4pilot's generic path cannot carry; cjpeg sends raw rows + a 9-byte header, not the PPM).
#   llvm_fr5_fe_gate.sh [ROUNDS]    run from the dir holding the clang *-O2/-Os.btl and photo.ppm (llvm_fr5_gate.sh's dir)
# Each round swaps the shipped icc image and the two clang candidates into /usr/lib/t425/<name>.btl and times the whole
# command (68030 front end + T425 call), build order reversed on even rounds. A run whose stderr says "68030" fell back
# and is marked FALLBACK. The shipped images are restored at exit and checked by md5 (rpm -V clean again).
# Run from the serial console with ttjobd stopped (the front ends take the T425 lock themselves) and no SSH.
# Output: "round workload build seconds md5 [FALLBACK]".
set -u
cd "$(dirname "$0")"
R=${1:-3} L=/usr/lib/t425
for i in libjpeg tppmquant tpnmtopng; do [ -s $i.icc ] || cp $L/$i.btl $i.icc; done
restore() {
    for i in libjpeg tppmquant tpnmtopng; do cp $i.icc $L/$i.btl; done
    md5sum $L/libjpeg.btl $L/tppmquant.btl $L/tpnmtopng.btl | sed 's/^/# restored /'
}
trap restore EXIT
img() {
    case $1 in libjpeg) s=jpegserv ;; tppmquant) s=ppmqserv ;; tpnmtopng) s=pngserv ;; esac
    if [ "$2" = icc ]; then echo $1.icc; else echo $s-$2.btl; fi
}
run() {   # workload build image command...
    local wl=$1 b=$2 im=$3 t; shift 3
    cp "$(img $im $b)" $L/$im.btl
    TIMEFORMAT=%R
    t=$( { time "$@" > fe.out 2> fe.err; } 2>&1 )
    echo "$r $wl $b $t $(md5sum < fe.out | cut -c1-32)$(grep -q 68030 fe.err && echo ' FALLBACK')"
}
echo "# fr5 front-end gate $(date) rounds=$R"
md5sum ./*.icc ./*-O2.btl ./*-Os.btl photo.ppm
r=1
while [ $r -le "$R" ]; do
    if [ $((r % 2)) = 1 ]; then B="icc O2 Os"; else B="Os O2 icc"; fi
    for b in $B; do run cjpeg $b libjpeg cjpeg photo.ppm; done
    for b in $B; do run tppmquant-256 $b tppmquant tppmquant 256 photo.ppm; done
    for b in $B; do run tpnmtopng $b tpnmtopng tpnmtopng photo.ppm; done
    r=$((r + 1))
done
echo "# fr5 front-end gate done $(date)"
