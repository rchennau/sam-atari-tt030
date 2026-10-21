#!/bin/bash
# ttkbd_key_install.sh — make one user's WiFi keyboard key the TT's key (tt030-remote-keyboard; the TT accepts exactly one).
#   1. that user, on the remote system management host:  python3 scripts/ttkbd_send.py keygen   (prints 64 hex chars)
#   2. anyone with ssh to the TT:                          scripts/ttkbd_key_install.sh <64-hex public key>
# Backs up the current /etc/ttkbd.pub to /etc/ttkbd.pub.<UTC stamp>, writes the 32 raw bytes, checks the md5 on the TT,
# restarts ttkbdd (/etc/rc.ttkbdd) and checks it listens on :7590. The previous key holder loses WiFi access.
# Undo: ssh atari-tt 'cp /etc/ttkbd.pub.<stamp> /etc/ttkbd.pub' then rerun the restart part (this script with that key's hex).
# Exit 0 installed + listening · 1 refused or failed.
. "$(dirname "$0")/tt_log.sh"
set -euo pipefail
hex=${1:-}
[[ $hex =~ ^[0-9a-fA-F]{64}$ ]] || { echo "usage: ttkbd_key_install.sh <64-hex Ed25519 public key from ttkbd_send.py keygen>" >&2; exit 1; }
want=$(printf '%s' "$hex" | xxd -r -p | md5sum | cut -d' ' -f1)
stamp=$(date -u +%Y%m%dT%H%M%SZ)
ssh atari-tt "cp /etc/ttkbd.pub /etc/ttkbd.pub.$stamp && cat > /etc/ttkbd.pub.new" < <(printf '%s' "$hex" | xxd -r -p)
ssh atari-tt "[ \"\$(md5sum < /etc/ttkbd.pub.new | cut -d' ' -f1)\" = $want ] && mv /etc/ttkbd.pub.new /etc/ttkbd.pub && sync" \
    || { echo "ttkbd_key_install: md5 mismatch on the TT — /etc/ttkbd.pub unchanged" >&2; exit 1; }
echo "ttkbd_key_install: /etc/ttkbd.pub = $want (previous kept as /etc/ttkbd.pub.$stamp)"
# exact last field: MiNT ps pads the name ("ttkbdd "), and a regex on the line also matches this shell's "rc.ttkbdd"
ssh atari-tt 'for p in $(ps ax | awk "\$NF == \"ttkbdd\" {print \$1}"); do kill $p; done; sleep 2; sh /etc/rc.ttkbdd'
for _ in 1 2 3 4 5 6; do
    timeout 5 bash -c '</dev/tcp/192.168.0.30/7590' 2>/dev/null && { echo "ttkbd_key_install: ttkbdd listening on :7590"; exit 0; }  # sam.int-exception: the TT has no DNS record
    sleep 5
done
echo "ttkbd_key_install: ttkbdd not listening on :7590 after restart (see /var/log/ttkbdd.log on the TT)" >&2; exit 1
