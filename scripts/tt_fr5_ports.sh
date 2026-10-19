#!/bin/sh
# tt_fr5_ports: ttjob-ports FR-5 / NFR-1 / NFR-2 on the TT. Every FR-5 gate workload (inputs as llvm_fr5_gate.sh makes
# them, photo = /tmp/fr7/p800.ppm 800x600) n times through ttjobd, n times on the direct path (ttjobd stopped: the
# shipped per-port .btl), once on the 68030 (the port's /etc/t425/enabled toggle moved aside). Then tgzip.
#   sh tt_fr5_ports.sh [N]      on the TT, under nohup; output "R workload mode rc=.. t=.. md5=.. err=.."
# Restores the toggles and production ttjobd at exit.
N=${1:-3}
W=/tmp/fr5p; mkdir -p $W; cd $W || exit 1
[ -s photo.ppm ] || cp /tmp/fr7/p800.ppm photo.ppm
[ -s photo.pgm ] || ppmtopgm photo.ppm > photo.pgm
[ -s photo.jpg ] || cjpeg photo.ppm > photo.jpg
[ -s text ] || head -c 262144 /etc/termcap > text
[ -s bin ] || head -c 262144 /bin/bash > bin
[ -s random.bin ] || head -c 262144 /dev/urandom > random.bin
for f in text bin random.bin; do [ -s $f.lzh ] || lha aq $f.lzh $f; done
md5sum photo.ppm photo.pgm photo.jpg text bin random.bin text.lzh bin.lzh random.bin.lzh
WL="lha-text lha-bin lha-rand lhd-text lhd-bin lhd-rand cjpeg djpeg bc-sqrt bc-pi bc-pow zip png-rgb png-gray pq256"
tog() { case $1 in lh*) echo lha;; cjpeg|djpeg) echo libjpeg;; bc-*) echo tbc;; zip) echo zip;; png-*) echo tpnmtopng;; pq256) echo tppmquant;; esac; }
cmd() { o=$W/out.$2; case $1 in
  lha-text) rm -f $o; lha aq $o text;;  lha-bin) rm -f $o; lha aq $o bin;;  lha-rand) rm -f $o; lha aq $o random.bin;;
  lhd-text) lha pq text.lzh > $o;;  lhd-bin) lha pq bin.lzh > $o;;  lhd-rand) lha pq random.bin.lzh > $o;;
  cjpeg) cjpeg photo.ppm > $o;;  djpeg) djpeg photo.jpg > $o;;
  bc-sqrt) echo 'scale=500; sqrt(2)' | tbc > $o;;  bc-pi) echo 'scale=200; 4*a(1)' | tbc -l > $o;;  bc-pow) echo '2^20000' | tbc > $o;;
  zip) rm -f $o; zip -q -X $o text;;  png-rgb) tpnmtopng photo.ppm > $o;;  png-gray) tpnmtopng photo.pgm > $o;;
  pq256) tppmquant 256 photo.ppm > $o;; esac; }
run() { n=$1.$2; s=$(date +%s); cmd $1 $n 2>$W/err.$n; rc=$?; e=$(date +%s)
  echo "R $1 $2 rc=$rc t=$((e-s)) md5=$(md5sum < $W/out.$n | cut -c1-8) err=$(tr '\n' '|' < $W/err.$n)"; }
restore() { for t in libjpeg tpnmtopng tppmquant lha zip tbc; do [ -e /etc/t425/enabled/$t.fr5off ] && mv /etc/t425/enabled/$t.fr5off /etc/t425/enabled/$t; done
  grep -q ttjobd /tmp/t425.lock 2>/dev/null && [ -d /kern/$(cut -d" " -f1 /tmp/t425.lock) ] || { (nohup sh /etc/rc.ttjobd < /dev/null > /dev/null 2>&1 &); sleep 25; }
  echo "restored: $(cat /tmp/t425.lock) toggles: $(ls /etc/t425/enabled | tr '\n' ' ')"; echo DONE; }
trap restore EXIT
L=$(wc -l < /var/log/ttjobd.log)
i=1; while [ $i -le $N ]; do for w in $WL; do run $w ttj$i; done; i=$((i+1)); done
s=$(date +%s); tgzip -c text > $W/text.gz; echo "R tgzip ttj rc=$? t=$(( $(date +%s)-s )) rt=$(gzip -dc < $W/text.gz | md5sum | cut -c1-8)"
echo "== ttjobd jobs"; tail -n +$((L+1)) /var/log/ttjobd.log | grep -vE "job .(X|V)."
P=$(cut -d" " -f1 /tmp/t425.lock); kill -TERM $P; for k in $(seq 1 15); do [ -d /kern/$P ] || break; sleep 1; done
echo "stopped ttjobd $P"
i=1; while [ $i -le $N ]; do for w in $WL; do run $w direct$i; done; i=$((i+1)); done
for w in $WL; do t=$(tog $w); mv /etc/t425/enabled/$t /etc/t425/enabled/$t.fr5off; run $w m68k; mv /etc/t425/enabled/$t.fr5off /etc/t425/enabled/$t; done
