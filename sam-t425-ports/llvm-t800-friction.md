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
