# bootlog — boot console font and colours (track tt030-atw800-video-modes FR-9)

AUTO program (`C:\AUTO\BOOTLOG.PRG`, before `MINT-4EB.PRG` in directory order and listed in the XBOOT sets
FREEMINT and MINT_ATW in `C:\XBOOT\SETS.DAT`) that switches the boot console to the Linux VGA 8x8 font and white
on black, and writes what it did to `C:\BOOTLOG.TXT`. XBOOT disables any AUTO program a set does not list.

Build: `m68k-atari-mint-gcc -m68020-60 -O2 bootlog.c -o bootlog.prg` (cross-mint), then strip.

Licence: `font8x8.h` is the Linux kernel font `lib/fonts/font_8x8.c` (GPL-2.0), so `bootlog.prg` is GPL-2.0;
this directory is its complete source.

## Boot console capture (2026-10-17)

`BOOTLOG.PRG` also hooks the BIOS console vector (`xconout[2]`, 0x586) and copies every console character into a
64 KB Malloc'd buffer (`blog.h`); its address goes into `C:\BOOTLOG.TXT` as `log 0x...`. FreeMiNT reads the vector
on every console write (`sys/bios.c` BCONOUT), so kernel, module and `mint.cnf` output is captured too.
`/usr/sbin/bootdump.prg` (atwtools RPM, XaAES autorun) writes it to `/var/log/boot.log` and syslog tag `boot`, which
fractal's rsyslog files in `/var/log/tt030/tt030.log` for the central log index. Not captured: output before
`BOOTLOG.PRG` runs, GEM alert boxes, and a boot that hangs (the buffer is lost). Self-check on a running TT:
build with `-DBLOG_TEST` and run it (it writes a marker to `/dev/console`; a process's own `Bconout(2)` goes to its
tty and never reaches the vector).
