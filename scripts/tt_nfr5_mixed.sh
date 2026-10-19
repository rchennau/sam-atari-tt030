#!/bin/bash
# tt_nfr5_mixed: ttjob-ports NFR-5, 200 interleaved jobs (6 small ops + a large ppmquant every 25th) on the test ttjobd;
# md5 per op must stay identical, then malformed input per op. ALWAYS restores production. Outputs are *.lzh: lha wants that name.
[ -e /tmp/nfr5.pid ] && kill -0 $(cat /tmp/nfr5.pid) 2>/dev/null && { echo "nfr5 already running" >&2; exit 1; }
echo $$ > /tmp/nfr5.pid
L=/tmp/nfr5.log; exec > $L 2>&1; cd /tmp/fr7
restore() { P=$(cut -d" " -f1 /tmp/t425.lock); case "$(cat /kern/$P/fname 2>/dev/null)" in *ttjobd-fr7*) kill -TERM $P; for i in $(seq 1 15); do [ -d /kern/$P ] || break; sleep 1; done;; esac
  (nohup bash /etc/rc.ttjobd < /dev/null > /dev/null 2>&1 &); sleep 25; echo "restored: $(cat /tmp/t425.lock)"; tail -1 /var/log/ttjobd.log; echo DONE; }
trap restore EXIT
P0=$(cut -d" " -f1 /tmp/t425.lock); kill -TERM $P0; for i in $(seq 1 15); do [ -d /kern/$P0 ] || break; sleep 1; done
(nohup /tmp/fr7/ttjobd-fr7.ttp /tmp/fr7/jobllvm.btl /var/run/ttjob.sock < /dev/null > /tmp/fr7/tj.log 2>&1 &); sleep 25; cat /tmp/fr7/tj.log
k() { echo "K after $1: $(./ttjobk.ttp 2>&1)"; }
t() { local tag=$1; shift; local s=$(date +%s); sh -c "$*" 2> $tag.err; local rc=$?; echo "$tag rc=$rc $(( $(date +%s) - s )) s $(tr '\n' ' ' < $tag.err | cut -c1-120)"; return $rc; }
k boot
# small inputs for the table
head -c 40000 big.txt > s.txt
./Xdjpeg -scale 1/4 r1.jpg > s.ppm
# job table: 6 small ops plus big. The TT's bash is 2.05a (no declare -A): per-op state lives in ref_<op>/cnt_<op>.
small=(cj dj lha zip png pq)
cmd() { case $1 in
  cj) echo './Xcjpeg -quality 85 s.ppm > $o';;   dj) echo './Xdjpeg r1.jpg > $o';;
  lha) echo 'rm -f $o; ./Xlha aq $o s.txt';;     zip) echo 'rm -f $o; ./Xzip -q -X $o s.txt';;
  png) echo './Xtpnmtopng s.ppm > $o';;          pq) echo './Xtppmquant 64 s.ppm > $o';;
  big) echo './Xtppmquant 256 t352.ppm > $o';; esac; }
for op in cj dj lha zip png pq big; do eval "cnt_$op=0; ref_$op="; done
ref_big=$(md5sum < r7.ppm | cut -d' ' -f1)      # r7.ppm: the known-good output of the big command
fail=0
N=${N:-200}
for i in $(seq 1 $N); do
  o=n$i.lzh
  if [ $(( i % 25 )) -eq 0 ]; then op=big; else op=${small[$(( (i*5+3) % 6 ))]}; fi
  t "n$i.$op" "o=$o; $(cmd $op)"; rc=$?
  eval "cnt_$op=\$(( cnt_$op + 1 ))"
  h=$(md5sum < $o | cut -d' ' -f1)
  eval "r=\$ref_$op"
  bad=0
  [ $rc -ne 0 ] || [ -z "$h" ] && bad=1
  if [ -z "$r" ]; then eval "ref_$op=$h"; elif [ "$h" != "$r" ]; then bad=1; fi
  if [ $bad -ne 0 ]; then fail=$((fail+1)); echo "FAILED n$i op=$op rc=$rc md5=$h"; fi
  rm -f $o
  if [ $(( i % 20 )) -eq 0 ]; then k "job $i"; fi
done
# malformed input, once per small op
head -c 1000 s.ppm > bad.ppm; head -c 500 r1.jpg > bad.jpg; head -c 100 s.txt > bad.txt
o=bad.lzh
for op in cj dj lha zip png pq; do
  case $op in
    cj) c='./Xcjpeg -quality 85 bad.ppm > $o';;
    dj) c='./Xdjpeg bad.jpg > $o';;
    lha) c='rm -f $o; ./Xlha aq $o bad.txt';;
    zip) c='rm -f $o; ./Xzip -q $o bad.txt';;
    png) c='./Xtpnmtopng bad.ppm > $o';;
    pq) c='./Xtppmquant 64 bad.ppm > $o';;
  esac
  sh -c "o=$o; $c" 2> bad.err; rc=$?
  echo "MALFORMED op=$op rc=$rc"
  if { [ $op = lha ] || [ $op = zip ]; } && [ $rc -ne 0 ]; then fail=$((fail+1)); echo "MALFORMED op=$op rc=$rc expected 0"; fi
  ./ttjobk.ttp > /dev/null 2>&1; krc=$?
  if [ $krc -ne 0 ]; then fail=$((fail+1)); echo "MALFORMED op=$op rc=$rc ttjobk=FAIL"; fi
  rm -f $o
done
# final SSH-path check
./Xcjpeg -quality 85 s.ppm > post.jpg; rc=$?
if [ $rc -ne 0 ]; then fail=$((fail+1)); echo "POST rc=$rc"; fi
k final
echo "OPS cj=$cnt_cj dj=$cnt_dj lha=$cnt_lha zip=$cnt_zip png=$cnt_png pq=$cnt_pq big=$cnt_big"; for op in cj dj lha zip png pq big; do eval "echo REF $op \$ref_$op"; done
echo "NFR5 jobs=$N fail=$fail"
echo "== ttjobd log"; cat /tmp/fr7/tj.log
