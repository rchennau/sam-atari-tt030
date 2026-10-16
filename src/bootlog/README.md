# bootlog — boot console font and colours (track tt030-atw800-video-modes FR-9)

AUTO program (`C:\AUTO\BOOTLOG.PRG`, before `MINT-4EB.PRG` in directory order and listed in the XBOOT sets
FREEMINT and MINT_ATW in `C:\XBOOT\SETS.DAT`) that switches the boot console to the Linux VGA 8x8 font and white
on black, and writes what it did to `C:\BOOTLOG.TXT`. XBOOT disables any AUTO program a set does not list.

Build: `m68k-atari-mint-gcc -m68020-60 -O2 bootlog.c -o bootlog.prg` (cross-mint), then strip.

Licence: `font8x8.h` is the Linux kernel font `lib/fonts/font_8x8.c` (GPL-2.0), so `bootlog.prg` is GPL-2.0;
this directory is its complete source.
