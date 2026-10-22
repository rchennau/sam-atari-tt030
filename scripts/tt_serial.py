#!/usr/bin/env python3
"""tt_serial.py — run a command on the TT's serial console (Modem 2, 38400, ttygetty + bash) and print
its output. For when WiFi/SSH to the TT is degraded; stdlib only (no pyserial on the remote system management host).

  tt_serial.py [--timeout S] [--port /dev/atari-tt] 'COMMAND'     exit = the command's exit code
  tt_serial.py --clear [--port P]       only make sure the console answers (exit 0 yes, 1 no)
                                                                     (124 = no end marker in time)
The console runs `bash --noediting` with echo on, so the marker is built from two halves the echo
cannot contain whole. Needs the cable on Modem 2 and atari-tt-slip.service inactive (inf-card).
"""
import os
import re
import select
import sys
import termios
import time


def open_port(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd)
    a[0] = 0                                             # iflag: raw, no XON/XOFF (ttygetty turns it off too)
    a[1] = 0                                             # oflag
    a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL  # cflag: 8N1, no modem control, no RTS/CTS
    a[3] = 0                                             # lflag: raw, no echo
    a[4] = a[5] = termios.B38400                         # scc.xdd refuses 57600
    a[6][termios.VMIN], a[6][termios.VTIME] = 0, 0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


def console_ready(fd, wait=4.0):
    """True when bash answers a prompt. A serial ttkbdd (`ttkbdd -S`, exec'd over the console by a keyboard session)
    left behind by a session that did not end cleanly holds the line and swallows everything (defect ae6225de,
    2026-10-21: tt_zput failed "rz did not start"); then a ttkbd quit frame ends it, ttygetty respawns bash, and we
    look again. Same recovery as ttkbd-session.sh's start; this is the shared path tt_zput.sh and run() use."""
    def prompt():
        termios.tcflush(fd, termios.TCIFLUSH)
        os.write(fd, b"\x15\r")                        # ^U clears a half-typed line, CR asks for a prompt
        buf, end = b"", time.monotonic() + wait
        while time.monotonic() < end:
            if select.select([fd], [], [], 0.2)[0]:
                buf += os.read(fd, 256)
                if b"# " in buf:
                    return True
        return False
    if prompt():
        return True
    import subprocess
    here = os.path.dirname(os.path.abspath(__file__))
    print("tt_serial: no console prompt; sending a ttkbd quit frame (orphaned ttkbdd -S?)", file=sys.stderr)
    subprocess.run([sys.executable, os.path.join(here, "ttkbd_send.py"), "--serial", PORT_IN_USE[0], "frames", "800"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
    time.sleep(4)                                        # ttkbdd exits, ttygetty respawns bash
    return prompt()


PORT_IN_USE = ["/dev/atari-tt"]


def run(cmd, port="/dev/atari-tt", timeout=120.0):
    # One user of the line at a time: a status command written mid-ZMODEM corrupted an upload (2026-09-26).
    # tt_zput.sh takes the same lock. Blocks until the line is free.
    import fcntl
    lk = open("/tmp/atari-tt-serial.lock", "w")
    if not os.environ.get("TT_SERIAL_LOCKED"):         # tt_zput.sh already holds it when it calls us
        fcntl.flock(lk, fcntl.LOCK_EX)
    fd = open_port(port)
    PORT_IN_USE[0] = port
    if not console_ready(fd):
        os.close(fd)
        print("tt_serial: the TT's serial console does not answer", file=sys.stderr)
        return 124
    tag = f"{os.getpid()}x{int(time.time())}"
    # printf joins the halves on the TT; the echoed command line holds them apart
    line = f"{cmd}; printf 'T4E''ND-{tag}-%s\\n' $?\r"
    os.write(fd, b"\r")
    time.sleep(0.3)
    termios.tcflush(fd, termios.TCIFLUSH)
    for i in range(0, len(line), 32):                    # no flow control: pace it
        os.write(fd, line[i:i + 32].encode())
        time.sleep(0.03)
    buf, end = b"", time.monotonic() + timeout
    rx = re.compile(rb"T4END-" + tag.encode() + rb"-(\d+)")
    last = time.monotonic()
    while time.monotonic() < end:
        r, _, _ = select.select([fd], [], [], 0.5)
        if not r and time.monotonic() - last > 3:
            # The TT's serial transmit can stall with output queued until it receives a byte (2026-10-16:
            # a finished run's end marker arrived only when the next run typed). A lone CR restarts it;
            # bash sees an empty line. ponytail: assumes the command does not read stdin.
            os.write(fd, b"\r")
            last = time.monotonic()
        if r:
            last = time.monotonic()
            buf += os.read(fd, 4096)
            m = rx.search(buf)
            if m:
                out = buf[:m.start()].decode("latin-1").replace("\r", "")
                first = out.find("\n")                   # drop the echoed command line
                sys.stdout.write(out[first + 1:] if first >= 0 else out)
                os.close(fd)
                return int(m.group(1))
    os.close(fd)
    sys.stdout.write(buf.decode("latin-1").replace("\r", ""))
    print(f"\ntt_serial: no end marker within {timeout:.0f} s", file=sys.stderr)
    return 124


if __name__ == "__main__":
    args, timeout, port = sys.argv[1:], 120.0, "/dev/atari-tt"
    clear = "--clear" in args
    args = [a for a in args if a != "--clear"]
    while args and args[0].startswith("--"):
        if args[0] == "--timeout":
            timeout = float(args[1])
        elif args[0] == "--port":
            port = args[1]
        args = args[2:]
    if clear and not args:
        PORT_IN_USE[0] = port
        _fd = open_port(port)
        ok = console_ready(_fd)
        os.close(_fd)
        sys.exit(0 if ok else 1)
    if len(args) != 1:
        print(__doc__, file=sys.stderr)
        sys.exit(2)
    sys.exit(run(args[0], port, timeout))
