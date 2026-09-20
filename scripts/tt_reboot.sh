#!/bin/bash
# tt_reboot.sh — restart the TT030 from fractal and wait for it to go down.
#
# FreeMiNT on this machine ships no reboot/halt/shutdown binary (/sbin holds only tzinit), so
# `ssh atari-tt reboot` is a SILENT no-op — it once left the machine up for 1 h 47 m while a poller
# waited (RCA 2026-09-20). C:\TTREBOOT.PRG clears the memory-valid markers and jumps the ROM reset
# vector, so the restart takes the full power-on path (memory test included).
#
# Note: the boot SET is chosen only at XBOOT's interactive menu, so this returns the machine to
# whatever set is picked there — it cannot select one. Usage: tt_reboot.sh [--wait-back]
set -u
HOST=${TT_HOST:-atari-tt}
SSH="ssh -o ConnectTimeout=${TT_CONNECT:-90} $HOST"

timeout 90 $SSH 'cd /c && (./TTREBOOT.PRG &)' >/dev/null 2>&1 || true   # connection dies with the reset
for i in $(seq 1 20); do
    sleep 5
    ping -c1 -W2 "$(getent hosts ${TT_IP:-192.168.0.30} | awk '{print $1}' || echo ${TT_IP:-192.168.0.30})" >/dev/null 2>&1 || {
        echo "tt_reboot: machine is down — restarting"
        [ "${1:-}" = "--wait-back" ] || exit 0
        for j in $(seq 1 60); do
            sleep 15
            if timeout 120 $SSH true 2>/dev/null; then echo "tt_reboot: back up after $((j*15))s"; exit 0; fi
        done
        echo "tt_reboot: did not come back — check the XBOOT menu (it may be waiting for a set)" >&2
        exit 2
    }
done
echo "tt_reboot: machine never went down — TTREBOOT.PRG missing or not executable?" >&2
exit 1
