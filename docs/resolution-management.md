# Resolution management on the Atari TT030 + ATW800/2

Measured 2026-10-15 on the real TT (FreeMiNT 1.19 + XaAES, XBOOT set MINT_ATW, xVDI 68000 build of Mar 17 2026,
XVDIMENU v1.24) with an **ASUS VS247H-P** monitor (23.6", native 1920×1080; accepts 30–80 kHz H, 55–75 Hz V).
Track `tt030-atw800-video-modes` (SAM repo `maestro/tracks/`).

## How a mode is chosen

- The ATW800/2's modes live in `C:\AUTO\XVDI.BIB`: an array of 86-byte records (name, depth, bytes/line, visible and
  virtual size, refresh, horizontal/vertical front porch, sync, back porch and display, PLL dividers, control word).
  The record layout and the PLL are documented in the vendor's *ATW800/2 Programmer's Manual*.
- `C:\AUTO\XVDIMENU.INF` word 0 picks the boot mode, **0-based** (word 14 = mode 15). XVDIMENU shows a short timed
  menu at boot and applies the choice **at boot only**: running it on the live desktop garbles the screen, so every
  resolution change is a reboot.
- Pixel clock = 27 MHz × feedback divider ÷ input divider ÷ 5 (the FPGA PLL runs at 5× for the HDMI serialiser).
- XBOOT boots the last set used (`C:\XBOOT\XBOOT.CFG`).

## Tools (this repo)

| Tool | What it does |
| :--- | :--- |
| `scripts/atw_modes.py` | decode `XVDI.BIB` → `docs/atw-modes.tsv` (size, depth, VRAM, 2 MB fit); `--md` → the timing matrix [`xvdi-mode-matrix.md`](xvdi-mode-matrix.md) (joined with `docs/atw-mode-status.tsv`) |
| `scripts/atw_mode.sh show\|set <mode>\|restore` | set the next boot's mode from the remote system management host; backup + SHA-256-verified upload over the serial line |
| `scripts/atw_moderun.sh MODE...` | per mode: set, reboot, check size (`atwres`), XaAES log and `xVDI`/`EdDI` cookies, then `vdibench` ×3 over serial |
| `src/atwbg/atwres.c` | prints the live `W H PLANES` from the Line-A variables |
| `src/vdibench.c` | VDI lines / filled bars / text / blits per second |

## Results

Benchmarks are medians of 3 `vdibench` runs. Pixel clock, H and V rates are computed from the record's dividers and
timing. "T425 load" = one SSH login (X25519 + Ed25519 verify through `ttjobd`) and, for mode 22, three `tgzip` jobs,
watched by the operator. Modes 24 and 28 (FR-6, 2026-10-17): two runs each of one login + three `tgzip` jobs, all
on the T425, 0 blanks per run.

| Mode | Size | Colours | VRAM KB | Pixel clock MHz | H kHz | V Hz | ASUS VS247H-P | lines/s | bars/s | text/s | blits/s | T425 load |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 34 | 1920×1200 | 2 | 281 | 129.6 | 50.0 | 40.3 | OUT OF RANGE | 417 | 806 | 675 | 268 | — |
| 10 | 800×600 | 256 | 469 | 40.0 | 37.8 | 60.3 | shown | 1342 | 2198 | 710 | 1429 | — |
| 14 | 1024×768 | 256 | 768 | 64.8 | 48.2 | 59.8 | shown | 1110 | 2151 | 708 | 1429 | — |
| 18 | 1280×720 | 256 | 900 | 75.6 | 45.8 | 61.1 | shown | 1043 | 2222 | 739 | 1429 | — |
| 11 | 800×600 | 65536 | 938 | 40.0 | 37.8 | 60.3 | shown | 1336 | 1887 | 495 | 769 | — |
| 21 | 1280×800 | 256 | 1000 | 82.8 | 49.3 | 59.5 | shown | 995 | 2128 | 710 | 1429 | — |
| 24 | 1280×960 | 256 | 1200 | 102.6 | 59.9 | 60.3 | shown | 904 | 2105 | 709 | 1379 | stable |
| 26 | 1280×1024 | 256 | 1280 | 108.0 | 64.0 | 60.0 | shown | 834 | 1802 | 678 | 1379 | blanks |
| 28 | 1440×960 | 256 | 1350 | 102.6 | 53.9 | 54.2 | shown | 887 | 2083 | 714 | 1290 | stable |
| 15 | 1024×768 | 65536 | 1536 | 64.8 | 48.2 | 59.8 | shown | 1117 | 1980 | 486 | 714 | stable |
| 30 | 1600×1200 | 256 | 1875 | 135.0 | 62.5 | 50.0 | OUT OF RANGE | 754 | 2128 | 705 | 1290 | — |
| 22 | 1280×800 | 65536 | 2000 | 82.8 | 49.3 | 59.5 | shown | 993 | 1786 | 474 | 656 | stable |
| 32 | 1920×1080 | 256 | 2025 | 137.7 | 66.2 | 59.6 | shown | 728 | 2020 | 713 | 1212 | blanks |
| 37 | 1920×1080 | 2 | 253 | 137.7 | 66.2 | 59.6 | shown | 427 | 816 | 671 | 250 | — |
| 38 | 1600×1200 | 256 | 1875 | 129.6 | 73.6 | 59.8 | OUT OF RANGE | 753 | 2041 | 717 | 1250 | — |

Mode 39 (**boot default from 2026-10-17 ~16:55 PDT to 2026-10-20, when mode 37 replaced it; the operator confirmed a normal desktop; `XVDI.BIB` d238c3ac on the card, previous table deb47fbb in `~/.local/state/tt030-deploy/xvdi-1664/card.bib.orig`**): 1664×936×8 (16:9) on mode 32's reduced-blanking timing at the
proven-stable 102.6 MHz clock (PLL 19/1): htotal 1824, vtotal 967 → 56.25 kHz, 58.2 Hz, 1521 KB. The goal is mode 32's
aspect with offload that does not blank. Built from the live table (md5 deb47fbb) → `XVDI.BIB` md5 d238c3ac
(`~/.local/state/tt030-deploy/xvdi-1664/`). Width is a multiple of 32 like every vendor mode.

Modes 37 and 38 are **custom** records appended to `XVDI.BIB` (37 = mode 32's timing with 1-bit depth; 38 = 1600×1200
at 60 Hz with reduced blanking). Vendor records 31 and 35 are labelled 50/60 Hz but are programmed for 40/30 Hz.
Mode 28 computes to 54.2 Hz, just under the monitor's stated 55 Hz, and still displayed.

## Findings

1. **Monitor, not card, limits the list.** Every mode booted correctly (size, desktop, cookies). The ASUS rejects
   below 55 Hz (modes 34 at 40 Hz, 30 at 50 Hz) and anything taller than 1080 lines even inside its rates (mode 38,
   74 kHz / 60 Hz).
2. **High pixel clock + T425 work = HDMI dropouts.** At 108 MHz (mode 26) and 137.7 MHz (mode 32) the picture goes
   black and back (~6 times per SSH login) whenever the T425 runs a job; quiet otherwise. 65 MHz (mode 15) and
   82.8 MHz (mode 22, same memory traffic as mode 32) stay stable — so the trigger tracks pixel clock, not video
   memory bandwidth. Threshold: between **102.6 MHz** (modes 24 and 28 stable, FR-6 2026-10-17) and 108 MHz (blanks).
3. **Mono is slower than 256 colours** at the same size (mode 37 vs 32): lines 1.7×, bars 2.5×, blits 4.8× slower;
   text about equal. Cause: the driver copies mono with the CPU; the card's copy engine is used only at 8 bits and up.
   The resident `atwmono` hook (atwtools) drives the engine for byte-aligned mono copies: blits 268 → 4444/s, bit-exact
   (track `tt030-atw-display-tuning` NFR-2, 2026-10-20). Request to the vendor: [`vendor-note-atw800-mono.md`](vendor-note-atw800-mono.md).
4. **SSH cost of keeping the T425 idle:** login 16–18 s with offload vs **27.1 s** on the 68030 (X25519 6.3 s,
   Ed25519 sign 2.5 s, verify 9.0 s).

## Chosen configuration (operator, 2026-10-15)

**Current (2026-10-20):** boot mode **37** (1920×1080, 1 bit). `/etc/atw-mono-colours.sh` (atwtools 1.4, `%config`), run from `xaaes.cnf`
after the `atwmono` hook, sets VDI colours 0/1 to black/white (white text on black) when the screen has one plane;
256-colour modes keep `DEFAULT.PAL`. Dropbear offload is on (`DROPBEAR_NO_ATW` commented out since 2026-10-17).
Mode 37 shares mode 32's 137.7 MHz clock; no watched T425 run in mode 37 is recorded. The history follows.

**2026-10-17 update (operator watching):** in mode 32, T425 X25519 jobs, TRAM-pinned X25519 jobs and an SSH login with T425 offload (20 s, against ~28 s on the 68030) did **not** blank the picture. So compute-only jobs are fine, and the blanks came with bulk-data jobs (tgzip). Dropbear offload is now ON in mode 32 (`DROPBEAR_NO_ATW` commented out). Whether 300 KB SHA-256 jobs blank was not answered.

Default **mode 32** (1920×1080, 256 colours). Until 2026-10-17 Dropbear ran with `DROPBEAR_NO_ATW=1` (`/etc/dropbear/dropbear.conf`, dropbear-2026.94-2.sam1, `%config(noreplace)`) so logins
do not touch the T425; the serial console (Modem 2) is preferred for connections in this mode. Other `ttjobd` work
(tgzip, lha, bzip2) still blanks the picture briefly while it runs. For full offload with no blanking use mode 24
(1280×960, 256 colours), 28 (1440×960, 256 colours), 22 (1280×800, 65536 colours) or 15 (1024×768, 65536 colours) and comment out `DROPBEAR_NO_ATW` in that file.

## PLL limits (operator-supplied driver header + GW2AR-18 datasheet table, 2026-10-17)

- **Mode record:** the xVDI driver's `RESOLUTION` struct (`res.h`) holds name, mode, bytes/line, planes, sizes,
  VTG porches/sync/display, PLL fbdsel/idsel/odsel and `ctrl_reg`.
  - `ctrl_reg` bits: a = VTG enable, b = positive H sync, c = positive V sync, d = 0→1 latches the PLL, fe = depth
    (00 1 bpp, 01 8 bpp LUT, 10 16 bpp, 11 15 bpp), **g = LUT owner: 0 = Atari, 1 = transputer**.
  - The header says fbdsel/idsel are "real value − 1". The BIB values are the real dividers: read as "−1", mode
    32 would be 40.5 Hz and mode 24 32 Hz, and the monitor shows both while it rejects < 55 Hz (modes 30 and 34).
    So pixel clock = 27 MHz × fb ÷ in ÷ 5 stands.
- **VCO = CLKOUT × ODIV** (Gowin rPLL), with CLKOUT = 5 × pixel clock and ODIV = 2 in every vendor record.
  The GW2AR-18 allows VCO 500–1250 MHz (speed grade C8/I7, A6) or 400–1000 MHz (C7/I6).

| Pixel clock | VCO | C8/I7 | C7/I6 | Modes |
| ---: | ---: | :--- | :--- | :--- |
| 82.8 MHz | 828 MHz | in spec | in spec | 22 |
| 102.6 MHz | 1026 MHz | in spec | 3 % over | 24, 28, 39 |
| 108.0 MHz | 1080 MHz | in spec | over | 26 |
| 137.7 MHz | **1377 MHz** | **over** | **over** | 32, 37 (the default) |

  ODIV cannot go below 2, so a 137.7 MHz pixel clock cannot be in spec. **Hypothesis, not measured:** the
  intermittent blanking at ≥ 108 MHz is the PLL past its limits, not T425 load. On 2026-10-17 neither T425 nor TRAM
  load reproduced it. To settle it: read the FPGA's speed-grade marking, and run mode 39 (1026 MHz VCO) long-term.
  Mode 39 was the boot mode 2026-10-17 to 2026-10-20; no watched T425 run in it is recorded, so this is still open.
