# Atari TT030 Enhancement Project

Management, drivers, custom software, and hardware acceleration for the Atari TT 030 retro-computing workstation in the SAM ecosystem.

---

## Hardware Profile
- **CPU / FPU**: Motorola MC68030 @ 32 MHz · MC68882 FPU
- **Memory**: 4 MB ST-RAM · 64 MB TT-RAM
- **Storage**: BlueSCSI v2 (Pico 2) SCSI ID 0 (`HD00_512.hda`, FAT16) and SCSI ID 1 (`HD10_512.hda`, 1 GiB Ext2 RAW)
- **Networking**: BlueSCSI DaynaPORT WiFi (`192.168.0.30`) & Serial SLIP (`192.168.10.2`)
- **Co-Processor**: ATW800/2 Transputer Board (T425 FPGA transputer @ 40 MHz)

---

## Software Stack & Extensions
- **Bootloader**: XBOOT III (TOS / FreeMiNT profile switcher)
- **Disk Driver**: CBHD 5.02 (SCSIDRV cookie & XHDI partition mapping)
- **Networking**: STiNG 1.26 (TOS) / MiNTnet `scsilink.xif` (FreeMiNT 1.19)
- **Userland**: SpareMiNT 23-package base set (bash 2.05a, OpenSSH 5.6p1, Dropbear 2026.94)

---

## Repository Structure & Subsystems

```
.
├── docs/                      # Technical runbooks, BlueSCSI setup, XBOOT manual & inventory
├── emulator/                  # Hatari workstation emulation configs and HDD image staging
├── scripts/                   # Floppy staging, backup, and emulator integration test harnesses
├── staging/                   # Atari TT formatted floppy & drive C: installation packages
│   ├── AUTO/                  # Driver auto-executables (XBOOT, HDDRIVER, ICD, STiNG)
│   ├── DRIVERS/               # SCSI utility software and disk tools
│   ├── C_ATW800_2/            # ATW800/2 transputer tools, drivers, and runtime server binaries
│   └── TRANSPUTER/            # INMOS T800 Transputer SDK & Helios OS distribution packages
├── src/                       # Custom source code for Atari TT030 co-processing & tooling
│   └── x25519bench/           # Transputer server source, Dropbear offload, and host bridges
└── tt_bridge/                 # SAM TT-Bridge HTTP Client C codebase (TOS/MiNT)
```

---

## Project Phases & Key Accomplishments

### Phase 0: System Staging & Storage Baseline
- 1.44 MB FAT12 staging floppies and sector-level SCSI backup utilities.
- Dual BlueSCSI v2 disk mapping with HDDRIVER / CBHD / ICD Pro driver suites.

### Phase 1: Native TT-Bridge HTTP Communication
- Cross-compiled HTTP/1.1 client (`tt_bridge/`) targeting M68030 / FreeMiNT & TOS STiNG stack.

### Phase 2: Workstation Emulation & SLIP Networking
- Hatari integration test automation and PPP/SLIP serial bridge.

### Phase 3 & Transputer Hardware Acceleration (`sam-ssh-tt` & `sam-scp-tt`)
The ATW800/2 T425 transputer accelerates cryptographic session setup and transfer framing for SSH and SCP:

1. **`sam-ssh-tt`**: Offloads X25519 key exchange and Ed25519 signature verification to the T425 transputer, reducing login handshake latency from ~9.4 s down to ~6.4 s.
2. **`sam-scp-tt`**: Transputer-assisted SCP acceleration bridge operating alongside `fpgabios.tos` and `atwxserv` (`xserv2.btl` combined server).

#### Architecture & Build Patterns
- **Host Bridge**: `src/x25519bench/sam_scp_tt.c` (built with `m68k-atari-mint-gcc -m68020-60 -O2`)
- **Transputer Server**: `src/x25519bench/ed25519/xserv2.c` (built with INMOS ANSI C `icc`/`ilink` under `t4`)
- **Runtime Staging**: `staging/C_ATW800_2/` and `tools/dropbear-native/`

