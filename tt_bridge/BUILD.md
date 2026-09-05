# Atari TT030 Cross-Compilation Guide - SAM TT-Bridge HTTP Client

This guide details the cross-compilation toolchain setup and build steps for target host **Atari TT030** (Motorola 68030 CPU, TOS 3.06 / FreeMiNT).

---

## 1. Toolchain Prerequisites

To build the `TTBRIDGE.PRG` executable binary from modern Linux (x86_64 / arm64), choose one of the two supported m68k Atari cross-compilers:

### Option A: `m68k-atari-mint-gcc` (GCC 13+ Cross Compiler) [Recommended]
- **Ubuntu/Debian PPA**: `ppa:ggarra13/cross-mint` or Vincent Rivière's m68k-atari-mint GCC distribution.
- **Docker Container**:
  ```bash
  docker run --rm -v $(pwd):/src -w /src registry.gitlab.com/vincentriviere/cross-mint gcc -O2 -m68030 -m88881 -Isrc src/main.c src/http_client.c -o bin/ttbridge.prg
  ```

### Option B: `vbcc` (Portable C Compiler for Amiga/Atari)
- Install `vbcc` with the Atari TOS target config (`target/m68k-atari`).
- Environment variables:
  ```bash
  export VBCC=/opt/vbcc
  export PATH=$PATH:$VBCC/bin
  ```

---

## 2. Building `TTBRIDGE.PRG`

### Building with GCC (`m68k-atari-mint-gcc`)
From the `tt_bridge` directory:
```bash
make gcc
```
This produces `bin/ttbridge.prg` targeting Motorola 68030 (`-m68030 -m88881`).

To build a universal M68000 binary compatible with legacy Atari ST/STE models:
```bash
M68K_CPU_FLAGS="-m68000" make gcc
```

### Building with VBCC (`vc`)
```bash
make vbcc
```
This produces `bin/ttbridge_vbcc.prg` using `vc -sc-only -mint`.

---

## 3. Deployment to Atari TT030

1. **Install to Staging Floppy Directory**:
   ```bash
   make install
   ```
   This copies `bin/ttbridge.prg` into `../staging/AUTO/TTBRIDGE.PRG`.

2. **Format & Write Floppy Disk**:
   ```bash
   ../scripts/stage_floppy.sh --device /dev/sdf
   ```

3. **Deploy via BlueSCSI v2 SD Card**:
   - Copy `TTBRIDGE.PRG` directly to the `HD00_512.img` hard disk volume using `mtools` or Hatari disk utility.

---

## 4. Execution & CLI Options on Atari TOS / FreeMiNT

Run `TTBRIDGE.PRG` from the TOS GEM Desktop or MiNT bash shell:

```bash
TTBRIDGE.PRG -h 192.168.1.1 -p 8080 -u /api/v1/status
```

### Command Line Arguments:
- `-h, --host HOST`: Host IP address of the SAM server bridge (default: `192.168.1.1`).
- `-p, --port PORT`: Server TCP port (default: `8080`).
- `-u, --url PATH`: HTTP path endpoint (default: `/api/v1/status`).
- `-m, --method METHOD`: HTTP Method `GET` or `POST` (default: `GET`).
- `-d, --data DATA`: POST body content string / JSON payload.
- `-o, --out FILE`: Path to write the HTTP response payload file.
