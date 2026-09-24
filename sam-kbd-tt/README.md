# SAM KBD TT (`sam-kbd-tt`) — fractal's keyboard and mouse as the Atari TT030's own

> **Status: keyboard and mouse working on the real TT, operator-confirmed (2026-09-23); ttkbdd starts at
> boot; about 1 key in 8,100 lost at fast typing (target ≤ 1 in 5,000).** Keys typed on fractal reach XaAES, TeraDesk and
> TosWin2 exactly as if typed on the TT's own keyboard — including Alt, Control, F-keys and the space bar,
> which the TT's physical keyboard cannot send (space and M are dead, Alt and Left Control unusable) — and
> fractal's mouse drives the TT pointer. Moving the TT pointer into its top-right corner hands control
> back. The attached keyboard and mouse keep working alongside.

`sam-kbd-tt` is the plan `tt030-remote-keyboard` in the SAM monorepo
(`maestro/tracks/tt030-remote-keyboard/`, approved 2026-09-23, decisions D1 direct TCP, D2 swap the key
table while connected, D3 encrypt every frame). A daemon on the TT, `ttkbdd`, accepts one signed,
encrypted session at a time on `192.168.0.30:7590` and feeds each key into FreeMiNT's own keyboard
path; `ttkbd_send.py` on fractal captures the keyboard and streams it.

| Part | What it does |
| :--- | :--- |
| [`../src/ttkbdd.c`](../src/ttkbdd.c) | TT daemon: WiFi listener or raw serial (`-S` on stdin, `-s DEV`, `-v` hex dump), handshake, XChaCha20-Poly1305 frames, inject via the kbdvec slot, key-table swap, key-repeat off for the session, release on end / signal / 3 s silence; `--release` recovery; `-f FILE` replay; `--bench N` |
| [`../scripts/ttkbd-session.sh`](../scripts/ttkbd-session.sh) | one-command session on fractal: serial (default) or `--wifi`, keyboard + mouse; restores the console on the stop chord or TT hot corner |
| [`../scripts/ttkbd_send.py`](../scripts/ttkbd_send.py) | fractal sender: `keygen`, `grab` (evdev, exclusive), `type "text"`, `frames HEX…`, `latency N`; Linux keycode → Atari scancode map |
| [`../scripts/test_ttkbd_send.py`](../scripts/test_ttkbd_send.py) | checks the map against the TT's own `en_uk.tbl` and the key-pump rules |
| [`../src/kbdinj.c`](../src/kbdinj.c) | Phase 0 spike and test tool: inject raw scancodes, read the BIOS queue back (`-r`), swap the table (`-k`) |
| `../staging/HD10_OVERLAY/etc/rc.ttkbdd` | boot script: `ttkbdd --release`, then the WiFi listener — **installed on the card 2026-09-23**, verified by a reboot |
| iac/fractal `70-atari-tt-kbd-uaccess.rules` (SAM monorepo) | gives the desktop user of fractal read access to the KB104 keyboard, nothing else |

## How keys get in

FreeMiNT 1.19's ACIA handler jumps to an undocumented vector at `Kbdvbase()-4` for every keyboard
byte; the kernel chains `kbdvec_handler` there, which calls `ikbd_scan(scancode, kbd_iorec)`
(`sys/arch/acia.S:149`, `sys/arch/intr.S:637`, `sys/keyboard.c:952`). `ttkbdd` calls that vector in
supervisor mode with interrupts masked, so an injected key takes exactly the hardware key's path:
`keyboard.tbl`, Shift/Control/Alternate state, both keyboards merged. Keys are released on session
end; `Keytbl()` writes are ignored under MiNT, so the table swap uses `Ssystem(S_LOADKBD)`.

## Wire

```
TT -> fractal   ver(1)=3 | challenge(16) | TT X25519 public key(32)
fractal -> TT   fractal X25519 public key(32) | Ed25519 signature(64) over challenge | both keys | "ttkbd1"
key = BLAKE2b-256(X25519 shared | challenge | TT key | fractal key)
fractal -> TT   mac(16) | ciphertext(4)   XChaCha20-Poly1305, nonce = 16 zero bytes | 64-bit counter
                plaintext {flags, code, dx, dy}: bit0 key up, bit1 heartbeat, bit3 quit (serial),
                bit4 mouse (code = buttons), bit5 ack wanted
TT -> fractal   0x06 ack (only when bit5 is set) · 0x05 pointer reached the hot corner
serial          {0xA5, flags, code, dx, dy}, no crypto
```

Protocol v3 keeps the counter in the last 8 nonce bytes, so the TT derives the XChaCha subkey once per
session and each record costs one ChaCha20-Poly1305 (1.47 ms verify vs 2.53 ms for v2, `--bench`).
A forged key types nothing; a tampered, replayed or reordered record ends the session untyped; one
heartbeat a second, and 3 s of silence releases every held key. The TT uses Monocypher 4.0.3 (the copy
Dropbear already uses); fractal uses Python `cryptography` plus a small HChaCha20, cross-checked
against Monocypher byte for byte.

## Results (real TT, 2026-09-23)

| Check | Result |
|---|---|
| Keys reach XaAES / TosWin2 (`MEMPROT:ON`) | `abA` typed, Ctrl+Alt+L opened the task manager |
| Both keyboards at once | 60 injected `x` + 10 physical `o`, interleaved, none lost |
| Key table swap (D2) | remote `;` and `[` type `;` and `[`; the space remap is back afterwards |
| Encrypted session over WiFi | `hello; [world] A1!` typed exactly |
| Forged key / tampered / replayed record | nothing typed / session ended / session ended |
| Release on SIGTERM, `kill -9` + `--release`, 3 s silence | no key left held in any case |
| 2,700 keys at 15 keys/s over WiFi (NFR-2) | shipped pacing: **2,700 · 2,699 · 2,700 — 1 key in 8,100**; target re-scoped to ≤ 1 in 5,000 (operator) ✅ |
| ttkbdd's own CPU (NFR-3) | idle 0 %; WiFi **10.2 % at 10 keys/s** (v3 + opt-in acks; limit 10 %); serial **9.3 %** |
| Key latency (NFR-1), send → TT ack after inject, 50 Shift taps | **WiFi median 32.4 ms, p95 65.2 ms** (ping avg ≈ 95 ms same session) · **serial median 6.0 ms, p95 7.0 ms** |
| Connect | 4–10 s wait; the handshake costs **12.6 s of 68030 CPU** (X25519 + Ed25519) |
| Per frame (`--bench`) | decrypt 2.5 ms, `Supexec` inject 0.65 ms, a 5 ms `usleep` 5.2 ms CPU (it rounds up to 20 ms wall) |

What the measurements changed:

- **Lost keys** came from bursts: `ikbd_scan()` queues into a 16-entry ring drained once per 5 ms tick
  and drops silently when full, and the application's own key buffer overflows behind it. `ttkbdd`
  injects at most one frame per 20 ms (25 keys/s), sleeping only for what is left of the 20 ms and
  re-reading the clock afterwards.
- **Doubled keys** came from the TT's own key repeat firing whenever a key-up arrived late. The session
  sets `Kbrate` delay to 255 (5.1 s) and fractal sends the repeats; the saved rate comes back after.
- **CPU vs loss trade-off (four pacing variants, 2,700-key run / ttkbdd CPU at 10 keys/s):** fixed 20 ms
  wait per frame 2,699 of 2,699 sent ✓ / 23.8 %; wait only in bursts 2,600 (scrambled backlog) / 9.1 %; 20 ms cap
  assuming the sleep was exact 2,693 / 9.8 %; **20 ms cap re-reading the clock (shipped) 2,700 and
  2,699 / 10.8–11.2 %**. Later: per-frame acks (added for NFR-1) pushed it to 12.5 %; protocol v3 and
  acks only on request brought it to **10.2 %** (two runs) — 0.2 points over the limit on WiFi.
- Whole-system CPU while typing is dominated by TosWin2 drawing (≈ 45 % at 15 keys/s even with no
  network), the same as typing on the TT's own keyboard — hence NFR-3 measures ttkbdd's own time.
- An unpaced replay of 5,400 frames (≈ 400 keys/s) garbles; no sender produces that rate.

## Mouse (FR-8…11, added 2026-09-23)

fractal's Logitech mouse drives the TT pointer through `Kbdvbase()->mousevec`, the path FreeMiNT's own
mouse emulation uses, on both transports. Protocol v2 frames are `{flags, code, dx, dy}` (flag 0x10 =
mouse, code = buttons 1 right / 2 left). Motion is scaled (`--mouse-scale`, default 4 counts per TT unit;
the TT moves 4 px per unit), coalesced to one frame per 20 ms and split into ±127 steps.
**Hand-back:** moving the TT pointer into its **top-right corner** makes ttkbdd send `0x05`; the sender
ungrabs and ends the session (serial: the console returns). `ttkbdd -c N` picks the corner (0 = off).
`ttkbd-session.sh` includes the mouse when it is readable (`TTKBD_NO_MOUSE=1` for keyboard only).

| Check (real TT, 2026-09-23) | WiFi | Serial |
|---|---|---|
| Drive to top-left, then +25,+25 / +50,+30 units | 0,0 → 100,100 | 0,0 → 200,120 |
| Pointer into the top-right corner | 1023,0, `0x05` received | `0x05` received |
| Keys still acked | yes | — |

**Live run with the real mouse: works (operator, 2026-09-23)**, after re-installing the uaccess rule, which
now covers the Logitech receiver. Not yet: a separate button click / release check (FR-11), the mouse's
CPU cost on the TT, and a hot corner on **fractal's** screen to *take* control (COSMIC is Wayland: a
client cannot see the global pointer; `ttkbd-session.sh` starts a session for now).

## Raw serial transport (decision D4, 2026-09-23)

No TCP/IP: `ttkbdd` reads 3-byte frames `{0xA5, scancode, flags}` straight off the null-modem cable on
Modem 2 at 38400 and injects them the same way; no handshake, no crypto (the cable is the trust
boundary). fractal: `ttkbd_send.py --serial /dev/atari-tt type|frames|grab`.

**Start it from the serial console itself:** at the console's bash prompt, `exec /usr/sbin/ttkbdd -S`
(`scripts/ttkbd-session.sh` does this for you). It takes over the console's line; when it exits, `ttygetty` respawns the
console bash. Measured on the real TT: that works; starting `ttkbdd -s /dev/ttyS1` from an SSH session
does **not** — the port is the console bash's controlling terminal, so any other process that reads it
is stopped by SIGTTIN (state T) and reads nothing. Six stopped shells found and cleaned up after the
tests. A forced `TIOCSCTTY` would be the fix for `-s`; untested.

| Check (serial) | Result |
|---|---|
| ttkbdd CPU at 10 keys/s | **9.32 %** (no handshake, no decrypt) |
| 600 keys at 10 keys/s | byte-identical |
| 2,700 keys at 15 keys/s | 2,690 / 2,700 — keys still lost, as on WiFi; cause not found |
| Cost | Modem 2 is not the console while it runs |

## Layout

```
src/ttkbdd.c              the TT daemon
src/kbdinj.c              raw-scancode test tool
scripts/ttkbd_send.py     fractal sender
scripts/test_ttkbd_send.py
staging/HD10_OVERLAY/etc/rc.ttkbdd, etc/ttkbd.pub, usr/sbin/ttkbdd   boot install (on the card since 2026-09-23)
tools/monocypher-4.0.3/   crypto (gitignored toolchain dir)
tools/freemint/           FreeMiNT source at the TT's snapshot 4eb44d14, for reference (gitignored)
```

## Build

```bash
M=tools/monocypher-4.0.3/src
tools/cross-mint/usr/bin/m68k-atari-mint-gcc -m68020-60 -O2 -o build/ttkbdd.prg src/ttkbdd.c \
    $M/monocypher.c $M/optional/monocypher-ed25519.c -I$M -I$M/optional
tools/cross-mint/usr/bin/m68k-atari-mint-gcc -m68020-60 -O2 -o build/kbdinj.prg src/kbdinj.c
/usr/bin/python3 -m pytest scripts/test_ttkbd_send.py     # needs python3-evdev (system python)
```

## Run

**One command (on fractal, from your desktop session):** `scripts/ttkbd-session.sh` — raw serial, no key
needed, works with WiFi down. It types `exec ttkbdd -S` at the TT's Modem 2 console, grabs the keyboard,
and on **Right Ctrl + Right Alt + Esc** sends a quit frame: ttkbdd exits, `ttygetty` respawns the console,
and the script checks it answers. `scripts/ttkbd-session.sh --wifi` uses the encrypted WiFi path instead
(needs your key on the TT and ttkbdd listening; the console is not touched).

**Back to console mode:** the stop chord does it. If the script died mid-session, by hand, in order:
`ttkbd_send.py --serial /dev/atari-tt frames 800` (quit frame) · over WiFi kill ttkbdd · then
`ttkbdd --release` if a key or the key table is stuck. The same list is at the end of the script.

Manual steps, if you are not using the script:


1. **Key (once, on fractal, as the user who will type):** `python3 scripts/ttkbd_send.py keygen` writes
   `~/.config/atari-tt/ttkbd.key` (mode 600) and prints the public key; write its 32 raw bytes to the
   TT's `/etc/ttkbd.pub`. The TT accepts exactly one key.
2. **TT:** the WiFi listener starts at boot (`/etc/rc.ttkbdd` from `mint.cnf`; log `/var/log/ttkbdd.log`).
3. **fractal, from the desktop session:** `python3 scripts/ttkbd_send.py grab`. Keys stop typing on
   fractal and type on the TT. **Right Ctrl + Right Alt + Esc** stops.
4. Scripted input: `ttkbd_send.py type "text\n"`; raw frames: `ttkbd_send.py frames 2a 1e 11e 12a`.
5. **Stuck key or wrong table after a crash:** on the TT, `ttkbdd --release` (breaks every scancode,
   restores the space-patched table and the boot key-repeat rate `0x0f02`).

Key map notes: Linux keys 1–68 map 1:1; Right Ctrl/Alt → the TT's single Control/Alternate; Home →
Clr/Home; F11 → Undo; F12 → Help; the UK 102nd key → `\`; keypad mapped to the TT keypad.

## Hardware

- Atari TT030, FreeMiNT 1.19 (`4eb44d14`) with XaAES, BlueSCSI DaynaPORT WiFi at 192.168.0.30
- fractal: KB104 keyboard (`/dev/input/by-id/usb-05ac_KB104_Dongle-event-kbd`), readable through the
  uaccess rule
