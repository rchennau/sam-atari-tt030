# SAM ROM TT (`sam-rom-tt`) — custom TT030 firmware

> **Status: ALPHA — runs in the emulator, untried on hardware.** `rom0` boots in Hatari as a TT
> ROM, executes, and prints over the MFP serial port. No chip has been burned; the roadmap below
> is unstarted.

A bare-metal firmware for the Atari TT030's four socketed ROM chips: no TOS, no EmuTOS, no GEMDOS,
BIOS, VDI or AES. Operator goal (2026-09-17): a completely custom ROM, chiefly to escape the RAM
test that costs TOS 3.06 ~80 s of its ~96 s boot (measured in Hatari, `../docs/fr8-os-evaluation.md`).

Nothing that exists on the TT today runs under this: XBOOT, CBHD, FreeMiNT and every GEM app need
TOS. Design discussion and trade-offs: A-Mem note `a822618e`, board card `56a25f99`.

## ROM budget and layout

- 512 KB = 4 × 128 KB, **one chip per byte lane** of the 68030's 32-bit bus: chip N holds bytes
  N, N+4, N+8 … Use `tools/interleave.py` to split and join.
- The limit is the TT's decode window `0xE00000`–`0xE7FFFF`, **not** the chips. Larger 32-pin
  EPROMs fit physically but the TT holds their top address lines fixed (and they must not float).
  More space: the cartridge port at `0xFA0000` (+128 KB, no soldering), a jumper bank select (keeps
  a rescue image in bank 0), or software banking (board modification).
- Header, read from the real `tos306uk.img`: longword 0 = `BRA` + OS version, longword 1 = reset PC
  (`0x00E00030`), longword 2 = ROM base, longword 3 = end of OS. EmuTOS 1.4 uses the same layout.

## Burn safely — the interleave must be proven first

1. Dump all four original chips. **Keep the dumps and the chips.**
2. `python3 tools/interleave.py join <prefix> rejoined.img`
3. `cmp rejoined.img ../emulator/roms/tos306uk.img`

A byte-exact match proves both the lane order and the reads. If it fails, the lanes are in a
different order — try the permutations. Nothing gets burned before this passes.

## Two traps this firmware has already hit

- **Anything writable must be at a fixed RAM address.** The ROM is linked as one `.text` blob, so a
  `static` buffer lands in ROM and the first write bus-errors (`Bus Error writing at address
  $e00fd8`). `tt.h` puts scratch at `RAM_BUF` (`0x1000`); the stack is at `0x800` and grows down.
- **`m68k-atari-mint-gcc` prefixes C symbols with `_`** — the asm stub calls `_cmain`.

## Hardware register facts

The TT's SCSI addresses and phase encoding come from **EmuTOS 1.4 (GPL), `bios/scsi.c`** — the TT
SCSI DMA at `0xFFFF8700`, the 5380 at `0xFFFF8780` on odd bytes, phase = `(bus_status >> 2) & 7`.
Those are hardware facts; the firmware code here is its own. EmuTOS's source also warns that
Hatari's TT emulation "emulates their presence but does not emulate the actual SCSI chip" — that
was true of Hatari 2.1 and is **not** true of 2.5, where `rom3` completes a real INQUIRY and
FreeMiNT boots from `--scsi` images.

## Build

```bash
make rom      # build/rom0.img, 512 KB, 0xFF padded — serial hello
make rom1     # build/rom1.img — RAM sizing over serial (make hatari1 to run it)
make rom2     # build/rom2.img — caches, FPU probe, memctrl readout, RAM sizing (make hatari2)
make rom3     # build/rom3.img — SCSI INQUIRY scan over the 5380 in PIO (make hatari3)
make rom4     # build/rom4.img — partition table + FAT16 root directory listing (make hatari4)
make split    # build/rom0.chip0 .. chip3  (one per socket)
make hatari   # boot in the emulator and print the serial output
make debug    # prove execution via breakpoints (no emulated UART needed)
```

## What is proven, and what is not

| Claim | State |
|---|---|
| Assembles with the repo's `m68k-atari-mint-gcc` | proven 2026-09-17 |
| Hatari loads a non-TOS ROM (so emulator development is possible) | proven — no version rejection |
| `split` → `join` round-trips byte-identically | proven |
| The code executes on reset | proven 2026-09-18 — breakpoints hit `tx` 30 times (29 chars + terminator) and reach the halt |
| MFP USART serial output works | proven — 29 bytes captured: `SAM-TT custom ROM 0.1 alive\r\n` |
| `rom1` sizes RAM over serial | proven 2026-09-18 — `ST RAM 4 MB` / `TT RAM 64 MB` on the repo's TT profile, in well under a second (TOS spends ~80 s) |
| Bus-error recovery from absent memory | proven — vectors in ROM via VBR; faults at `0x400800` and `0x5000000` caught, probe returns the count |
| `rom2` bring-up line: caches, FPU, memory-controller readout | proven 2026-09-18 — `memctrl 0a`, `cacr 00000100`, `fpu present`, 4 MB / 64 MB |
| `rom3` reaches the disk: SCSI INQUIRY from ROM | proven 2026-09-18 — `target 0  type 00  Hatari  EmulatedHarddisk` via the TT's 5380 in PIO |
| `rom4` reads the card: partitions, FAT16 BPB, root directory | proven 2026-09-18 — C (bootable, 292 MB) / D / E, 8192-byte logical sectors, 31 files + 8 dirs listed |
| Anything on real hardware | **untried** — checklist in [docs/hardware-session.md](docs/hardware-session.md) |

**Hatari traps:** `--ttram` on the command line prints "Automatically enabling 32-bit addressing"
and then blocks before reading its input, which looks like a firmware hang — use a config file with
the TT profile instead (`make hatari1` does). And serial capture needs `--rs232-in` as well as `--rs232-out`. Without an input
device Hatari fails to open its default `/dev/modem`, disables RS232 and writes an empty file with
no error on stdout — which looks exactly like firmware that never ran. `make hatari` passes both;
`make debug` proves execution with breakpoints and no UART at all.

Next step: RAM sizing (skipping TOS's slow test), then SCSI + FAT16 — roadmap below.

## Roadmap (nothing started)

1. ROM monitor: serial console, memory peek/poke, SCSI read, load-and-run from the BlueSCSI.
2. Bring-up order: reset vectors ✅ → RAM sizing ✅ (`rom1`) → caches/FPU ✅ (`rom2`) → SCSI ✅
   (`rom3`) → block reads + FAT16 directory ✅ (`rom4`) → FAT chain walk → load and run a file. **`rom2` reads the memory controller but deliberately does not write it:**
   the right value is hardware-dependent and a wrong write can make ST RAM unreadable, so the real
   machine's value (checklist step 5) is the reference before that step is written.
   **Open question for hardware:** CACR was written `00000101` and reads back `00000100` in Hatari —
   the data cache took, the instruction-cache bit did not. Check whether a real 68030 keeps it.
3. ATW800/2 **at boot and display level only**: carry the T425 image in ROM (`xserv2.btl` is 22 KB),
   boot it early, use its framebuffer (`0xFEC00000`, RGB565, 1024×768) as the console. Hatari does
   not emulate the ATW, so those paths are hardware-only. Firmware crypto on the T425 waits for
   `sam-yum-tt`'s link measurement (card `7d9cb80d`).
4. Possible end states: ROM monitor · a loader for Linux/m68k · purpose-built SAM-node firmware.
   FreeMiNT and Helios do **not** fit (`mint030.prg` 371 KB + `xaaes.km` 526 KB vs 512 KB total).

## Blocked on the operator

- Part number of the blank EPROMs (capacity, and which pins the TT holds fixed).
- Whether a UV eraser is available — it sets the iteration cost.
- Plan constraint **C1** (`atari-tt030-enhancement`) forbids ROM flashing; a burn needs it amended.
