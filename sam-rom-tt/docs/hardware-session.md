# Real-TT session checklist (operator, planned Saturday 2026-09-19)

Everything here needs the TT powered on. Ordered so the cheap measurements come first and nothing
later depends on an earlier step succeeding. Emulator-side values to compare against are quoted.

## 0. Before you start

- Serial cable on **Modem 2**, `picocom -b 38400 /dev/atari-tt` from fractal (57600 is refused by
  FreeMiNT's `scc.xdd`). Log the session: `picocom -b 38400 --logfile ~/tt-session.log /dev/atari-tt`.
- Files to copy to the TT first (FTP as usual, from the side repo):
  `sam-yum-tt/build/shabench.ttp`, `sam-yum-tt/build/shaserv.btl`, `build/vdibench.prg`.

## 1. NFR-1 — boot time (stopwatch, ~5 min)

| Measure | Emulator reference |
|---|---|
| Power-on → TOS 3.06 desktop, plain set | Hatari ~96–100 s, of which ~80 s is the RAM test |
| Power-on → FreeMiNT/XaAES desktop, MINT set | Hatari ~104 s |

Record both. The target is ≤ +5 s over stock TOS. **The interesting number is how much of it is the
RAM test** — that is what the custom firmware removes.

## 2. FR-8 — the real-TT column (~10 min)

Run `vdibench.prg` from the desktop under each boot set; it appends to `VDIBENCH.TXT` beside itself.

| Config | Hatari reference (640×480×16): lines/s, bars/s, text/s, blits/s |
|---|---|
| TOS 3.06 | 985 / 552 / 172 / 192 |
| TOS 3.06 + `EMUTOS.PRG` (RAM-loaded EmuTOS) | 426 / 363 / 161 / 111 |
| ATW800/2 set (xVDI) | **no emulator reference — Hatari cannot run it** |

The xVDI row is the one Hatari cannot produce, so it is the row worth the trip.

## 3. `shabench` — the T425 decision (~10 min)

```
fpgabios.tos
shabench SHASERV.BTL 512
```

Reports three numbers: 68030 SHA-256, link echo (both ways), and T425 SHA-256 including transfer,
plus whether the two digests agree.

- Hatari reference for the 68030: **137 KB/s** (emulated timing, not authoritative).
- WiFi download runs at ~115 KB/s, so if the 68030 is near that, hashing roughly doubles a package
  fetch — that is what makes offloading worth considering.
- **Decision rule:** propose a native T425-assisted `yum` only if the T425 number, transfer
  included, clearly beats the 68030. Otherwise the planned `sh` + `awk` client stands.
- This resets the T425, so the `sam-ssh-tt` offload server stops; the next SSH login re-boots it.

## 4. ROM chips — dump and prove the interleave (~20 min, no burning)

1. Read all four chips with the burner. Keep the dumps **and** label the chips by socket.
2. `python3 sam-rom-tt/tools/interleave.py join <prefix> rejoined.img`
3. `cmp rejoined.img emulator/roms/tos306uk.img`

A byte-exact match proves the lane order and that the reads are good. If it fails, try the other
orderings — the tool is deliberately trivial to re-run. **Nothing is burned until this passes.**
Also note the part number, speed suffix and voltage marking on the original chips, and on the
blanks; that decides what can be burned and whether banking is possible.

## 5. `rom2` readouts to compare (only if step 4 passed and C1 is amended)

`rom2` prints these on the emulator; the real machine should be diffed against them **before**
trusting any firmware that writes hardware:

```
memctrl 0a          <- Hatari's TT profile. The real TT's value is the reference for
                       programming the memory controller, which rom2 deliberately does NOT do.
cacr    00000100    <- written 00000101; the instruction-cache bit did not stick in Hatari.
                       If the real 68030 reads back 00000101, that was an emulator limitation.
fpu     present
ST RAM  4 MB
TT RAM  64 MB       <- must match the machine's real fit
```

A mismatch on `memctrl` or `ST RAM` is a stop sign: it means the firmware's assumptions about this
machine are wrong, and the next firmware step (programming the controller) would be guesswork.

## Not in this session

`rom0`–`rom4` cannot run on the TT until a chip is burned, and step 4 must pass first. They are
emulator-only for now; the hardware questions they raise are listed in step 5 (`memctrl`, CACR's
instruction-cache bit) plus one more: `rom4`'s partition and FAT16 numbers come from a *copy* of the
card image, so on hardware they should match what the live card reports — a mismatch means the
work copies have drifted from the card, which matters to every other TT task too.

## Afterwards

Record the numbers in `maestro/tracks/atari-tt030-enhancement/progress.md` (NFR-1, FR-8) and
`maestro/tracks/tt030-rpm-pipeline/progress.md` (shabench), and close board cards `7d9cb80d`
(shabench) and, if the dump matched, note the interleave result on `56a25f99`.
