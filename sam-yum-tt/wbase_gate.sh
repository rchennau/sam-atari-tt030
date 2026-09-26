#!/bin/bash
# wbase_gate.sh — W-BASE timing gate on the TT (tt030-t425-kernel-ports FR-8): stock binary vs the -m68020-60
# rebuild, same input, output compared by md5sum. Run from /ram with the rebuilt binaries in /ram/wb/.
#   bash wbase_gate.sh RUNS      one line per case and run: "case run stock_s new_s same|DIFF"
# Inputs: /ram/big = /etc/termcap x3 (text), /ram/bigs = big sorted. Stock tools are found on PATH.
cd /ram || exit 1
[ -f big ] || { cat /etc/termcap /etc/termcap /etc/termcap > big; sort big > bigs; }
TIMEFORMAT=%R
N=/ram/wb
run() {  # name, then the command with @ standing for the tool path
  local name=$1 tool=$2; shift 2
  local s=$(type -P "$tool") cmd="$*" ts tn a b
  if [ $((r % 2)) = 1 ]; then   # alternate who goes first: the first run pays the cold disk cache
    ts=$( { time eval "${cmd//@/$s}" > o.s 2>/dev/null; } 2>&1 )
    tn=$( { time eval "${cmd//@/$N/$tool}" > o.n 2>/dev/null; } 2>&1 )
  else
    tn=$( { time eval "${cmd//@/$N/$tool}" > o.n 2>/dev/null; } 2>&1 )
    ts=$( { time eval "${cmd//@/$s}" > o.s 2>/dev/null; } 2>&1 )
  fi
  a=$(md5sum < o.s); b=$(md5sum < o.n)
  echo "$name $r $ts $tn $([ "$a" = "$b" ] && echo same || echo DIFF)"
}
for r in $(seq 1 "${1:-2}"); do
  run grep-c   grep '@ -c "tc=" big'
  run grep-i   grep '@ -i "vt1[0-9]0" big'
  run sed      sed  '@ "s/:\([a-z][a-z]\)=/:\1#/g" big'
  run sort     sort '@ big'
  run wc       wc   '@ big'
  run md5sum   md5sum '@ big'
  run cut      cut  '@ -d: -f1,3 big'
  run tr       tr   '@ a-z A-Z < big'
  run uniq     uniq '@ -c bigs'
  run ls-lR    ls   '@ -lR /usr/lib'
  run du       du   '@ -s /usr'
  run find     find '@ /usr -name "*.h"'
  run tar      tar  '@ cf - /usr/include'
done
