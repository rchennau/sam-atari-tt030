#!/bin/bash
# tt_get.sh <remote-path> <local-path> — fetch a file from the TT, refusing an empty read.
# An SSH login here takes ~17 s and can time out; a timed-out read returns 0 bytes, which looked
# exactly like a successful fetch and produced a corrupted card write (RCA 2026-09-20, D3).
set -u
REMOTE=${1:?usage: tt_get.sh <remote> <local>}
LOCAL=${2:?usage: tt_get.sh <remote> <local>}
HOST=${TT_HOST:-atari-tt}
timeout "${TT_TIMEOUT:-180}" ssh -o ConnectTimeout="${TT_CONNECT:-90}" "$HOST" "cat '$REMOTE'" > "$LOCAL"
n=$(stat -c%s "$LOCAL" 2>/dev/null || echo 0)
if [ "$n" -eq 0 ]; then
    echo "tt_get: EMPTY read of $REMOTE — refusing (login timeout or missing file)" >&2
    rm -f "$LOCAL"
    exit 1
fi
echo "tt_get: $REMOTE -> $LOCAL ($n bytes)"
