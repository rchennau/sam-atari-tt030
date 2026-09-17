# FR-8 OS evaluation — emulator column (Hatari 2.5, 2026-09-16)

Scope: EmuTOS 1.4 vs stock TOS 3.06 (+ FreeMiNT, pending). MultiTOS not evaluated (operator
decision 2026-09-16: no licensed source). Real-TT column still to do (TT was off).

Rig: `scripts/fr8_boot.sh <name> <rom> [hatari args]` (work dir `emulator/fr8/` is git-ignored: ROMs, AUTO\EMUTOS.PRG, shots) — Xvfb + Hatari
flatpak, TT, 68030/68882, 4 MB ST + 64 MB TT RAM, `--fast-forward off`, a screenshot every 2 s
for 150 s. EmuTOS files: SourceForge `emutos-prg-1.4.zip`, `emutos-512k-1.4.zip` (GPL).

| Config | TT RAM seen | Screen settles (wall s, ±2) | Notes |
|---|---|---|---|
| A. TOS 3.06 UK ROM | 65536 KB (memory test) | ~96–100 | ~80 s of it is the 64 MB memory test |
| B. EmuTOS 1.4 512K ROM (emulator only — C1 forbids flashing) | not shown on splash | ~22 | Desktop, drives A/B |
| C. TOS 3.06 + `AUTO\EMUTOS.PRG` (RAM-loaded; the real-TT path) | "Alt-RAM 64 MB" on EmuTOS splash | ~94 | Pays TOS's memory test, then EmuTOS boots in ~10 s; GEMDOS drive C: visible |

Caveats: wall-clock under Xvfb, not a stopwatch on hardware; includes ~1–2 s emulator start.
Not yet measured: GEM application compatibility, VDI drawing rate, FreeMiNT on EmuTOS,
TOS 3.06 + FreeMiNT (needs a real HD image; `emulator/hd00_512.hda` is a 28-byte placeholder).
