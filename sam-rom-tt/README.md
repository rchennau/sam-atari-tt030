# SAM ROM TT (`sam-rom-tt`) — custom TT030 firmware

> **Status: ALPHA SKELETON — nothing here is proven to run.** It assembles, and Hatari loads it
> without rejecting it. Whether the code executes is **unverified**. Do not burn a chip from this.

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

## Build

```bash
make rom      # build/rom0.img, 512 KB, 0xFF padded
make split    # build/rom0.chip0 .. chip3  (one per socket)
make hatari   # boot it in the emulator
```

## What is proven, and what is not

| Claim | State |
|---|---|
| Assembles with the repo's `m68k-atari-mint-gcc` | proven 2026-09-17 |
| Hatari loads a non-TOS ROM (so emulator development is possible) | proven — no version rejection |
| `split` → `join` round-trips byte-identically | proven |
| The code executes on reset | **unproven** |
| MFP USART serial output works | **unproven** — `--rs232-out` captured nothing, twice |
| Anything on real hardware | **untried** |

Next step: Hatari's debugger (`--parse`, breakpoint at `0xE00030`) to see whether the entry is
reached, then Hatari native features as a print channel that needs no emulated UART.

## Roadmap (nothing started)

1. ROM monitor: serial console, memory peek/poke, SCSI read, load-and-run from the BlueSCSI.
2. Bring-up order: reset vectors → RAM sizing (skipping the slow test) → caches/FPU → console →
   NCR 5380 SCSI + FAT16.
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
