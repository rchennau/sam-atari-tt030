# Atari TT030 & ATW800/2 Transputer Enhancement Project

Management, drivers, custom software, and hardware acceleration utilities for the **Atari TT030 retro-computing workstation** operating within the **SAM (Sensible Agent Management)** ecosystem.

---

## Hardware & System Overview

### Atari TT030 Workstation
The Atari TT030 is a 32-bit Motorola 68030-based workstation running at 32 MHz with an MC68882 FPU, 4 MB ST-RAM, and 64 MB TT-RAM. Equipped with BlueSCSI v2 (Pico 2) storage and BlueSCSI DaynaPORT WiFi local networking, the machine serves as a live node within the SAM architecture running TOS 3.06 and FreeMiNT 1.19.

### ATW800/2 Transputer Co-Processor
The ATW800/2 is an expansion board housing an INMOS T425 32-bit RISC Transputer running at 40 MHz (implemented via FPGA). The transputer features parallel hardware links, hardware process scheduling, and dedicated SRAM.

### Project Purpose
The primary purpose of this project is to adapt and modify legacy and modern Unix/C software to run in tandem with the ATW800/2 Transputer. By offloading computationally intensive operations (such as Elliptic-Curve Cryptography X25519 key exchange, Ed25519 signature verification, and payload stream framing) to the T425 transputer, the host 68030 CPU is freed from heavy mathematical bottlenecks during secure remote management sessions.

## Project Sub-Utilities & Independent Repositories

Each custom software component contains its own dedicated project directory, README, build instructions, and execution documentation:

1. **[`sam-ssh-tt`](sam-ssh-tt/README.md)**: Dropbear SSH server with T425 transputer offload for X25519 key exchange & Ed25519 signature verification (per-login crypto ~25.0 s → ~14.4 s, measured on the TT 2026-09-13).
2. **[`sam-scp-tt`](sam-scp-tt/README.md)**: SCP acceleration bridge for hardware packet framing and high-speed transputer transfer handling.
3. **[`ttmon`](src/ttmon.c)**: Telemetry publisher — retained MQTT `sam/node/telemetry/atari-tt030` every 60 s (the live SAM integration).
4. **[`sam-yum-tt`](sam-yum-tt/README.md)** — **v1.0, working on the real TT**: `yum install | update | remove | list | search | makecache` from the LAN mirror, with a signed index, per-package SHA-256 and build-on-miss for anything the mirror lacks (track `tt030-rpm-pipeline`, all requirements met). Shipped as the `yum-tt` RPM (`src/yum` + `resolve.awk` + `update.awk`).
5. **[`sam-rom-tt`](sam-rom-tt/README.md)** — *alpha skeleton, unproven*: custom (no-TOS) 512 KB firmware for the four ROM sockets, plus the byte-lane interleave tool. Nothing here is proven to run; do not burn from it.
6. **[`ttmqtt`](src/ttmqtt.c)**: Minimal MQTT 3.1.1 `pub` / `sub -W secs` for shell scripts (113 KB). `sub` prints the first message's topic and payload; exit 0 message, 1 timeout, 2 error. Broker by name (`mqtt.sam.int`, pinned in the TT's `/etc/hosts`). Carries build-on-miss requests and replies.
7. **[`tt_bridge`](tt_bridge/README.md)**: *Superseded by `ttmon` (2026-09-13).* HTTP/1.1 client originally aimed at port 8080, which on fractal is taken by another service.
---

## Software Stack & Subsystem Directory

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
├── sam-ssh-tt/                # Transputer-accelerated Dropbear SSH server project & docs
├── sam-scp-tt/                # Transputer-assisted SCP acceleration bridge project & docs
├── sam-rom-tt/                # ALPHA skeleton: custom no-TOS TT030 firmware (512 KB, 4 byte-lane chips)
├── sam-yum-tt/                # the TT-side yum client (+ the T425 SHA-256 probe that ruled out offload)
└── tt_bridge/                 # SAM TT-Bridge HTTP Client C codebase project & docs (TOS/MiNT)
```

---

## Role of SAM (Build, Test, & Deployment Pipeline)

The **SAM (Sensible Agent Management)** platform orchestrated end-to-end development, verification, and live deployment to the Atari TT030:


1. **Cross-Compilation & Build**:
   - Built Motorola 68030 host binaries using `m68k-atari-mint-gcc` (GCC 15.2 cross-compiler, `scripts/setup_cross_mint.sh`) and INMOS ANSI C (`icc`/`ilink` under `t4`) for the T425 transputer server code.
2. **Automated Emulation & Integration Testing**:
   - Validated binaries using Hatari emulator automation suites (`scripts/test_sting_hatari.py`), checking network stack behavior and raw sector disk safety before touching real hardware.
3. **Over-the-Air Live Staging & Deployment**:
   - Bootstrapped real hardware via DaynaPORT WiFi (`192.168.0.30`) and Dropbear SSH/SCP.
   - Live-tested `atwxserv` persistent server initialization, verifying handshake acceleration directly on the ATW800/2 hardware co-processor.

---

## Package Pipeline (track `tt030-rpm-pipeline`)

```mermaid
flowchart LR
    subgraph fractal ["fractal (build host)"]
        A["scripts/build_*.sh<br/>m68k-atari-mint-gcc"] --> B["scripts/rpm_header.py<br/>write_rpm (v3, no rpmbuild)"]
        B --> C["/mnt/vault/tt030/built/*.rpm"]
        U["SpareMiNT upstream RPMs"] --> M["/mnt/vault/tt030/m68kmint/*.rpm"]
        C --> I["scripts/tt_rpm.py index<br/>index.tsv"]
        M --> I
    end
    subgraph nas ["NAS /vault/tt030 (NFS from fractal)"]
        I --> S["mirror files"]
    end
    subgraph ct104 ["CT104 Caddy"]
        S --> H["http://mirror.sam.int/tt030/<br/>LAN sources only"]
    end
    subgraph tt ["Atari TT030"]
        H -->|wget| R["rpm -i<br/>(UNIXMODE=/brUs)"]
        Q["ttmqtt pub/sub"] <-->|"sam/tt030/build/*"| BR["mqtt.sam.int broker"]
    end
```

| Piece | Where | State (2026-09-22) |
| :--- | :--- | :--- |
| RPM header parser + v3 writer | `scripts/rpm_header.py` | ✅ no `rpm` binary needed; the TT's `rpm -qpi` / `-i` / `-e` / `-V` accept its output |
| Host mirror CLI | `scripts/tt_rpm.py` — `sync`, `index`, `add`, `push`, `keygen`, `synth` | ✅ 420-package signed index; `push` installs to a TT that cannot reach the mirror (ssh, SHA re-checked on the TT) |
| Mirror | NAS `/vault/tt030` → `http://mirror.sam.int/tt030/` (CT104 Caddy, LAN only) | ✅ tailnet source 403, LAN 200 |
| `yum` client | `sam-yum-tt/src/yum` + `resolve.awk` + `update.awk`, packaged as `yum-tt` | ✅ `install` 47 s (`less`), `update` 35 s (upgraded itself 1.7→1.8), `remove`, `list`, `search`, `makecache` |
| TT tools | `mawk` (cross-built), SpareMiNT `wget`, `src/ttmqtt.c`, `src/ttsign.c` | ✅ all installed as RPMs from the mirror |
| Build-on-miss | `scripts/ttbuildd.py` (fractal unit `sam-ttbuildd`), `scripts/build_recipe.py`, `recipes/source-map.json` | ✅ `yum install pv` with pv absent → built + installed, 1 m 47 s; no recipe → queued + board card; build error → `failed`; broker down → exit 1 |
| Recipes | `pv` 1.7.24, `xz` 5.8.4 | ✅ two clean builds give the same stripped binary (NFR-3) |
| rpm DB backfill | `scripts/rpm_va_classify.py` | ✅ `rpm -Va` classified: 0 real drift |
| Card image | `scripts/build_hd10_ext2.sh` + `staging/SAM_RPMS/` | ✅ rebuilt with the yum tools; **not boot-tested** (needs an SD flash) |

### FreeMiNT rpm / yum package management

The TT runs SpareMiNT's **rpm 3.0.6** (2001) against a modern mirror. What that costs:

- **`UNIXMODE` is load-bearing.** Without it mintlib opens files in text mode and turns CRLF into LF:
  `openssl` prints wrong digests, `rpm -i` dies with `cpio: read`, and `rpm -V` reports false MD5
  mismatches. The kernel sets `/brUs` at boot, but Dropbear cleared it for ssh sessions until
  `rc.dropbear` gained `-e`. `yum` re-exports it defensively.
- **`rpm --root` is not a sandbox.** SpareMiNT patches `chroot()` to `chdir()`, so only the database
  moves and files land at their real paths.
- **`RPMVERSION` must be in the header,** or rpm 3.0.6 on a big-endian host verifies with its "broken
  MD5" routine and flags every file. `write_rpm` writes it.
- **No `rpmbuild`.** Packages are written byte-wise by `rpm_header.write_rpm`: v3 lead (arch 13, OS 255),
  flat `FILENAMES`, gzip cpio whose names carry no `./`, symlinks as MiNT's `S_IFLNK` **0160777**.
- **`yum` runs under bash,** because the TT's `/bin/sh` is a minimal bash build with no `kill` builtin.
- **Integrity:** the client reads one file, `index.signed` = RSA signature + index, so a GET can never
  pair a new index with an old signature; its `#serial` line must not go backwards (replay), and every
  package's SHA-256 is checked before a plain `rpm -i` (never `--force`). Build requests are Ed25519-signed
  by the card (`ttsign`), since the estate broker is anonymous and classic mosquitto ACLs cannot deny one
  topic.
- **Speed, measured on the 68030:** `makecache` ~18 s · `yum install less` ~47 s · `rpm -Va` ~16 min ·
  RSA-2048 signing 2 m 29 s (hence Ed25519 at ~2.7 s) · `xz -6` fails for want of ~94 MB RAM (use `-0`..`-3`).
- **Housekeeping:** the card has no `wheel` group, so package files were `chgrp`'d to a new one; the two
  `gzip` symlinks cannot follow (MiNT has no `lchown`). Providers such as `base`, `initscripts`,
  `freemint-net` and `gawk` are deliberately not installed: they would overwrite `/etc`, replace the boot
  scripts, pull the kernel package, or collide with mawk's `/usr/bin/awk`.

Full operator guide: `docs/runbooks/tt030-packages.md` in the SAM monorepo.

*Superseded:* `scripts/sam_tt030_rpm_builder.py` and the `ping` spec workflow (`rpmbuild`-based, pre-plan) were not used; the plan requires the dependency-free writer above.

---

## Hardware Profile
- **CPU / FPU**: Motorola MC68030 @ 32 MHz · MC68882 FPU
- **Memory**: 4 MB ST-RAM · 64 MB TT-RAM
- **Storage**: BlueSCSI v2 (Pico 2) SCSI ID 0 (`HD00_512.hda`, FAT16) and SCSI ID 1 (`HD10_512.hda`, 1 GiB Ext2 RAW)
- **Networking**: BlueSCSI DaynaPORT WiFi (`192.168.0.30`) & Serial SLIP (`192.168.10.2`)
- **Co-Processor**: ATW800/2 Transputer Board (T425 FPGA transputer @ 40 MHz)

---


