#!/bin/bash
# tt_boot_time.sh — tt030-rom-set FR-1 cold-boot baseline: mains on -> SSH accepting a login, n runs, from fractal.
#   scripts/tt_boot_time.sh [n=5]     one line per run, then SUMMARY lines (median/min/max) for boot_s and pre_mint_s
# pre_mint_s = boot_s - /kern/uptime at the first login: memtest + TOS init + XBOOT + HD driver (an upper bound on the
# RAM test; stock TOS prints nothing on serial). ponytail: +-1 s from the ssh round trip.
# Memory test RUNNING only (operator 2026-10-19: bypassed runs stay open in FR-1; stock TOS 3.06 has no remote skip).
# FreeMiNT here has no halt binary (tt_reboot.sh), so each run syncs over SSH before the relay drops the mains.
# t0 is taken just before `tt_power.sh on`. ponytail: the plug's ~0.5 s client start counts against the boot; fine at
# a minutes-scale boot, use the serial capture's first byte if sub-second matters.
. "$(dirname "$0")/tt_log.sh"
set -u
cd "$(dirname "$0")" || exit 1
N=${1:-5}; HOST=${TT_HOST:-atari-tt}; OFF=${TT_OFF_S:-15}; LIMIT=${TT_BOOT_LIMIT_S:-900}
up() { timeout 20 ssh -o ConnectTimeout=5 -o BatchMode=yes "$HOST" true 2>/dev/null; }

up || { echo "tt_boot_time: $HOST not answering SSH before run 1 — refusing to cut power on an unknown state" >&2; exit 2; }
times=(); pre=()
for i in $(seq 1 "$N"); do
    timeout 60 ssh -o ConnectTimeout=10 "$HOST" sync || { echo "tt_boot_time: run $i sync failed — not cutting power" >&2; exit 2; }
    sleep 2
    ./tt_power.sh off || exit $?
    sleep "$OFF"
    t0=$(date +%s.%N)
    ./tt_power.sh on || exit $?
    until up; do
        if (( $(date +%s) - ${t0%.*} > LIMIT )); then
            echo "tt_boot_time: run $i no SSH after ${LIMIT}s — check the serial capture / XBOOT menu" >&2; exit 1
        fi
        sleep 1
    done
    s=$(echo "$(date +%s.%N) - $t0" | bc); s=$(printf '%.1f' "$s"); times+=("$s")
    k=$(timeout 20 ssh -o ConnectTimeout=5 "$HOST" cat /kern/uptime | cut -d' ' -f1)
    [ -n "$k" ] || { echo "tt_boot_time: run $i could not read /kern/uptime" >&2; exit 1; }
    p=$(printf '%.1f' "$(echo "$s - $k" | bc)"); pre+=("$p")
    echo "RUN $i boot_s=$s mint_uptime_s=$k pre_mint_s=$p"
done
summary() { printf '%s\n' "${@:2}" | sort -n | awk -v n="$N" -v f="$1" '{a[NR]=$1} END {
    m = (NR % 2) ? a[(NR+1)/2] : (a[NR/2] + a[NR/2+1]) / 2
    printf "SUMMARY %s n=%d median=%.1f min=%.1f max=%.1f\n", f, n, m, a[1], a[NR] }'; }
summary boot_s "${times[@]}"; summary pre_mint_s "${pre[@]}"
