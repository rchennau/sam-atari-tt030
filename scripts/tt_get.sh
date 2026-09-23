#!/bin/bash
# tt_get.sh <remote-path> <local-path> — fetch a file from the TT via /ram, refusing a short read.
# An SSH login here takes ~17 s and can time out; a timed-out read returns 0 bytes, which looked
# exactly like a successful fetch and produced a corrupted card write (RCA 2026-09-20, D3).
set -u
REMOTE=${1:?usage: tt_get.sh <remote> <local>}
LOCAL=${2:?usage: tt_get.sh <remote> <local>}
HOST=${TT_HOST:-atari-tt}
# Stage through the RAM disk: disk I/O concurrent with a DaynaPORT send kills the send (measured
# 2026-09-22, NFR-2 426 root cause), so copy to /ram first and send with the disk idle.
# ponytail: file must fit in /ram (TT-RAM, ~60 MiB free); stream from disk + retry if that ever bites.
err=$(mktemp)
timeout "${TT_TIMEOUT:-180}" ssh -o ConnectTimeout="${TT_CONNECT:-90}" "$HOST" \
    "t=/ram/tt_get.\$\$; cp '$REMOTE' \$t || exit 3; echo TTGET_SIZE=\$(wc -c < \$t) >&2; cat \$t; rm -f \$t" > "$LOCAL" 2> "$err"
want=$(sed -n "s/^TTGET_SIZE= *//p" "$err")
n=$(stat -c%s "$LOCAL" 2>/dev/null || echo 0)
if [ "$n" -eq 0 ] || [ "$n" != "$want" ]; then
    echo "tt_get: read of $REMOTE got $n of ${want:-?} bytes — refusing (login timeout, missing file, or /ram full)" >&2
    grep -v ^TTGET_SIZE= "$err" >&2; rm -f "$LOCAL" "$err"
    exit 1
fi
rm -f "$err"
echo "tt_get: $REMOTE -> $LOCAL ($n bytes)"
