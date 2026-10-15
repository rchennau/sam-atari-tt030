#!/bin/bash
# llvm_fr5_gate.sh — PMS tt030-llvm-t800-adoption FR-5 on the real TT for lha, libjpeg, ppmquant, pnmtopng.
#   llvm_fr5_gate.sh [ROUNDS [REGEX]]     run from the dir holding t4pilot.ttp, the clang *-O2/-Os.btl, photo.ppm.gz, random.bin
# Every round times the shipped icc image (/usr/lib/t425) and the two clang candidates (sam-yum-tt/t4clang.sh) on each
# workload with t4pilot (boot and call apart; the call is the gated quantity), build order reversed on even rounds.
# Run it from the serial console with ttjobd stopped (ttjobd owns the T425) and no SSH login during it (NFR-4).
# Output: "round workload build boot_ms call_ms outlen adler32"; the adler32 must match across builds (NFR-1).
set -u
cd "$(dirname "$0")"
R=${1:-3} P=./t4pilot.ttp L=/usr/lib/t425
[ -s photo.ppm ] || gunzip -c photo.ppm.gz > photo.ppm
[ -s photo.pgm ] || ppmtopgm photo.ppm > photo.pgm
[ -s photo.jpg ] || cjpeg photo.ppm > photo.jpg
[ -s text ] || head -c 262144 /etc/termcap > text
[ -s bin ] || head -c 262144 /bin/bash > bin
[ -s pq.in ] || { printf '\x00\x01\x00\x00'; cat photo.ppm; } > pq.in               # 256 colours, LE
if [ ! -s dec.in ]; then                                                            # lha decode: size (LE) || packed
    $P $L/lha.btl 5 5 text enc.out > /dev/null || exit 1
    { printf '\x00\x00\x04\x00'; tail -c +3 enc.out; } > dec.in
fi
img() {
    case $1 in lha) s=lhaserv i=lha ;; jpeg) s=jpegserv i=libjpeg ;; ppmq) s=ppmqserv i=tppmquant ;; png) s=pngserv i=tpnmtopng ;; esac
    if [ "$2" = icc ]; then echo $L/$i.btl; else echo ./$s-$2.btl; fi
}
echo "# fr5 gate $(date) rounds=$R"
md5sum $L/lha.btl $L/libjpeg.btl $L/tppmquant.btl $L/tpnmtopng.btl ./*-O2.btl ./*-Os.btl text bin random.bin photo.ppm photo.pgm photo.jpg pq.in dec.in
W="lha-text lha 5 5 text
lha-bin lha 5 5 bin
lha-rand lha 5 5 random.bin
lha-dec lha d 5 dec.in
jpeg-C jpeg C 0 photo.ppm
jpeg-D jpeg D 0 photo.jpg
ppmq-256 ppmq q 0 pq.in
png-rgb png p 6 photo.ppm
png-gray png p 6 photo.pgm"
W=$(echo "$W" | grep -E "${2:-.}")                                              # e.g. ^lha for a smoke round
r=1
while [ $r -le "$R" ]; do
    if [ $((r % 2)) = 1 ]; then B="icc O2 Os"; else B="Os O2 icc"; fi
    echo "$W" | while read -r name port op lvl in; do
        for b in $B; do echo "$r $name $b $($P "$(img $port $b)" $op $lvl $in 2>&1)"; done
    done
    r=$((r + 1))
done
echo "# fr5 gate done $(date)"
