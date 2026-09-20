#!/bin/bash
# tt_put.sh <local-path> <remote-path> — copy a file to the TT and verify it byte-for-byte.
# Never write to the card without this: a short or empty transfer is otherwise indistinguishable
# from success (RCA 2026-09-20, D3).
set -u
LOCAL=${1:?usage: tt_put.sh <local> <remote>}
REMOTE=${2:?usage: tt_put.sh <local> <remote>}
HOST=${TT_HOST:-atari-tt}
[ -s "$LOCAL" ] || { echo "tt_put: $LOCAL is empty or missing — refusing" >&2; exit 1; }
want=$(stat -c%s "$LOCAL")
timeout "${TT_TIMEOUT:-300}" ssh -o ConnectTimeout="${TT_CONNECT:-90}" "$HOST" "cat > '$REMOTE'" < "$LOCAL" || {
    echo "tt_put: transfer of $LOCAL failed" >&2; exit 1; }
back=$(mktemp)
trap 'rm -f "$back"' EXIT
timeout "${TT_TIMEOUT:-300}" ssh -o ConnectTimeout="${TT_CONNECT:-90}" "$HOST" "cat '$REMOTE'" > "$back"
got=$(stat -c%s "$back" 2>/dev/null || echo 0)
if [ "$got" -ne "$want" ] || ! cmp -s "$LOCAL" "$back"; then
    echo "tt_put: VERIFY FAILED for $REMOTE (sent $want bytes, read back $got)" >&2
    exit 1
fi
echo "tt_put: $LOCAL -> $REMOTE ($want bytes, verified)"
