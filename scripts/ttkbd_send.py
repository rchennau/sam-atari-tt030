#!/usr/bin/env python3
"""ttkbd_send.py — fractal side of the TT030 remote keyboard (track tt030-remote-keyboard, Phases 2-3).

Connects to ttkbdd on the TT, runs the handshake, then sends encrypted key frames.

  ttkbd_send.py keygen                       write ~/.config/atari-tt/ttkbd.key (Ed25519 seed, 600)
                                             and print the 32-byte public key hex for /etc/ttkbd.pub
  ttkbd_send.py type "text"                  type text (UK layout subset), then close
  ttkbd_send.py frames HEX...                send raw frames: each HEX is scancode|flags<<8 (e.g. 2a 1e 19e)
  ttkbd_send.py grab [--device PATH]         capture fractal's keyboard (evdev, exclusive grab) and type on
                                             the TT until Right Ctrl + Right Alt + Esc; needs read access to
                                             the event device (root:input 660 on fractal)
  options: --host 192.168.0.30 --port 7590 --hold SECONDS (keep the session open, heartbeats on)
           --serial /dev/atari-tt  raw serial to Modem 2 instead of WiFi (D4; TT runs ttkbdd -s /dev/ttyS1)

Wire (all TT-side reads are fixed sizes):
  TT -> fractal  hello  : ver(1)=1 | chal(16) | tt_x25519_pub(32)
  fractal -> TT  auth   : f_x25519_pub(32) | sig(64)   sig = Ed25519(chal | tt_pub | f_pub | b"ttkbd1")
  key = BLAKE2b-256(x25519(f_sk, tt_pub) | chal | tt_pub | f_pub)
  fractal -> TT  record : mac(16) | ct(2)   XChaCha20-Poly1305, nonce = counter (u64 LE) padded to 24,
                          plaintext {scancode, flags}; flags bit0 break, bit1 heartbeat, bit3 quit (serial)
Heartbeat every 1 s; the TT releases all keys after 3 s of silence.
Python `cryptography` has no XChaCha20-Poly1305, so HChaCha20 is done here (RFC draft-irtf-cfrg-xchacha
§2.3) and the IETF ChaCha20-Poly1305 does the rest — the same construction as Monocypher's crypto_aead_lock.
"""
import argparse
import hashlib
import os
import socket
import struct
import sys
import time
from pathlib import Path

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey, X25519PublicKey
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat

KEYFILE = Path("~/.config/atari-tt/ttkbd.key").expanduser()
F_BREAK, F_HEARTBEAT, F_QUIT = 1, 2, 8

# UK Atari scancodes (keyboard/en_uk.tbl, unpatched — the TT loads it for the session, D2).
# ponytail: letters/digits/common punctuation only; Phase 3's evdev map replaces this.
SC = {**{c: s for c, s in zip("1234567890-=", range(0x02, 0x0e))},
      **{c: s for c, s in zip("qwertyuiop[]", range(0x10, 0x1c))},
      **{c: s for c, s in zip("asdfghjkl;'", range(0x1e, 0x29))},
      **{c: s for c, s in zip("zxcvbnm,./", range(0x2c, 0x36))},
      " ": 0x39, "\n": 0x1c, "\t": 0x0f, "#": 0x2b}
SHIFTED = {**{c: s for c, s in zip('!"£$%^&*()_+', range(0x02, 0x0e))},
           **{c: s for c, s in zip("QWERTYUIOP{}", range(0x10, 0x1c))},
           **{c: s for c, s in zip('ASDFGHJKL:@', range(0x1e, 0x29))},
           **{c: s for c, s in zip("ZXCVBNM<>?", range(0x2c, 0x36))}, "~": 0x2b}
LSHIFT = 0x2a


def text_frames(text):
    out = []
    for ch in text:
        if ch in SC:
            out += [(SC[ch], 0), (SC[ch], F_BREAK)]
        elif ch in SHIFTED:
            s = SHIFTED[ch]
            out += [(LSHIFT, 0), (s, 0), (s, F_BREAK), (LSHIFT, F_BREAK)]
        else:
            raise SystemExit(f"no scancode for {ch!r}")
    return out


def _rotl(v, n):
    return ((v << n) | (v >> (32 - n))) & 0xFFFFFFFF


def hchacha20(key, nonce16):
    s = list(struct.unpack("<4I", b"expand 32-byte k") + struct.unpack("<8I", key) + struct.unpack("<4I", nonce16))

    def qr(a, b, c, d):
        s[a] = (s[a] + s[b]) & 0xFFFFFFFF; s[d] = _rotl(s[d] ^ s[a], 16)
        s[c] = (s[c] + s[d]) & 0xFFFFFFFF; s[b] = _rotl(s[b] ^ s[c], 12)
        s[a] = (s[a] + s[b]) & 0xFFFFFFFF; s[d] = _rotl(s[d] ^ s[a], 8)
        s[c] = (s[c] + s[d]) & 0xFFFFFFFF; s[b] = _rotl(s[b] ^ s[c], 7)
    for _ in range(10):
        qr(0, 4, 8, 12); qr(1, 5, 9, 13); qr(2, 6, 10, 14); qr(3, 7, 11, 15)
        qr(0, 5, 10, 15); qr(1, 6, 11, 12); qr(2, 7, 8, 13); qr(3, 4, 9, 14)
    return struct.pack("<8I", *(s[0:4] + s[12:16]))


def xchacha_lock(key, nonce24, pt):
    """-> mac(16) | ct, the layout crypto_aead_lock writes (ct and mac are separate there)."""
    sealed = ChaCha20Poly1305(hchacha20(key, nonce24[:16])).encrypt(b"\0\0\0\0" + nonce24[16:], pt, None)
    return sealed[-16:] + sealed[:-16]


def nonce(counter):
    return struct.pack("<Q", counter) + bytes(16)


def keygen():
    if KEYFILE.exists():
        raise SystemExit(f"{KEYFILE} exists — refusing to overwrite")
    KEYFILE.parent.mkdir(parents=True, exist_ok=True)
    seed = os.urandom(32)
    fd = os.open(KEYFILE, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    os.write(fd, seed)
    os.close(fd)
    pub = Ed25519PrivateKey.from_private_bytes(seed).public_key().public_bytes(Encoding.Raw, PublicFormat.Raw)
    print(pub.hex())


def recv_exact(s, n):
    b = b""
    while len(b) < n:
        c = s.recv(n - len(b))
        if not c:
            raise ConnectionError(f"closed after {len(b)}/{n} bytes")
        b += c
    return b


class Session:
    def __init__(self, host, port):
        seed = KEYFILE.read_bytes()
        self.s = socket.create_connection((host, port), timeout=60)
        self.s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        hello = recv_exact(self.s, 49)
        if hello[0] != 1:
            raise SystemExit(f"unknown protocol version {hello[0]}")
        chal, tt_pub = hello[1:17], hello[17:49]
        f_sk = X25519PrivateKey.generate()
        f_pub = f_sk.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw)
        sig = Ed25519PrivateKey.from_private_bytes(seed).sign(chal + tt_pub + f_pub + b"ttkbd1")
        self.s.sendall(f_pub + sig)
        shared = f_sk.exchange(X25519PublicKey.from_public_bytes(tt_pub))
        self.key = hashlib.blake2b(shared + chal + tt_pub + f_pub, digest_size=32).digest()
        self.ctr = 0
        self.last = time.monotonic()

    def send(self, sc, flags):
        self.s.sendall(xchacha_lock(self.key, nonce(self.ctr), bytes([sc, flags])))
        self.ctr += 1
        self.last = time.monotonic()

    def heartbeat_if_due(self):
        if time.monotonic() - self.last >= 1.0:
            self.send(0, F_HEARTBEAT)

    def close(self):
        self.s.close()


class SerialLink:
    """Raw serial transport (decision D4): 3-byte frames {0xA5, scancode, flags} on the null-modem cable
    to the TT's Modem 2 at 38400 — no TCP, no handshake, no crypto; the cable is the trust boundary.
    Same send / heartbeat_if_due / close interface as Session."""

    SYNC = 0xA5

    def __init__(self, dev):
        import termios
        import tty
        self.fd = os.open(dev, os.O_RDWR | os.O_NOCTTY)
        tty.setraw(self.fd)
        attr = termios.tcgetattr(self.fd)
        attr[2] |= termios.CLOCAL | termios.CREAD
        attr[2] &= ~termios.CRTSCTS
        attr[4] = attr[5] = termios.B38400
        termios.tcsetattr(self.fd, termios.TCSANOW, attr)
        self.last = time.monotonic()

    def send(self, sc, flags):
        os.write(self.fd, bytes([self.SYNC, sc, flags]))
        self.last = time.monotonic()

    def heartbeat_if_due(self):
        if time.monotonic() - self.last >= 1.0:
            self.send(0, F_HEARTBEAT)

    def close(self):
        os.close(self.fd)


# Linux evdev keycode -> Atari scancode (FR-5). Linux 1-68 are PC set-1 codes, which the Atari IKBD
# shares for the main block and F1-F10; the rest differ and are listed explicitly.
EV2ST = {c: c for c in range(1, 69)}
EV2ST.update({
    97: 0x1D, 100: 0x38,                       # Right Ctrl/Alt -> the TT's single Control/Alternate
    103: 0x48, 108: 0x50, 105: 0x4B, 106: 0x4D,  # cursor up/down/left/right
    102: 0x47, 110: 0x52, 111: 0x53,           # Home -> Clr/Home, Insert, Delete
    87: 0x61, 88: 0x62, 138: 0x62,             # F11 -> Undo, F12 -> Help, KEY_HELP -> Help
    86: 0x60,                                  # UK 102nd key (\ |)
    71: 0x67, 72: 0x68, 73: 0x69,              # keypad 7 8 9
    75: 0x6A, 76: 0x6B, 77: 0x6C,              # keypad 4 5 6
    79: 0x6D, 80: 0x6E, 81: 0x6F,              # keypad 1 2 3
    82: 0x70, 83: 0x71, 96: 0x72,              # keypad 0 . Enter
    98: 0x65, 55: 0x66, 74: 0x4A, 78: 0x4E,    # keypad / * - +
})
RELEASE_CHORD = {97, 100, 1}                   # Right Ctrl + Right Alt + Esc


class KeyPump:
    """Turns evdev (code, value) events into TT frames. value: 1 down, 0 up, 2 autorepeat — sent as
    another make. ttkbdd turns the TT's own repeat off for the session: it fired on every late break
    and doubled keys (NFR-2, 2026-09-23), so repeat timing now comes from fractal."""

    def __init__(self, send):
        self.send, self.down = send, set()

    def event(self, code, value):
        """-> False when the release chord was pressed (caller ends the session)."""
        if value == 2:
            sc = EV2ST.get(code)
            if sc is not None:
                self.send(sc, 0)
            return True
        if value == 1:
            self.down.add(code)
            if RELEASE_CHORD <= self.down:
                return False
        else:
            self.down.discard(code)
        sc = EV2ST.get(code)
        if sc is not None:
            self.send(sc, 0 if value == 1 else F_BREAK)
        return True


def grab(host, port, device, serial=None):
    import select
    import evdev  # python3-evdev (system python on fractal)
    dev = evdev.InputDevice(device)
    sess = SerialLink(serial) if serial else Session(host, port)
    dev.grab()  # keys stop typing on fractal while the session runs
    pump = KeyPump(sess.send)
    print("ttkbd_send: typing on the TT — Right Ctrl + Right Alt + Esc to stop", file=sys.stderr)
    try:
        while True:
            r, _, _ = select.select([dev.fd], [], [], 0.5)
            if r:
                for ev in dev.read():
                    if ev.type == evdev.ecodes.EV_KEY and not pump.event(ev.code, ev.value):
                        return
            sess.heartbeat_if_due()
    finally:
        dev.ungrab()
        if serial:
            sess.send(0, F_QUIT)  # ttkbdd exits; ttygetty respawns the serial console
        sess.close()  # the TT releases every held key when the session closes (FR-6)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["keygen", "type", "frames", "grab"])
    ap.add_argument("args", nargs="*")
    ap.add_argument("--host", default="192.168.0.30")  # sam.int-exception: the TT has no DNS record
    ap.add_argument("--port", type=int, default=7590)
    ap.add_argument("--hold", type=float, default=0.0)
    ap.add_argument("--delay", type=float, default=0.0, help="seconds between frames")
    ap.add_argument("--device", default="/dev/input/by-id/usb-05ac_KB104_Dongle-event-kbd")
    ap.add_argument("--serial", metavar="DEV", help="raw serial to the TT's Modem 2 instead of WiFi (D4), e.g. /dev/atari-tt")
    a = ap.parse_args()
    if a.cmd == "keygen":
        return keygen()
    if a.cmd == "grab":
        return grab(a.host, a.port, a.device, a.serial)
    frames = text_frames(" ".join(a.args).replace("\\n", "\n")) if a.cmd == "type" else \
        [(int(h, 16) & 0xFF, int(h, 16) >> 8) for h in a.args]
    t0 = time.monotonic()
    sess = SerialLink(a.serial) if a.serial else Session(a.host, a.port)
    print(f"ttkbd_send: session up in {time.monotonic() - t0:.1f} s", file=sys.stderr)
    for sc, fl in frames:
        sess.send(sc, fl)
        if a.delay:
            time.sleep(a.delay)
    end = time.monotonic() + a.hold
    while time.monotonic() < end:
        time.sleep(0.2)
        sess.heartbeat_if_due()
    sess.close()


if __name__ == "__main__":
    main()
