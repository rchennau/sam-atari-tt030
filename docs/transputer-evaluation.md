# Transputer evaluation — every TT program (product rule 1)

Operator rule 2026-10-17: every app is evaluated for the T425 and the two TRAMs by default. One row per program
this project ships or wrote for the TT. **Basis** says whether the verdict rests on a measurement or on reasoning
from measured numbers. The second kind is not a measurement and should be confirmed if the verdict matters.

Facts the rows rely on (all measured, README / `docs/resolution-management.md` / `sam-job-tt`):
- `ttjobd` job round trip ≤ 7 ms p50; link 462 KB/s per direction to the T425.
- At pixel clocks ≥ 108 MHz, including the default 1920×1080 mode 32 and mono mode 37, the picture blanks while
  the T425 runs a job. Jobs still complete.
- The T425 sees VRAM from 0x80600000. The TRAMs have no path to VRAM.
- The T425 loses wherever data is large and work per byte small: inflate, LZW, SHA-256, image scaling
  (`sam-t425-ports` "measured and declined").

| Program | Heavy work | Where it runs | Basis | Note |
| :--- | :--- | :--- | :--- | :--- |
| `dropbear` (sam-ssh-tt) | X25519, Ed25519 verify | **68030 in mode 32**, T425 in modes ≤ 102.6 MHz | measured | Offload cuts login crypto 25.0 → 14.4 s, but `DROPBEAR_NO_ATW=1` keeps it off the T425 in the default mode because every login would blank the screen. Mode 32 users pay the slow login. |
| `ttjobd` / `libttjob` (sam-job-tt) | queue for all offload | T425 + 2 TRAMs | measured | The transputer path itself. |
| T425 ports (`tbc`, `djpeg`, `lha`, `tbzip2`, `tgzip`, …) | per port | per port table | measured | Every port timed; losers declined with numbers (`sam-t425-ports/README.md`). Open: FR-8 rows in tt030-ttjob-ports. |
| `yum-tt` + `rpm` | SHA-256, inflate, RSA verify | 68030 | measured | SHA-256 on the T425 lost incl. transfer (`shabench`); inflate 1.42–1.67× slower on the T425. RSA verify is one 256-byte signature per refresh. |
| `ttpkg` (yum GUI) | index parse, list redraw | 68030 | reasoned | No compute-heavy step: 442 rows parsed once; the slow part (`rpm -qa`, 7–15 s) is disk-bound and already removed by `installed.tsv`. Drawing goes through the VDI. |
| `ttkbdd` (sam-kbd-tt) | XChaCha20-Poly1305 per input record | 68030 | reasoned | 1.47 ms per record on the 68030 vs ≥ 7 ms `ttjobd` round trip, and keystrokes are latency-bound, so offload would make every key slower. Mode 32 would also blank per key. |
| `ttsaver` (FR-5/FR-9) | matrix animation into VRAM | 68030 | measured (68030) / reasoned (T425) | 11.7 fps on the 68030 in mode 32. A T425 renderer is possible (it sees VRAM) but blanks the picture in mode 32. Unmeasured in ≤ 102.6 MHz modes. |
| `atwmono` | mono screen copies | card copy engine (FPGA) | measured | Uses the card's blitter, not a transputer. Mode 37 would blank with the T425. |
| `atwshot` | 2 MB VRAM read; transfer | 68030 | reasoned | VRAM read is 0.81 s. The slow step is the transfer over SSH, whose crypto is on the 68030 in mode 32. Compressing on the T425 would blank the picture being captured. `gzip -1` on the 68030 is what's used. |
| `ttmon`, `ttmqtt`, `sntp` | none (one small message per 60 s) | 68030 | reasoned | Network-bound; nothing to offload. |
| `rz`/`sz`/`lrz`/`lsz` | ZMODEM CRC | 68030 | reasoned | Serial-bound at 38400 baud (~3.5 KB/s); the CPU is idle. |
| `ifconfig`, `route` | none | 68030 | reasoned | One-shot configuration. |
| `atwpal`, `vtgprobe`, `atwres`, `mxprobe`, `blitchk`, `fillchk`, `vdiprof`, `vdibench` | none / measurement tools | 68030 | reasoned | Register pokes and probes. The benchmarks measure the 68030→card path on purpose. |
| `sam-rom-tt` | firmware | n/a | — | No-TOS ROM skeleton, unproven, before any OS or ttjobd. |

## Third-party apps (packaged by FHS FR-3, added 2026-10-17)

The first version of this table covered only our own programs. These apps were packaged into `/opt`. Without
source, a program cannot be rebuilt or split for the T425.

| App | Heavy work | Where it runs | Basis | Note |
| :--- | :--- | :--- | :--- | :--- |
| `nethack-atw` (NetHack 3.6.0) | game engine | **T425** (`NETHACK.BTL`) | vendor build | Ships its own T425 image. It talks to the card directly, not through `ttjobd`. Coexistence with `ttjobd` is unmeasured; the README asks for a cold boot. |
| `xptr-tos` (XPTR) | ray tracing | **T425** (184 `ATW\*.BTL`) | vendor build | Same: own T425 images, coexistence with `ttjobd` unmeasured. |
| `turbocad-tools-tos` (TurboCAD 3.6 converters, TBPARTS, TBCAD2TT/TBOUT/TBSHELL) | file-format conversion (DXF/HPGL/ASCII/GEM ↔ TC2), plotting | 68030 | blocker: closed binaries | No source (Technobox, proprietary), so nothing can move to a transputer. Converters are file-I/O-sized jobs. Unmeasured: how long a typical conversion takes on the 68030. |
| `wordplus-tos` (1st Word Plus 3.20) | text editing | 68030 | blocker: closed binary | Interactive and latency-bound. |
| `nvdi-tools-tos` (NVDI 5 utilities, GEM-TEST) | font/printer config, VDI benchmark | 68030 | blocker: closed binaries | Config tools. NVDI drawing itself is replaced by xVDI on the ATW800/2. |
| `highwire-tos` (HighWire 0.5.0 browser) | HTML layout, image decode | 68030 | reasoned (GPL source exists) | Source is available, so a T425 image decoder is possible in principle. Its JPEG/PNG decode is the candidate (`djpeg` on the T425: 0.37 of stock). Not evaluated further; low use. |

## Findings

1. **The default mode blocks transputer use for anything interactive.** Mode 32 blanks the screen during every T425
   job. Today that keeps Dropbear's measured 1.7× faster login on the 68030, and it rules out the T425 for the screensaver and
   screenshots. Batch jobs through `ttjobd` still run and blank briefly. A ≤ 102.6 MHz default (mode 24 1280×960 or
   28 1440×960, both stable with offload since FR-6) would re-enable SSH offload. That is an operator display choice.
2. **No unevaluated program is a likely winner.** Every program not already offloaded is I/O-, latency- or
   serial-bound, or its kernel was measured and lost.
3. **Reasoned rows that could be measured cheaply:** `ttkbdd` per-record cost through `ttjobd` (confirms the ≥ 7 ms
   loss), and a T425 matrix renderer in mode 24 (only if the operator moves off mode 32).
