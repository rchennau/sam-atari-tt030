# llvm-t800 0.2.0 — friction log for the maintainer

Field notes from porting our ATW800/2 T425 servers (built so far with the INMOS D7205 toolset) to llvm-t800
`0.2.0+20260926.g0d979ca.l973377d` (PMS `tt030-llvm-t800-adoption`). Host: Pop!_OS x86-64. Target: TT030 +
ATW800/2 FPGA V0208 T425, 2 MB-video jumper, `-mcpu=t425 -mboard=atw800 -mmem=5M`. Each entry: what happened,
how to reproduce, what we did, and a suggestion. Newest last. Cycle counts are `t4 -st 0`, not hardware timings.

## What worked without friction
- The `.deb`s are relocatable: `dpkg-deb -x` into any directory, and `t800-cc` finds its prefix from its own
  path. No root, no `/opt` needed.
- 64-bit arithmetic, `#warning`, and `__has_feature` all work. The INMOS build needed three source patches to zlib
  (a `Z_U8` probe, `#warning`, and a sanitizer block); clang needed none.
- GNU bc 1.05 builds and gives byte-identical results to the icc build on all six test programs.
- The `-mhost=none` `getenv` is a `ldc 0; ret` stub, so `malloc`'s `morecore` → `getenv` is safe.
- The link map is easy to audit: `__ram_end = 0x80500000` at `-mmem=5M`, so there is one symbol to check.

## F1 — libt800 `<channel.h>` has no `LINK0IN`/`LINK0OUT`
INMOS code writes `ChanOut(LINK0OUT, …)`. libt800's `channel.h` declares `ChanIn`/`ChanOut` but not the link
channel macros; the addresses exist only as `T8_LINK0_OUT`/`T8_LINK0_IN` in `atw800_map.h`.
- Repro: `#include <channel.h>` then `ChanOut(LINK0OUT, b, 4);` → `use of undeclared identifier 'LINK0OUT'`.
- Ours: a guarded fallback in `t4serv.c` defining them from `atw800_map.h`.
- Suggest: define `LINK0IN..LINK3OUT` (and `EVENT`) in `channel.h`, as the INMOS header does.

## F2 — `warn` in libc.a collides with an application's own `warn`
`posix-t425.o` in `sysroots/libt800/lib/t425/libc.a` defines `warn` alongside other POSIX functions. Once
anything else pulls that member in, a program that defines its own `warn` (GNU bc's `util.c` does) fails to link.
- Repro: build GNU bc 1.05 → `ld.lld: error: duplicate symbol: warn` (util.c vs posix-t425.o).
- Ours: `-Dwarn=bc_warn`.
- Suggest: split `err`/`warn` into their own archive members, or make them weak.

## F3 — `<signal.h>` uses `pid_t` without declaring it
`signal.h:54 int kill(pid_t, int)` relies on `<sys/types.h>` already having been included. With a project-local
`sys/types.h` on `-I.` (our INMOS-era stub), it fails.
- Repro: `-I.` with a `sys/types.h` lacking `pid_t`, then `#include <signal.h>` → `unknown type name 'pid_t'`.
- Ours: dropped the stub for the clang build. Our stub was the real cause.
- Suggest (minor): `signal.h` could take `pid_t` from an internal header so that it does not depend on what `sys/types.h` resolves to.

## F4 — `-monchip-stack` on the T425 overflows silently, and `-mstack-check` does not catch it
- Repro: zlib 1.3.2 + bzip2 1.0.8 test (`compt4test.c`, ours), `-O2 -monchip-stack -mstack-check`, then
  `t4 -s4 -sm 23 -sr -ss -sb vs.btl`. Two of four results print, then `-E-EMU414: Error - bad Icode! (#FA)` at
  `IPtr #00000602 WPtr #00000600`, which is execution inside the on-chip stack area.
- Expected: `-mstack-check` stops the program with a stack-overrun report, as the README says.
- Ours: not using `-monchip-stack`. The README frames it for the T800 TRAM, and it is unclear whether it applies
  to the T425.
- Suggest: say whether `-monchip-stack` is supported on the T425; make `-mstack-check` cover the on-chip layout.

## F5 — performance: compression runs 1.22× icc's cycles; bc runs 0.68–0.87×
The same sources and workloads, output identical:

| Workload | `-sd` | icc (D7205) cycles | clang `-O2` cycles | clang / icc |
|---|---|---:|---:|---:|
| zlib+bzip2 test (z/Z/b/r) | 0 | 1,064,440,407 | 1,303,764,859 | 1.22 |
| same | 2 | 1,482,172,179 | 1,803,544,803 | 1.22 |
| bc `scale=40; 4*a(1)` | 2 | 1,588,477 | 1,086,693 | 0.68 |
| bc `f(120)` factorial | 2 | 10,725,466 | 9,280,139 | 0.87 |

`-O3`, `-Os` and `-fno-unroll-loops` give the same result within 0.5 %. The compression code is byte and
bit-twiddling on `unsigned char`/`unsigned short` arrays (deflate's hash chains, the bzip2 block sort), which
could be a code-generation case worth a look.

**On the real T425** (ATW800/2 V0208, 2 MB-video jumper; the call timed after boot, 3 rounds, output identical):

| Workload | icc best ms | clang worst ms | clang / icc |
|---|---:|---:|---:|
| bc `scale=500; sqrt(2)` | 3,310 | 2,820 | 0.85 |
| bc `-l` 200 digits of π | 10,270 | 9,635 | 0.94 |
| bc `2^20000` | 18,365 | 17,515 | 0.95 |
| zlib gzip -6, 256 KB text | 8,590 | 8,940 (typical) | 1.04 |
| zlib raw deflate -6, 256 KB text | 8,370 | 8,705 | 1.04 |
| bzip2 -3, 256 KB binary | 18,755 | 16,960 | 0.90 |

Deflate (zlib `deflate.c` longest_match / hash insert) is the one case where clang loses to INMOS icc on hardware.

## F6 — `t4` prints `RESULT 0` for clang programs that return from `main`
The icc builds under `t4` print only their own output; clang builds add a `RESULT 0` line. It is harmless, but
it breaks a naive `diff` of the two runs.
- Suggest: document the line, or print it only with `-si`.

## F7 — `t4` cycle ratios did not predict the hardware ratios
Same images, same workloads: `t4 -st 0` (with `-sd 0` and `-sd 2`) predicted clang ÷ icc = 0.68 / 0.87 for bc and 1.22
for the compression test; the real T425 measured 0.85 / 0.94 for bc and 0.90 (bzip2) to 1.04 (deflate). `-sd 2` changed
the absolute counts but not the ratio. We gate on hardware only, as the plan already says; a note in the README
that `t4` ratios between compilers are not a hardware predictor would have saved us a wrong expectation.
Suggest: if the V0208 FPGA's memory timing is known, a `-sd`/`-sm` preset for "ATW800/2 T425" in `t4`.

## F8 — (positive) boot on our own loader
A `-mhost=none` `.btl` boots on our link loader (raw image down link 0, then wait for the program's first bytes)
with no start-up traffic at all: our loader logged no iserver packet before the server's `RDY0`. Boot time 975–1,050 ms
vs 985–1,075 ms for the INMOS-linked image of the same program.

## F9 — `t800-run -ext` defaults to a 256 KB RAM; running out looks like a program bug
lha's encoder (`lhat4test.c` + SpareMiNT `slide.c`, ~400 KB of tables) under `t800-run -mcpu=t425 -ext -O2 -lc` returned
-1 from every call, identically at -O0/-O2/-Os. Cause: `t800-ext.ld` sizes RAM at 256 KB unless `__ram_end` is defined,
so `malloc` returned NULL. Nothing on the console says the heap is exhausted. `--defsym=__ram_end=0x80500000` (the
ATW800/2's 5 MB) fixed it: output identical to the host build.
- Suggest: a `t800-run -mmem=SIZE` (as `t800-cc` has), or print the RAM size the image was linked for; a heap-exhausted
  note from libt800's `malloc` under `t4` would also have named it at once.
- Second half of the same trap (found the same day): `t4` itself emulates **2 MB** unless given `-sm` (man t4: default
  21 bits), and `t800-run` passes no `-sm` when `__ram_end` is larger. Linking for 5 MB then runs fine until the heap
  crosses `0x80200000`; above it memory aliases and libt800's free list corrupts. Repro: allocate 520 × 4 KB, free all,
  allocate again -> hangs in `free` (500 × 4 KB passes; threshold is the 2 MB line, not quadratic: 400 blocks 0.1 s).
  It looked like a libt800 heap bug until `T800_T4_FLAGS='-sm 23'` made it vanish. ppmquant's test (120k small
  allocations) hit it.
- Suggest: `t800-run` derive `-sm` from `__ram_end` (or `-mmem`), or `t4` trap accesses beyond its memory instead of
  aliasing.

## F10 — libt800 lacks `<sys/file.h>`, `<sys/dir.h>`, `<dirent.h>`
Unix-era sources (SpareMiNT lha 1.14i `lha.h`) include them unconditionally even when the kernel uses none of their
contents. We add empty stubs (as for the INMOS toolset). Minor; empty headers in libt800 would let such code build as is.
Note the reverse trap: a stub `<sys/types.h>` written for icc shadows libt800's own and breaks `<signal.h>` (`pid_t`).

## F11 — `t800-run` takes no `.o` inputs, and `--no-relax` comes only with `--inmos-libc`
Linking our own icc-compiled kernels (converted with `tcoff2elf`) next to a clang `main` is exactly what `tcoff2elf`
enables, and it works: 16 converted zlib/bzip2 objects + a clang test main run under `t4` byte-identical to a host
build (`sam-job-tt/fr2_mixed_link.sh`, 2026-10-15). But `t800-run.sh` rejects `*.o` ("unrecognized argument"), and
the `--no-relax` that converted modules need (raw branch operands) is only set by `--inmos-libc`, which also swaps
the whole C library. We run a patched copy. Suggestion: accept `.o`/`.a` inputs, and add `--no-relax` whenever an
input came from `tcoff2elf` (or a plain `--no-relax` flag), independent of `--inmos-libc`.

## F12 — `T8TTP` finds neither its appended image nor a `.BTL` beside it (FreeMiNT, ext2)
`mkprg mixed.btl -o MIXED.TTP`, uploaded byte-identical (md5 checked) to `/f/root/fr5/mixed.ttp` on the TT's ext2 drive:
running it prints "T8TTP: no transputer program: none appended, no .BTL beside it, none on the parameter line" and waits
for a key — also with `mixed.btl` next to it. Naming the image on the command line works (`./mixed.ttp mixed.btl`).
Suspect: the runner locates itself by an upper-case 8.3 name or via a TOS path call that MiNT's ext2 lowercase file
does not match. Also: the "-- key --" wait hangs a non-interactive (serial/ssh) caller; suggestion: skip the wait
when stdin is not a terminal. 2026-10-15.

## F13 — `ProcPar` returns at once instead of waiting for its processes
Repro (`sam-job-tt/llvm/procpar_repro.c`, `t800-cc -mcpu=t425 -mboard=atw800 -mmem=5M`, run under `t4`): three processes
— a producer that sends 3 ints then `ProcStop()`s, a relay that loops forever, a consumer that prints 3 then `ProcStop()`s.
`ProcPar(p1, p2, p3, NULL)` returns before the consumer prints anything, and the line after it printed **twice**. In our
resident job image `main` then returned into `exit()` right after its ready marker, so every job timed out on the card.
Workaround: `ProcRun()` each process, then `ProcStop()` in `main`. Suggestion: make `ProcPar` wait (as INMOS's does), or
document that it does not. 2026-10-15.
