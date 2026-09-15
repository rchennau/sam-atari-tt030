# C:\ATW800_2 — runtime ATW800/2 files for the card's C: (FAT) partition

Copy this folder to `C:\ATW800_2\` on the BlueSCSI card. `mint.cnf` loads `ATWTOOLS\fpgabios.tos`
before `rc.dropbear`, and `atwxserv` (on the ext2 disk) boots `TRANS_DEV\...` — but note `xserv.btl`
and `atwxserv` themselves live on the ext2 disk under `/etc/dropbear` and `/usr/sbin` (HD10_OVERLAY),
so the copy here is a spare / for running the tools by hand from a TOS shell.

Contents (runtime only — the full 85 MB vendor pack with Helios, d72uni and examples is NOT here;
it is unlicensed for redistribution and lives in `staging/TRANSPUTER/` for the build host):
- `ATWTOOLS/fpgabios.tos`   link driver (GEMDOS extension), loaded at boot
- `ATWTOOLS/iserver.ttp`    INMOS host server (manual transputer runs)
- `ATWTOOLS/resetatw.tos`   restore the display after a raw transputer program
- `ATWTOOLS/linkalia.tos`   AliaBios link driver (alternative to fpgabios)
- `TRANS_DEV/xserv.btl`     sam-ssh-tt / sam-scp-tt X25519 & Ed25519 server (constant-time build)
- `TRANS_DEV/atwinfo.btl`   read the card's FPGA/VDI info
- `TRANS_DEV/sam-scp-tt`    sam-scp-tt transputer SCP acceleration bridge (m68k binary)
