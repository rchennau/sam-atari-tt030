#!/bin/bash
# tt_stack_check.sh — tt030-rom-set NFR-2 SAM stack checklist, run from fractal. One line per item:
#   PASS|FAIL|SKIP <item> <evidence>
# Exit 0 only when every item PASSes; a SKIP (e.g. a loopback with no plug fitted) is not a pass.
#   scripts/tt_stack_check.sh            check (ttjob port md5s against scripts/tt_stack_check.baseline)
#   scripts/tt_stack_check.sh --record   write the baseline from this run (do it on stock TOS 3.06 only)
#   TT_LOOPBACK="modem1 modem2 lan"      ports with a loopback plug fitted (FR-2); others SKIP
. "$(dirname "$0")/tt_log.sh"
cd "$(dirname "$0")" || exit 1
BASE=tt_stack_check.baseline; HOST=${TT_HOST:-atari-tt}; XVDI_MD5=d238c3ac26a3a1c16c8131e83a091985   # mode-39 table

out=$(timeout 600 ssh -o ConnectTimeout=60 "$HOST" "LOOP='${TT_LOOPBACK:-}' XVDI_MD5=$XVDI_MD5 sh -s" <<'EOF'
r() { if eval "$2" >/dev/null 2>&1; then echo "PASS $1 $3"; else echo "FAIL $1 $3"; fi; }
r mint      'grep -q FreeMiNT /kern/version'                          "$(cut -c1-24 /kern/version)"
r xaaes     'ps ax | grep -q "[A]ESSYS"'                              "AESSYS process"
r ext2-root 'grep -q ext2 /kern/filesystems && ls -l /bin | grep -q "> /f/bin$"' "/bin -> /f/bin, ext2 loaded"
r cbhd      'ls /c/ && ls /d/ && ls /e/'                              "C: D: E: listed"
r daynaport 'ifconfig en0 | grep -q "inet 192.168.0.30"'              "en0 inet"
r xvdi-39   'grep -q "(xVDI)" /kern/cookiejar && test "$(md5sum < /c/auto/XVDI.BIB | cut -c1-32)" = "$XVDI_MD5"' "cookie + XVDI.BIB md5"
r ttjobd    'test "$(grep -c " present$" /var/run/ttjob.topology)" = 3 && grep -q ttjobd /tmp/t425.lock' "$(grep -c " present$" /var/run/ttjob.topology)/3 nodes"
r ssh       true                                                      "this session"
r yum       'yum list ttjob | grep -q ttjob'                          "yum list from the mirror (the TT yum has no makecache)"
r ttkbdd    'ps ax | grep -q "[t]tkbdd"'                              "ttkbdd process"
# one ttjob port per op class: archive, maths, image (md5 compared with the baseline on fractal)
cd /tmp && head -c 65536 /etc/termcap > sc.txt && touch -t 202601010000 sc.txt && rm -f sc.lzh && lha aq sc.lzh sc.txt >/dev/null 2>&1
echo "MD5 lha $(md5sum < sc.lzh | cut -c1-32)"
echo "MD5 tbc $(echo '2^5000' | tbc | md5sum | cut -c1-32)"
echo "MD5 cjpeg $(cjpeg /tmp/fr7/p800.ppm 2>/dev/null | md5sum | cut -c1-32)"
for p in modem1 modem2 lan; do
  case " $LOOP " in *" $p "*) stty -F /dev/$p raw -echo 9600 2>/dev/null; (sleep 1; echo sam-loop > /dev/$p) &
     if timeout 5 head -c 8 /dev/$p | grep -q sam-loop; then echo "PASS loop-$p echoed"; else echo "FAIL loop-$p no echo"; fi;;
  *) echo "SKIP loop-$p no loopback plug (FR-2)";; esac
done
EOF
)
rc=$?
res=$( { [ $rc = 0 ] || echo "FAIL ssh-session rc=$rc"
echo "$out" | grep -v '^MD5 '
md5s=$(echo "$out" | grep '^MD5 ')
if [ "${1:-}" = --record ]; then echo "$md5s" > "$BASE"; echo "SKIP ttjob-ports baseline recorded ($(wc -l < "$BASE") ports)"
elif [ ! -s "$BASE" ]; then echo "FAIL ttjob-ports no baseline (run --record on stock)"
else diff <(echo "$md5s") "$BASE" >/dev/null && echo "PASS ttjob-ports md5 = baseline" || echo "FAIL ttjob-ports $(diff <(echo "$md5s") "$BASE" | grep '^<' | tr '\n' ' ')"; fi
# ttmon: the retained runtime frame proves nothing about now, so wait for a live telemetry message
tm=$(timeout 75 mosquitto_sub -h mqtt.sam.int -t sam/node/telemetry/atari-tt030 -C 1 -W 70 -R 2>/dev/null)
[ -n "$tm" ] && echo "PASS ttmon live telemetry $(echo "$tm" | grep -o '"uptime_s":[0-9]*')" || echo "FAIL ttmon no telemetry in 70 s"; } )
echo "$res"
n=$(echo "$res" | grep -c '^PASS'); t=$(echo "$res" | grep -c .)
echo "== $n/$t PASS"
[ "$n" = "$t" ]
