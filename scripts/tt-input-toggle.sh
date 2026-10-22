#!/bin/bash
# tt-input-toggle.sh — the "TT Keyboard & Mouse" launcher icon (track tt030-remote-keyboard FR-12).
# --start (FR-13, the TT's request listener): start only; a running session is left alone.
# Click 1: start a serial ttkbd-session (this keyboard and mouse drive the TT). Click 2: stop it through the sender's
# normal exit path (SIGINT -> ungrab, quit frame, the TT releases every key and button; ttygetty respawns the console).
# Refuses, with a notification, while another program holds the Modem 2 line lock (tt_zput.sh, tt_serial.py, a mode
# run): a session would hijack the console mid-transfer. The session then holds that lock itself until it ends.
# Runs as the desktop user (evdev needs the uaccess rule; /dev/atari-tt is group sam). Serial only: WiFi needs the
# sender key in the user's own ~/.config/atari-tt/ (run ttkbd-session.sh --wifi from a terminal for that).
. "$(dirname "$0")/tt_log.sh"
DIR=$(cd "$(dirname "$0")" && pwd)
LOCK=/tmp/atari-tt-serial.lock
PIDF=${XDG_RUNTIME_DIR:-/tmp}/tt-input-toggle.pid
note() { notify-send -a "TT Keyboard & Mouse" -i input-keyboard "TT Keyboard & Mouse" "$1" 2>/dev/null; echo "tt-input-toggle: $1"; }

if [ -f "$PIDF" ] && kill -0 "$(cat "$PIDF")" 2>/dev/null; then
    [ "${1:-}" = --start ] && { echo "tt-input-toggle: already running"; exit 0; }   # FR-13: a TT request never stops one
    pkill -INT -u "$(id -u)" -f "ttkbd_send.py --serial .* grab" && note "Stopping — control back on this computer" \
        || note "Session is still starting; click again in a few seconds"
    exit 0
fi

exec 9< "$LOCK" 2>/dev/null || exec 9> "$LOCK"      # read is enough for flock; the file is usually sam's 0644
if ! flock -n 9; then
    note "Not started: the TT's serial line is busy (an upload or test is running). Try again when it finishes."
    exit 1
fi
note "Starting — keyboard and mouse go to the TT. Stop: click again, Right Ctrl + Right Alt + Esc, or the TT's top-right corner."
TT_SERIAL_LOCKED=1 setsid bash "$DIR/ttkbd-session.sh" --serial > "${XDG_RUNTIME_DIR:-/tmp}/tt-input-toggle.log" 2>&1 &
echo $! > "$PIDF"
wait $!
rc=$?
rm -f "$PIDF"
[ $rc = 0 ] && note "Stopped — the TT's console is back" \
    || note "Session ended with an error (rc $rc): $(tail -1 "${XDG_RUNTIME_DIR:-/tmp}/tt-input-toggle.log")"
exit $rc
