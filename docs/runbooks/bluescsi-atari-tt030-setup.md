# Atari TT 030 BlueSCSI v2 Setup & Troubleshooting Runbook

This runbook documents the exact hardware configuration, SCSI bus parameterization, WiFi DaynaPORT setup, Front-Panel Web Portal access, and partitioning procedures for integrating BlueSCSI v2 into the Atari TT 030 workstation.

---

## 1. Hardware Architecture & Bus Configuration

- **System**: Atari TT 030 (Motorola 68030 @ 32MHz, NCR 5380 SCSI Host Adapter)
- **Primary Boot Drive (Target ID 0)**: Internal Maxtor 7080SC 80MB Hard Disk (`C:`, `D:`, `E:`, `F:`)
- **BlueSCSI Storage (Target ID 1)**: `HD10_512.hda` (1GB Raw SCSI Image -> TOS FAT16 Partitions `G:`, `H:`, `I:`)
- **BlueSCSI Network (Target ID 3)**: DaynaPORT Wireless Ethernet Emulation (`NE3.txt`)

### SCSI Cable & Header Pin 1 Orientation
- **Atari TT Motherboard (J23 Internal Header)**: Pin 1 (Red Stripe) faces the **FRONT** of the Atari TT case (toward the floppy drive bay).
- **Hard Drive / BlueSCSI Connector**: Pin 1 (Red Stripe) faces the **4-pin Molex Power Connector** side of the board/drive.

---

## 2. BlueSCSI `bluescsi.ini` Configuration

Create or update `bluescsi.ini` in the root directory of the BlueSCSI MicroSD card:

```ini
[BlueSCSI]
Debug=1

[SCSI]
System="MegaSTE"
EnableParity=0
EnableFrontPanel=true
Debug=1

[WiFi]
SSID=chennault
Password=ethandaniel
Debug=1

[DaynaPORT]
Debug=1
```

### Key Parameter Rationale
- `System="MegaSTE"`: Sets SCSI timing parameters compatible with Atari NCR 5380 host adapters.
- `EnableParity=0`: Disables SCSI parity checking required by the Atari TT NCR 5380 controller.
- `EnableFrontPanel=true`: Enables the browser-based Web Portal over local WiFi (`http://bluescsi.local`).
- `NE3.txt`: Empty placeholder file in SD root reserving SCSI Target ID 3 for DaynaPORT emulation.

---

## 3. Front-Panel Web Portal Access

Once BlueSCSI v2 powers up and connects to WiFi `chennault` (indicated by a solid Blue Pico W LED):

- **Web Portal URL**: `http://bluescsi.local`
- **Fallback AP Mode URL** (if unconfigured): `http://192.168.4.1` (SSID: `BlueSCSI-FrontPanel`, Pass: `frontpanel123`)
- **Features**:
  - Web-based upload and download of `.hda` hard disk images over WiFi.
  - Live editing of `bluescsi.ini`.
  - On-the-fly CD image injection/ejection.
  - OTA firmware updating.

---

## 4. TOS Partitioning Procedure (`ICDFMT` v6.20)

To partition the 1GB BlueSCSI storage target on Target ID 1 without triggering 2-bomb Bus Errors:

1. **Boot TT 030**: Boot cleanly into TOS 3.06 from internal Hard Drive `C:`.
2. **Launch Installer**: Run `ICDFMT.PRG` v6.20 from `E:\ICD_PRO_655\ICDFMT.PRG`.
3. **Select Target**: Choose **Target 1** (`BLUESCSIHARDDRIVE 1.0`).
4. **Configure Partition Table**:
   - Do NOT use 32MB GEM defaults.
   - Clear default rows and configure 3 **BGM** (Big GEM FAT16) partition rows:
     - **Partition 1 (`G:`)**: Type `BGM`, Start `2`, End `600,000` (~307 MB). Checkbox: **Enabled**.
     - **Partition 2 (`H:`)**: Type `BGM`, Start `600,001`, End `1,300,000` (~358 MB). Checkbox: **Enabled**.
     - **Partition 3 (`I:`)**: Type `BGM`, Start `1,300,001`, End `2,047,999` (~383 MB). Checkbox: **Enabled**.
5. **Execute**: Click `PARTITION ENTIRE HARD DISK` -> `Proceed`.
6. **Reboot**: Restart the Atari TT 030. `ICDBOOT` will automatically mount drive letters `G:`, `H:`, and `I:`.

---

## 5. Network Stack Integration (STiNG 1.26)

- **Engine Location**: `C:\AUTO\STING.PRG` & `C:\AUTO\STING.INF`
- **Protocol Modules**: `C:\STING\` (`TCP.STX`, `UDP.STX`, `RESOLVE.STX`, `ROUTE.TAB`)
- **Control Accessories**: `C:\CPX\STING.CPX`, `C:\CPX\STNGPORT.CPX`
- **Target IP**: `192.168.1.185` (Gateway `192.168.1.1`, Interface: DaynaPORT SCSI ID 3).
