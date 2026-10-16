#!/bin/bash
# atw_moderun.sh MODE... — PMS tt030-atw800-video-modes Phase 3: per mode, boot it and record FR-3 checks 2-4 + FR-4.
#   set MODE (atw_mode.sh) -> tt_reboot --wait-back -> wait for uptime >= 60 s ->
#   (2) atwres W H PLANES vs docs/atw-modes.tsv  (3) xa_boot.log size line  (4) xVDI + EdDI cookies   [ssh]
#   then vdibench x3 over the serial console (C4: no SSH during a vdibench run), retry a failed run up to 2 times.
# Check 1 (a picture on the monitor) is the operator's; the row leaves it "operator".
# Rows go to $OUT (TSV). Stops at the first mode whose machine does not come back, so `atw_mode.sh restore` can run.
. "$(dirname "$0")/tt_log.sh"
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
TSV=$here/../docs/atw-modes.tsv
OUT=${OUT:-$HOME/.local/state/tt030-ops/atw-moderun.tsv}
[ -f "$OUT" ] || printf 'utc\tmode\tname\tatwres\tsize_ok\txa_size\tcookies\tbench\n' > "$OUT"
tt() { ssh -o ConnectTimeout=30 atari-tt "$@"; }

for m in "$@"; do
    want=$(awk -F'\t' -v m="$m" '$1 == m { print $3, $4, $7 }' "$TSV")
    [ -n "$want" ] || { echo "moderun: no mode $m"; exit 1; }
    bash "$here/atw_mode.sh" set "$m" || exit 1
    bash "$here/tt_reboot.sh" --wait-back || { echo "moderun: mode $m: TT did not come back — run atw_mode.sh restore"; exit 1; }
    for i in $(seq 1 30); do
        up=$(tt 'cut -d. -f1 /kern/uptime' 2>/dev/null) && [ "${up:-0}" -ge 60 ] && break
        timeout 10 tail -f /dev/null
    done
    res=$(tt /f/root/atwres.ttp 2>/dev/null)
    xa=$(tt 'grep -a "size=\[" /c/mint/1-19-4eb/xaaes/xa_boot.log | tail -1' 2>/dev/null | sed 's/^ *//; s/,  *colours.*//')
    ck=$(tt 'echo xVDI=$(grep -ac xVDI /kern/cookiejar) EdDI=$(grep -ac EdDI /kern/cookiejar)' 2>/dev/null)
    ok=no; [ "$res" = "$want" ] && ok=yes
    echo "moderun: mode $m: atwres '$res' want '$want' ($ok); xa '$xa'; $ck"
    bench=
    for run in 1 2 3; do
        for try in 1 2 3; do
            line=$(timeout 330 python3 "$here/tt_serial.py" --timeout 300 \
                'cd /c && n=$(wc -l < vdibench.txt) && ./vdibench.prg > /dev/null 2>&1; [ $(wc -l < vdibench.txt) -gt $n ] && tail -1 vdibench.txt' 2>/dev/null | grep '^screen' | tail -1)
            [ -n "$line" ] && break
        done
        bench="$bench${bench:+ || }${line:-FAILED}"
        echo "moderun: mode $m run $run: ${line:-FAILED}"
    done
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$(date -u +%FT%TZ)" "$m" "$(awk -F'\t' -v m="$m" '$1 == m { print $2 }' "$TSV")" \
        "$res" "$ok" "$xa" "$ck" "$bench" >> "$OUT"
done
