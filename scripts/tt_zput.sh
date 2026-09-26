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
exec 9> /tmp/atari-tt-serial.lock; flock 9   # one user of the line at a time (tt_serial.py takes it too)
export TT_SERIAL_LOCKED=1                     # our own tt_serial.py call below must not wait on us
stty -F "$PORT" 38400 raw -echo -ixon -ixoff -crtscts clocal cread
exec 3<>"$PORT"          # hold the port open for the whole run: bytes arriving while nothing has it open are dropped
# Start rz and wait for its ZRINIT frame (hex header "B01...") in ONE process, on the held-open port: a fixed
# delay lost the race (2026-09-26, ZMODEM bytes landed in bash), and a separate open missed the frame. Raw
# bytes, not grep: lrzsz ends the header with CR + 0x8A. rz re-sends ZRINIT, so consuming one is harmless.
python3 -c 'import os,sys,time,termios
fd=3; termios.tcflush(fd,termios.TCIFLUSH); os.set_blocking(fd,False)
os.write(fd,b"\r"); time.sleep(1)
os.write(fd,("cd %s && rz -e -y\r" % sys.argv[1]).encode()); end=time.time()+float(sys.argv[2]); buf=b""
while time.time()<end:
    try: buf+=os.read(fd,256)
    except BlockingIOError: time.sleep(0.05)
    if b"B01" in buf: sys.exit(0)
sys.exit(1)' "$RDIR" "${TT_RZWAIT:-30}" \
    || { printf '\030\030\030\030\030\r' >&3; echo "tt_zput: rz did not start on the TT console" >&2; exit 1; }
( cd "$STAGE" && timeout "${TT_ZTIMEOUT:-1800}" sz -e "$NAME" < "$PORT" > "$PORT" ) 2> "$STAGE/sz.err" \
    || { echo "tt_zput: sz failed:" >&2; tail -3 "$STAGE/sz.err" >&2; exit 1; }
sleep 1
got=$(python3 "$HERE/tt_serial.py" --timeout 300 "openssl dgst -sha256 '$RDIR/$NAME'" | sed -n 's/.*= *\([0-9a-f]\{64\}\).*/\1/p' | tail -1)
[ "$got" = "$want" ] || { echo "tt_zput: $RDIR/$NAME checksum mismatch (want ${want:0:16}, got ${got:0:16})" >&2; exit 1; }
echo "tt_zput: $LOCAL -> $RDIR/$NAME ($(stat -c %s "$LOCAL") bytes, sha256 verified over serial)"
