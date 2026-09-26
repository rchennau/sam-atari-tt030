#!/bin/bash
# tt_zput.sh LOCAL REMOTE_DIR [NAME] — copy a file to the TT over Modem 2 (serial, 38400, ZMODEM) and verify it.
# Operator 2026-09-26: all uploads go over Modem 2, so no SSH (Dropbear boots the T425 and loads the 68030
# during timed gates). ~3.5 KB/s. Starts `rz -e -y` on the TT's serial console (ttygetty bash, as tt_serial.py),
# runs `sz -e` here, then compares SHA-256 over the same line. Exit 0 verified · 1 transfer or verify failed.
set -u
LOCAL=${1:?usage: tt_zput.sh LOCAL REMOTE_DIR [NAME]}
RDIR=${2:?usage: tt_zput.sh LOCAL REMOTE_DIR [NAME]}
NAME=${3:-$(basename "$LOCAL")}
PORT=${TT_PORT:-/dev/atari-tt}
HERE=$(cd "$(dirname "$0")" && pwd)
[ -s "$LOCAL" ] || { echo "tt_zput: $LOCAL is empty or missing" >&2; exit 1; }
STAGE=$(mktemp -d); trap 'rm -rf "$STAGE"' EXIT
cp "$LOCAL" "$STAGE/$NAME"                               # sz sends the basename: stage it under NAME
want=$(sha256sum "$STAGE/$NAME" | cut -d' ' -f1)
stty -F "$PORT" 38400 raw -echo -ixon -ixoff -crtscts clocal cread
printf '\r' > "$PORT"; sleep 1                             # a fresh prompt first
printf 'cd %s && rz -e -y\r' "$RDIR" > "$PORT"             # -y: overwrite an existing file of that name
sleep "${TT_RZWAIT:-4}"                                    # rz starts slowly while the 68030 is busy (2 s was too short, 2026-09-26)
( cd "$STAGE" && timeout "${TT_ZTIMEOUT:-1800}" sz -e "$NAME" < "$PORT" > "$PORT" ) 2> "$STAGE/sz.err" \
    || { echo "tt_zput: sz failed:" >&2; tail -3 "$STAGE/sz.err" >&2; exit 1; }
sleep 1
got=$(python3 "$HERE/tt_serial.py" --timeout 300 "openssl dgst -sha256 '$RDIR/$NAME'" | sed -n 's/.*= *\([0-9a-f]\{64\}\).*/\1/p' | tail -1)
[ "$got" = "$want" ] || { echo "tt_zput: $RDIR/$NAME checksum mismatch (want ${want:0:16}, got ${got:0:16})" >&2; exit 1; }
echo "tt_zput: $LOCAL -> $RDIR/$NAME ($(stat -c %s "$LOCAL") bytes, sha256 verified over serial)"
