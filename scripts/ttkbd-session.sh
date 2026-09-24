#!/bin/bash
# ttkbd-session.sh — use fractal's keyboard as the Atari TT030's keyboard (track tt030-remote-keyboard).
#
#   scripts/ttkbd-session.sh            raw serial on Modem 2 (default; no key needed, works with WiFi down)
#   scripts/ttkbd-session.sh --wifi     encrypted TCP to 192.168.0.30:7590 (needs your key on the TT)
#
# Run it on fractal from your desktop session (the keyboard is readable there via the uaccess rule).
# Stop typing on the TT with  Right Ctrl + Right Alt + Esc.
#
# Serial mode takes over the TT's Modem 2 console for the session: it types `exec ttkbdd -S` at the
# console prompt, grabs the keyboard, and on the stop chord sends a quit frame — ttkbdd exits and
# ttygetty respawns the console bash. The script then checks that the console answers again.
# If anything goes wrong, see "Revert by hand" at the end of this file.
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
PY=/usr/bin/python3                       # system python: has evdev + cryptography
PORT=${TTKBD_PORT:-/dev/atari-tt}         # CP2102 null-modem cable to the TT's Modem 2
TTKBDD=${TTKBDD:-/tmp/ttkbdd.prg}         # path of ttkbdd on the TT (not yet installed at boot)
TT=192.168.0.30                           # sam.int-exception: the TT has no DNS record
mode=${1:---serial}

die() { echo "ttkbd-session: $*" >&2; exit 1; }

# console SEND EXPECT SECONDS — write to the TT's serial console, wait for EXPECT in the reply.
console() {
    "$PY" - "$PORT" "$1" "$2" "$3" <<'E'
import os, select, sys, termios, time, tty
port, send, expect, secs = sys.argv[1], sys.argv[2].encode().decode("unicode_escape").encode("latin-1"), sys.argv[3].encode(), float(sys.argv[4])
fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
tty.setraw(fd)
a = termios.tcgetattr(fd); a[2] |= termios.CLOCAL | termios.CREAD; a[2] &= ~termios.CRTSCTS
a[4] = a[5] = termios.B38400; termios.tcsetattr(fd, termios.TCSANOW, a)
termios.tcflush(fd, termios.TCIFLUSH)
os.write(fd, send)
buf, end = b"", time.time() + secs
while time.time() < end and expect not in buf:
    if select.select([fd], [], [], 0.3)[0]:
        buf += os.read(fd, 512)
os.close(fd)
sys.exit(0 if expect in buf else 1)
E
}

serial() {
    [ -r "$PORT" ] && [ -w "$PORT" ] || die "cannot open $PORT (needs group sam)"
    if command -v fuser >/dev/null && fuser "$PORT" >/dev/null 2>&1; then
        die "$PORT is in use (picocom? atari-tt-slip.service?) — close it first"
    fi
    # A ttkbdd left running by an earlier session holds the line: a quit frame hands it back.
    if ! console '\x15\r' '# ' 5; then
        "$PY" "$DIR/ttkbd_send.py" --serial "$PORT" frames 800 2>/dev/null
        sleep 3
        console '\x15\r' '# ' 5 || die "the TT's serial console is not answering (see Revert by hand)"
    fi
    echo "ttkbd-session: console answered; starting ttkbdd on Modem 2"
    console "exec $TTKBDD -S\\r" 'ttkbdd: serial on' 10 || die "ttkbdd did not start ($TTKBDD on the TT?)"
    echo "ttkbd-session: typing on the TT — Right Ctrl + Right Alt + Esc to stop"
    if [ -n "${TTKBD_TEST_TYPE:-}" ]; then   # test hook: type text instead of grabbing a keyboard
        "$PY" "$DIR/ttkbd_send.py" --serial "$PORT" type "$TTKBD_TEST_TYPE"
        "$PY" "$DIR/ttkbd_send.py" --serial "$PORT" frames 800
    else
        "$PY" "$DIR/ttkbd_send.py" --serial "$PORT" grab   # sends the quit frame on the chord
    fi
    sleep 3
    if console '\x15\r' '# ' 10; then
        echo "ttkbd-session: done — the serial console is back"
    else
        die "the console did not come back (see Revert by hand)"
    fi
}

wifi() {
    [ -f "$HOME/.config/atari-tt/ttkbd.key" ] || die "no key: run $DIR/ttkbd_send.py keygen and install the public key as the TT's /etc/ttkbd.pub"
    timeout 5 bash -c "</dev/tcp/$TT/7590" 2>/dev/null || die "ttkbdd is not listening on $TT:7590 (start it on the TT: $TTKBDD)"
    echo "ttkbd-session: typing on the TT over WiFi — Right Ctrl + Right Alt + Esc to stop (connect takes 4-10 s)"
    "$PY" "$DIR/ttkbd_send.py" grab
    echo "ttkbd-session: done (the serial console was not touched)"
}

case "$mode" in
    --serial) serial ;;
    --wifi) wifi ;;
    *) die "usage: $0 [--serial|--wifi]" ;;
esac

# Revert by hand (serial mode), in order:
#   1. Quit frame over the cable:   /usr/bin/python3 scripts/ttkbd_send.py --serial /dev/atari-tt frames 800
#      (ttkbdd exits; ttygetty respawns the console within a few seconds)
#   2. Over WiFi, as sam:           ssh atari-tt 'for p in /proc/ttkbdd*; do kill -TERM ${p##*.}; done'
#   3. A key or the key table stuck: ssh atari-tt '/tmp/ttkbdd.prg --release'
#   4. Check:                        picocom -b 38400 /dev/atari-tt   then Enter -> "-bash-4.4#"
