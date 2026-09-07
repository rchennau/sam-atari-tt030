# Atari TT 030 BlueSCSI v2 & STiNG WiFi DaynaPORT Integration Runbook

## 1. System Overview
- **Hardware Platform**: Atari TT 030 (32MHz Motorola 68030, 68882 FPU, 64MB TT RAM).
- **Solid-State Storage**: BlueSCSI v2 (RP2350 Pico 2 W) Target ID 0 (`HD00_512.hda`, 1GB RAW SCSI Image).
- **Embedded AHDI Partition Table**: `HD00_512.hda` contains 3 Atari AHDI FAT16 partitions (`C:`, `D:`, `E:`).
- **Wireless Ethernet**: Emulated DaynaPORT SCSI/Link on SCSI ID 3 (`NE3.hda`) bound to WiFi SSID `chennault`.
- **TCP/IP Stack**: STiNG v1.26 with Anodyne `DAYNAPOR.STX` / `SCSILINK.STX` drivers.
- **Boot Manager**: XBOOT III v3.21 Registered (`SHOW_MENU = 1`).
- **Multitasking & Open OS**: FreeMiNT 1.18.0 distribution (`D:\APPS\FREEMINT\`) & EmuTOS 1.3 512k ROM suite (`E:\ARCHIVE\EMUTOS\`).
- **Emulation Workstation**: Hatari v2.5.0 Atari TT 030 (`emulator/hatari.cfg`, `scripts/setup_emulator.sh`).
- **Host Mount Location Mandate**: `/media/atari/` for all block devices and image mounts.

---

## 2. Inner AHDI Logical Partition Distribution (`HD00_512.hda`)
- **`C:` System Partition (292 MB)**: Operating System Core (`AUTO/`, `STING/`, `XBOOT/`).
  - `C:\AUTO\`: `HDDRIVER.PRG`, `00JAR032.PRG`, `STING.INF`, `XBOOT.PRG`, `XBOOT.INF`.
  - `C:\STING\`: `STING.PRG`, `DEFAULT.CFG`, `DAYNAPOR.STX`, `STING.INF`.
  - `C:\XBOOT\`: `XBOOT.PRG`, `XBOOT.INF` (`SHOW_MENU = 1`, `TIMEOUT = 10`).
- **`D:` Apps Partition (341 MB)**: User Applications & Multitasking (`D:\APPS\FREEMINT\freemint\`).
- **`E:` Drivers & Archive Partition (365 MB)**:
  - `E:\DRIVERS\`: `ATW800/` Transputer, `SCSI/`, `NET/`.
  - `E:\ARCHIVE\`: `GERMAN_DOCS/`, `EMUTOS/`, `LOGS/`, `FIRMWARE/`.

---

## 3. STiNG & Hardware Boot Stack Configuration Rules
- **Cookie Jar Expansion**: `C:\AUTO\00JAR032.PRG` expands Cookie Jar to 32 slots before STiNG initializes.
- **SCSI Driver Order**: `C:\AUTO\HDDRIVER.PRG` (Uwe Seimet SCSI driver) must precede XBOOT III and STiNG to initialize partition tables.
- **Boot Manager**: `C:\AUTO\XBOOT.PRG` and `C:\XBOOT\XBOOT.INF` configured with `SHOW_MENU = 1` and `TIMEOUT = 10`.
- **DMA Safe ST-RAM Memory Directive**: `C:\AUTO\STING.INF` and `C:\STING\DEFAULT.CFG` include `STRAMONLY = 1` to prevent 2-bomb (Bus Error) crashes on 64MB TT RAM systems during NCR 5380 SCSI DMA transfers.
- **Protocol Configuration**: `C:\STING\DEFAULT.CFG` contains TAB-separated configuration parameters (`IP = 192.168.0.185`, `GATEWAY = 192.168.0.1`).
- **Driver Binary**: `C:\STING\DAYNAPOR.STX` (Official Anodyne DaynaPORT driver binary).
- **Boot Diagnostics**: `C:\STING.LOG` and `C:\AUTO\00LOGALL.PRG` console logger.

---

## 4. Emulation Environment Setup & Launch
To launch the Hatari Atari TT 030 workstation emulator on `fractal`:

```bash
/home/sam/Projects/atari-tt030-enhancement/scripts/setup_emulator.sh
```

To run the automated integration test suite:

```bash
/home/sam/Projects/atari-tt030-enhancement/scripts/test_sting_hatari.py
```
