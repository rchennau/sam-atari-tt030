# Amber MDA look (PMS tt030-atw-display-tuning FR-4, operator-approved 2026-10-16)

Target: an amber IBM MDA monitor — black backgrounds, amber text, the MDA character set. Works in 256-colour modes
(mode 32, 1920x1080x8); 1-bit modes have no colour LUT on the ATW800/2.

| Piece | File on the TT | Built by | md5 (2026-10-16) |
| :--- | :--- | :--- | :--- |
| Palette: inverted luminance, pen -> (L, .85L, 0); desktop pen 3 black | `C:\AUTO\DEFAULT.PAL` | `scripts/atw_palette.py amber`, installed by `scripts/tt_palette.sh amber` | ec78ef25 |
| System font 8x16 slot = IBM MDA | `C:\GEMSYS\IBMMDA.FNT` + `C:\AUTO\XVDI.INF` (`16x8=c:\gemsys\ibmmda.fnt`) | `scripts/mda_gemfont.py Px437_IBM_MDA.ttf IBMMDA.FNT` | 030e6cc8 (INF) |
| toswin2: 8x16 font (size 10), background ANSI 7, text ANSI 0 | `/f/root/toswin2.cfg` | saved in toswin2, colours swapped by hand | 92cda697 at install; operator re-saved 10-16 17:25 (fg/bg 0/7, bold) → 557ec152 |

Why toswin2's colours are swapped: toswin2 maps ANSI black to VDI pen 1 and ANSI white to pen 0. The inverted palette
makes pen 0 black and pen 1 amber, so "background white / text black" in toswin2 shows as amber text on black.

Font credit: IBM MDA from *The Ultimate Oldschool PC Font Pack* by VileR (https://int10h.org/oldschool-pc-fonts/),
CC BY-SA 4.0. `IBMMDA.FNT` is a derivative under the same licence. The MDA's 9th column is dropped (it only extended
line-drawing glyphs) and one row is added above and below to fit the 8x16 cell; line and block glyphs are extended
into those rows so they join.

Undo: `tt_palette.sh normal` (removes DEFAULT.PAL); delete `C:\AUTO\XVDI.INF`; restore toswin2.cfg from the backup
(`~/.local/state/tt030-deploy/display-tuning-20261016/toswin2.cfg.orig`, f269ddae); reboot.
