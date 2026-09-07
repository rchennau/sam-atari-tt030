# Atari TT 030 BlueSCSI v2 & STiNG WiFi DaynaPORT Integration Runbook

## 1. System Overview
- **Hardware Platform**: Atari TT 030 (32MHz Motorola 68030, 68882 FPU, 64MB TT RAM).
- **Solid-State Storage**: BlueSCSI v2 (RP2350 Pico 2 W) Target ID 0 (`HD00_512.hda`, 1GB TOS FAT16).
- **Wireless Ethernet**: Emulated DaynaPORT SCSI/Link on SCSI ID 3 (`NE3.hda`) bound to WiFi SSID `chennault`.
- **TCP/IP Stack**: STiNG v1.26 with Anodyne `DAYNAPOR.STX` / `SCSILINK.STX` drivers.
- **Emulation Workstation**: Hatari v2.5.0 Atari TT 030 (`emulator/hatari.cfg`, `scripts/setup_emulator.sh`).
- **Host Mount Location Mandate**: `/media/atari/` for all block devices and image mounts.

---

## 2. STiNG Boot Stack Configuration Rules
- **Cookie Jar Expansion**: `C:\AUTO\00JAR032.PRG` expands Cookie Jar to 32 slots before STiNG initializes.
- **AUTO Path Directive**: `C:\AUTO\STING.INF` contains strictly `C:\STING` (resolves configuration path lookup).
- **DMA Safe ST-RAM Memory Directive**: `C:\STING\DEFAULT.CFG` includes `STRAMONLY = 1` to prevent 2-bomb (Bus Error) crashes on 32-bit FastRAM systems during SCSI DMA transfers.
- **Protocol Configuration**: `C:\STING\DEFAULT.CFG` contains TAB-separated configuration parameters (`IP = 192.168.0.185`, `GATEWAY = 192.168.0.1`).
- **Driver Binary**: `C:\STING\DAYNAPOR.STX` (Official Anodyne DaynaPORT driver binary).
- **Boot Diagnostics**: `C:\STING.LOG` and `C:\AUTO\00LOGALL.PRG` console logger.

---

## 3. Emulation Environment Setup & Launch
To launch the Hatari Atari TT 030 workstation emulator on `fractal`:

```bash
/home/sam/Projects/atari-tt030-enhancement/scripts/setup_emulator.sh
```

To run the automated integration test suite:

```bash
/home/sam/Projects/atari-tt030-enhancement/scripts/test_sting_hatari.py
```
