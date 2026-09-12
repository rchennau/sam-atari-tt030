# Atari TT 030 BlueSCSI v2 & STiNG Runbook

> **Rewritten 2026-09-12 from measurements.** The canonical, maintained copies are in the SAM repo:
> inf-card `docs/runbooks/inf-cards/atari-tt030.md` and procedure `docs/runbooks/atari-tt030-bootstrap.md`.
> The 2026-09-07 version of this file made several claims that measurement disproved; they are
> corrected below, next to what replaced them.

## 1. System overview (measured unless marked)

- **Platform:** Atari TT 030 (68030/68882), 4 MB ST-RAM, 64 MB TT-RAM *(operator-reported)*, TOS 3.06.
- **BlueSCSI v2 (Pico 2):** disk `HD00_512.hda` (1 GiB) at **SCSI ID 0**; DaynaPORT `NE3.hda` at **ID 3**.
  `bluescsi.ini` uses `System="MegaSTE"`; it holds the WiFi password — never copy it into docs.
- **Disk layout:** 3 ICD-formatted FAT16 partitions with **8192 B logical sectors** (not readable by
  Linux vfat/mtools/pyfatfs/7z — use `scripts/atari_fat16_ls.py` / `scripts/atari_fat16.py`).
- **Host mounts on fractal:** `/media/atari/sd` and `/media/atari/floppy` (fstab `user` entries — sam
  mounts them without sudo).

## 2. Partitions

**Measured on the original image (2026-09-12, `docs/HD00_512-inventory-2026-09-12.txt`):** C: held a
24.8 MiB legacy TT install; **D: and E: were empty** (0 entries). *Corrected:* the earlier claim that
FreeMiNT lived at `D:\APPS\FREEMINT` and an archive at `E:\ARCHIVE` was never true of this image.

**Rebuilt image (`floppy/hd00-reorg-plan.json`, byte-verified on a copy):** C: system core (AUTO,
STING, XBOOT, GEMSYS, SYS, CPX); D: `APPS\` (incl. FreeMiNT 1.18.0, not installed) and `DATA\`;
E: `DRIVERS\` and `ARCHIVE\`. The original is kept at `backups/HD00_512-orig-2026-09-12.hda`.

## 3. Boot stack and STiNG

- **AUTO order (floppy):** `SCSIDRV.PRG` (CBHD 5.02) → `STING.PRG` → `ICDBOOT.PRG` last. ICDBOOT runs
  the boot partition's `C:\AUTO` itself, so nothing after it in `A:\AUTO` runs. *Corrected:* HDDRIVER
  is not part of the stack (plan C3, freeware only) and did not "precede STiNG".
- **SCSIDRV is required** for the DaynaPORT driver: without it STiNG prints
  `SCSILINK.STX not installed: SCSIDRV not active` (Hatari, 2026-09-12).
- **STiNG modules** in the folder named by `AUTO\STING.INF`: TCP, UDP, RESOLVE, SCSILINK (DaynaPORT),
  SERIAL. *Corrected:* the old `C:\STING` had only `DAYNAPOR.STX`/`SCSILINK.STX` — no TCP/UDP/RESOLVE.
- *Corrected:* `00JAR032.PRG` (64 bytes) is a broken stub that crashes TOS with 11 bombs — removed;
  `STRAMONLY` is not a STiNG 1.26 key; `DEFAULT.CFG` needs `ACTIVATE = TRUE` and has no port/IP keys.
- **Port IPs** are set on the TT in `STNGPORT.CPX` (SCSI/Link for WiFi, Modem 1 for SLIP to fractal);
  routes are in `STING\ROUTE.TAB`.

## 4. Emulator

`emulator/hatari.cfg` is a TT profile (see the SAM inf-card for key names). Attach the disk with
`--scsi 0=<image>` and prove what loaded with `--trace gemdos`. *Corrected:* `scripts/test_sting_hatari.py`
printed hard-coded PASS lines without checking anything — deleted 2026-09-12.
