# SAM SSH TT (`sam-ssh-tt`) — Atari TT030 Transputer-Accelerated Dropbear SSH Server

`sam-ssh-tt` provides transputer-accelerated SSH key exchange and signature verification for the Atari TT030 running TOS 3.06 or FreeMiNT, offloading heavy Curve25519 (X25519) scalar multiplication and Ed25519 signature verification to the ATW800/2 INMOS T425 transputer co-processor.

---

## Utility Demonstration

![sam-ssh-tt Demo](file:///home/sam/Projects/atari-tt030-enhancement/docs/media/sam-ssh-tt-demo.gif)

---


## Hardware & Operating System Support
- **Target Platform**: Atari TT030 (Motorola 68030 @ 32 MHz)
- **Co-Processor**: ATW800/2 Transputer Card (T425 32-bit RISC Transputer @ 40 MHz)
- **Driver Layer**: Resident `fpgabios.tos` interface driver and `atwxserv` background server

---

## Performance Benchmark

| Operation | 68030 Host Alone | T425 Transputer Offload | Handshake Savings |
| :--- | :--- | :--- | :--- |
| **X25519 Key Exchange** | 13.0 s | **5.4 s** | 58% faster |
| **Ed25519 Host Verification** | 9.4 s | **6.4 s** | 32% faster |
| **Total SSH Login Handshake** | ~22.4 s | **~11.8 s** | **~47% overall reduction** |

---

## Build Instructions

### Prerequisites
- `m68k-atari-mint-gcc` (GCC 13+ Atari cross-compiler)
- INMOS C Compiler toolchain (`icc`, `ilink`) or pre-built `xserv2.btl` bootstrap server binary

### 1. Build host Dropbear SSH Server (`sam-ssh-tt` binary)
```bash
cd tools/dropbear-native/dropbear-2026.94
./configure --host=m68k-atari-mint --prefix=/usr --disable-zlib
make dropbear
```

### 2. Compile Transputer Server Image (`xserv2.btl`)
```bash
cd src/x25519bench/ed25519
icc -t4 -O2 xserv2.c -o xserv2.t4
ilink xserv2.t4 -o xserv2.btl
```

---

## Deployment & Execution Instructions

1. **Stage Files on Drive C:**
   - Copy `dropbear` to `C:\USR\SBIN\DROPBEAR.TTP` (or `/usr/sbin/dropbear` under FreeMiNT).
   - Copy `xserv2.btl` to `C:\ETC\DROPBEAR\XSERV.BTL` (or `/etc/dropbear/xserv.btl`).
   - Place `fpgabios.tos` into `C:\AUTO\FPGABIOS.TOS`.

2. **Boot Server**:
   - On boot, `fpgabios.tos` initializes link adapter registers at `$FFFF8001`.
   - Start `atwxserv` or launch Dropbear via network daemon:
     ```bash
     /usr/sbin/dropbear -E -p 22
     ```
3. **Verify Offload**:
   - Connect via SSH: `ssh -i ~/.ssh/id_ed25519 sam@192.168.0.30`
   - Check `/var/log/messages` or debug console output for `[T425] X25519 hardware offload active` and `[T425] Ed25519 verify complete`.
