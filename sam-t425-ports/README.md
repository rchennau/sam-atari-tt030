# sam-t425-ports — faster FreeMiNT userland on the TT030 + ATW800/2

Rebuild the CPU-heavy programs of the SpareMiNT package set so they run faster on this machine: an
Atari TT030 (68030 at 32 MHz, 68882 FPU) with an ATW800/2 card (INMOS T425 transputer, FPGA). Every
port is measured on the real TT, shipped as an RPM that `yum` installs from the LAN mirror, and
recorded below whether it won or lost.

Internal track: `tt030-t425-kernel-ports` (SAM). Results as of **2026-09-25**.

## Scope — what this is and is not

| Assumption | Measured answer |
|---|---|
| "FreeMiNT **kernel** packages" | No. The **FreeMiNT kernel is not touched.** "Kernel" here means a *compute kernel*: the hot loop of a userland program (compression, image codecs, bignum) that can run as buffer in → buffer out. 35 of the mirror's 420 packages were classified that way. |
| Recompiling for the TT030 makes programs faster | **Often, not always.** SpareMiNT's binaries are **68000 code**: stock `bc` contains 0 `mul.l`, 0 `div.l`, 0 `extb.l`, 0 FPU instructions (disassembled 2026-09-25). The same `bc` source built `-m68000` vs `-m68020-60`, timed on the TT, output identical: `sqrt(2)` to 500 digits **27.9 → 5.6 s (0.20)**, pi to 200 digits **28.2 → 18.8 s (0.67)**, `2^20000` **34.9 → 34.3–38.2 s (≈ 1.0, no gain)**. Division-heavy work gains most; the gain is per workload and has to be measured. |
| "The TT030's full system" | = this machine **with its upgrades**: 68030 + 68882 FPU, 64 MB TT-RAM, the ATW800/2 (T425 transputer, ~5 MB of its own, measured usable 4.94 MB, over a link of ~459 KB/s one way) and BlueSCSI storage. Each is a separate lever, measured separately: the **68030/68882 code** (row above), **TT-RAM** (stock and new binaries already carry all three TT-RAM program flags — fastload, load into TT-RAM, allocate from TT-RAM — so no gain is left there; 1,379 of the mirror's 1,380 executables, scanned 2026-09-25) and the **T425** (wins only where the work per byte moved is high). |
| This makes FreeMiNT **leaner** | **Not in disk or RAM.** The new binaries are larger than stock (they carry the offload runtime and newer libraries; +31 % to +57 % where a stock binary was rebuilt), and each T425 port adds a 35–165 KB server file. The twins read whole files into memory where stock streams them. What gets leaner is **CPU time**. One exception: the 68030 build of `bc` is ~9 % smaller (204 KB vs 224 KB) than the same source built for the 68000. |
| Everything gets faster | No. Several ports were **measured and declined** (table below): the T425 lost wherever the data is large and the work per byte small (inflate, LZW decode, SHA-256, image scaling). |

## How a port is judged

1. **Same code on both CPUs.** The T425 server and the 68030 fallback compile the same C, so output is
   identical whichever runs; any T425 failure falls back to the 68030 and says why.
2. **Speed gate:** the T425 path must take **≤ 0.90** of the same binary with offload off, including
   the link transfer, on the real TT (two runs, no SSH during timing — Dropbear uses the T425 too).
3. **Correctness gate:** output byte-identical 68030 ↔ T425, and identical to (or decodable by) the
   stock SpareMiNT tool.
4. Offload is **off until the gate passes**: a separate `<pkg>-t425-on` RPM installs the switch file
   `/etc/t425/enabled/<pkg>`.
5. Where a port is a **twin** (`t`-prefix), the stock program stays installed and unchanged; the twin
   covers the common options and says which ones to use stock for.

## Results — shipped

Times are wall clock on the TT030 (median of 2). **Stock** = SpareMiNT binary. **68030** = the new
binary, offload off. **T425** = offload on.

| Program | Input | Stock | 68030 | T425 | T425 / stock |
|---|---|---|---|---|---|
| `tgzip` (gzip -6) | 429 KB text | 22.0 s | — | 18.3 s | **0.83** |
| `tbzip2` | 542 KB binary | 57.5 s | — | 47.1 s | **0.82** |
| `lha a` (lh5) | 256 KB text / binary | 31.3 / 37.3 s | 26.0 / 32.7 s | 15.9 / 17.1 s | **0.51 / 0.46** |
| `lha x` | 256 KB text / binary | ~11 / ~13 s | 6.9 / 7.9 s | 5.9 / 7.0 s | — |
| `zip` | 256 KB text / binary / random | 12.6 / 17.6 / 18.3 s | 13.0 / 18.0 / 15.2 s | 11.2 / 14.4 / 11.3 s | **0.89 / 0.82 / 0.62** |
| `djpeg` | 800×600 JPEG | 41.4 s | 29.7 s | 15.4 s | **0.37** |
| `cjpeg` | 800×600 PPM | 39.8 s | 31.3 s | 19.7 s | **0.49** |
| `tppmquant 256` | 800×600 | 676.6 s | 471.5 s | 301.0 s | **0.45** |
| `tpnmtopng` | 800×600 RGB / gray / 320×240 | 194.2 / 95.8 / 31.1 s | 202.1 / 72.6 / 33.6 s | 163.1 / 51.9 / 28.1 s | **0.84 / 0.54 / 0.90** |
| `tbc` (bc) | 500 digits of √2 / 200 of π / 800! / 2^20000 | 19.5 / 62.6 / 37.5 / 97.1 s | 5.9 / 20.0 / 10.9 / 33.5 s | 4.9 / 11.8 / 8.0 / 20.0 s | **0.25 / 0.19 / 0.21 / 0.21** |

**68030-only twins** (the T425 lost its gate; the rebuilt 68030 path won — in-memory I/O plus 68030 code,
the two not measured separately):

| Program | Input | Stock | 68030 (new) | new / stock | T425 / 68030 (declined) |
|---|---|---|---|---|---|
| `tpnmscale` (`-xsize/-ysize/-xysize`) | 800×600 → 400 wide | 40.3 s | 16.2 s | **0.40** | 0.92–1.18 |
| `tgiftopnm` | 800×600, 256 colours | 39.1 s | 19.9 s | **0.51** | 0.80–1.16 |

Output: byte-identical to stock for `lha`, `cjpeg`/`djpeg`, `tpnmtopng`, `tpnmscale`, `tgiftopnm` and
`tbc`. `tgzip`/`tbzip2` are standard multi-member files (+0.3–0.9 %); `zip` +0.02–0.15 % (zlib vs zip's
own deflate); `tppmquant` picks a different but deterministic colormap than stock.

Offload windows (below them the ~1 s T425 boot loses; measured per port): `lha` compress 16 KB–1.5 MB,
extract 256 KB–1.5 MB; `zip` 128 KB–1.5 MB; `cjpeg`/`djpeg` from 320×240; `tppmquant` 64 KB–1.5 MB of
pixels; `tpnmtopng` from 224 KB. `tbc` offloads every run (its cost is not known in advance): `2+2`
takes 1.6 s instead of 0.45 s.

## Results — measured and declined

| Kernel | Why it lost |
|---|---|
| zlib inflate (`unzip`, `gunzip`) | 1.42–1.67× slower incl. transfer: inflate already runs at 230–330 KB/s on the 68030 |
| SHA-256 | 1.02×: too little work per byte moved |
| `pnmscale`, `giftopnm` (T425 path) | on/off 0.80–1.18: shipped as 68030-only twins instead |
| libtiff LZW decode | bound: decode is ~12 s of `tiffcp`'s 26 s; after 5.7 s of transfer the T425 would need a 3.2× faster decode (measured on this link: 1.2× SHA-256, 1.00 LZW) |
| libungif (GIF LZW) | same kernel as `giftopnm` |
| xz, lzip | 64-bit arithmetic / ~94 MB (xz); C++ (lzip) — the INMOS compiler has neither |

Deferred: `arc` (stock binary broken on MiNT), `lzo`, `gmp`/`mpfr`/`mpc` (no program on the mirror
uses them), `rsync` (no peer to measure), `unarj` (no input). Not a compute kernel: `giftrans`.

## Status

Done: W1 (compression), W2 (images), W3 (`bc`). Next: W4 (audio / digests: gsm, libmad, ogg/vorbis,
sox, openssl, gnupg, textutils, sharutils), W5 (ImageMagick, ghostscript). Open: a peak-memory counter
in the servers (board use is reported as allocated, not measured, for `lha`, `zip`, libjpeg).

**Finding worth its own work item:** the 68000 → 68030 rebuild alone gave 0.20–1.00 on `bc` with no T425
at all. Rebuilding the rest of the CPU-heavy SpareMiNT set with `-m68020-60`, and measuring each program,
may be a cheaper win than the T425 for many programs.

## Install (on the TT)

```sh
yum install tbc tbc-t425-on            # a program and its T425 switch
yum install tpnmscale                  # 68030-only twins have no switch
yum list | grep t425-on                # everything that currently offloads
```

## Where the code lives

All in this repository, `sam-yum-tt/` (the offload runtime ships with the package manager):

| Part | Files |
|---|---|
| T425 runtime (TT side) | `src/t4call.c` (boot, framing, adler32, fallback), `src/t4lock.c` (board lock), `src/atwboot.c` |
| T425 server loop | `src/t4serv.c`; one `*serv.c` per port (`compserv`, `lhaserv`, `jpegserv`, `ppmqserv`, `pngserv`, `bcserv`, …) |
| Shims (same code on both CPUs) | `src/pmshim.c` (netpbm I/O in memory), `src/bcshim.c` (bc's stdio), `src/pngenc.c` (PNG encoder) |
| Twins | `src/tgzip.c`, `src/tppmquant.c`, `src/tpnmscale.c`, `src/tgiftopnm.c`, `src/tpnmtopng.c`, `src/tbc.c` |
| T425 builds (INMOS icc under the `t4` emulator) | `t4*.sh` |
| RPM builds | `../scripts/build_t425_*.sh` |

Toolchain notes: the INMOS icc compiler has no `long long` and lexes 64-bit literals even inside a false
`#if`; nothing that asks the host for a service (`getenv`, `time`, `fopen`) may be linked into a server —
the emulator answers those calls, the real board does not.
