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
| D. TOS 3.06 + card image `HD00_512-prod14` as-is (XBOOT default set) | — | halts | `ATW800/2 not found`: the default set loads xVDI, and Hatari has no ATW800/2. Not an OS result |
| E. TOS 3.06 + FreeMiNT 1.19 (card images; `C:\AUTO` = `MINT-4EB.PRG` only, XBOOT/xVDI/STiNG disabled) | — | ~104 | XaAES + TosWin2 desktop |
| F. TOS 3.06 + `EMUTOS.PRG` (first in `C:\AUTO`) + FreeMiNT 1.19 | Alt-RAM 64 MB (EmuTOS) | ~112 | EmuTOS banner, then the MiNT boot log, XaAES + TosWin2 — **FreeMiNT/XaAES run on RAM-loaded EmuTOS**; ~8 s later than E |

D–F use work copies (`emulator/fr8/hd00*.hda`, `hd10.hda` = `build/HD10_512-gem.hda`); the
originals in `build/` and on the SD card were not touched. E's `C:\AUTO` edits are 8.3 renames
(`.PRG`↔`.PRX`) in the directory entries; F adds `EMUTOS.PRG` with `scripts/atari_fat16.py`
(plan: put, then move every existing AUTO entry to keep it after EmuTOS). Capture length via
`FR8_SECONDS` (default 150).

## GEM app check — 1st Word Plus (`D:\APPS\WORDPLUS\WORDPLUS.PRG`, 166 KB)

Copied (without dictionaries) to a Hatari GEMDOS drive C: and autostarted by the desktop `#Z`
line (`NEWDESK.INF` for TOS 3.06, `EMUDESK.INF` for EmuTOS).

| Config | App on screen (wall s, ±2) | Result |
|---|---|---|
| G. TOS 3.06 | ~94 | Starts; German UI, file selector, then alert "cannot find the folder" |
| H. EmuTOS 1.4 ROM | ~12 | Same, in EmuTOS's item selector |
| I. TOS 3.06 + `AUTO\EMUTOS.PRG` | ~96 | Same as H |

Same behaviour on all three, so no compatibility difference. The alert is the app's own config:
`WORDPLUS.INF` points at `D:\WORDPLUS`, and the test drive is C:. Not run under XaAES.

Caveats: wall-clock under Xvfb, not a stopwatch on hardware; includes ~1–2 s emulator start.
Not yet measured: GEM apps beyond the one above, VDI drawing rate, the ATW800/2 (xVDI) sets on
EmuTOS (hardware only), and the whole real-TT column.
