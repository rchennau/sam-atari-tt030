# SAM YUM TT (`sam-yum-tt`) — package installs for the Atari TT030

> **Status: ALPHA — proof of technology, question answered.** There is no `yum` client yet. This
> directory holds the measurement that decided how the client will be built: **the T425 is not
> worth using for package hashing** (real-TT numbers below). Nothing here installs software.

`sam-yum-tt` is the TT-side half of the SAM package pipeline: `yum install <package>` at the TT's
own shell, pulling SpareMiNT RPMs from a LAN mirror on fractal (plan: SAM monorepo
`maestro/tracks/tt030-rpm-pipeline/`, locked 2026-09-14).

## What the proof of technology tests

A yum client spends its CPU on two things the 68030 does slowly: SHA-256 of every downloaded
package and a signature check of the package index. The T425 already runs Ed25519 verify for
[`sam-ssh-tt`](../sam-ssh-tt/README.md) (6.4 s vs 9.4 s on the 68030). SHA-256 is unmeasured, so:

| Part | Runs on | Does |
|---|---|---|
| `src/sha256.c` | both | C89 SHA-256 on 32-bit `unsigned long`, shared by both sides |
| `src/shaserv.c` | T425 | link server: `E` echoes data (link rate), `H` hashes it |
| `src/shabench.c` | TT | times 68030 SHA-256, link echo, and T425 SHA-256 incl. transfer; checks both digests agree |
| `src/sha256test.c` | build host | prints digests like `sha256sum`, for checking the core |

**Decision rule:** build a T425-assisted native client only if the T425 hash, transfer included,
is clearly faster than the 68030. Otherwise the planned `sh` + `awk` client stays.

## Results so far

| Check | Result |
|---|---|
| SHA-256 core vs `sha256sum` (11 files, padding edges, 1 MiB, 3 MiB) | all match (2026-09-17) |
| Core on the T425 emulator `t4` | matches (2026-09-17) |
| 68030 build in Hatari | digest correct; **137 KB/s** (emulated timing) |
| **Real TT (2026-09-19)** | 68030 **70 KB/s** (7,225 ms); link one way **459 KB/s**; T425 incl. transfer **69 KB/s** (7,335 ms); digests agree |

**Verdict (measured 2026-09-19): do NOT offload hashing to the T425.** Net of the 1,115 ms
transfer, the T425 hashes ~512 KB in ~6,220 ms (~82 KB/s) against the 68030's 70 KB/s — about
1.2×, and shipping the data there costs more than that gain. The planned POSIX `sh` + `awk` client
stands; no native C client is justified by hashing.

Hatari had estimated the 68030 at 137 KB/s — **the real machine is half that**, and below the
~115 KB/s WiFi download, so SHA-256 is the slowest step of an install either way. The remaining
offload candidate is unaffected: an Ed25519 *signature* check is a tiny payload, where the T425's
measured 1.44× still applies.

## Layout

```
sam-yum-tt/
├── Makefile        make host | tt | t4   (outputs in build/)
├── build/          build output (git-ignored, except .gitkeep)
├── docs/           BUILD.txt (toolchain notes), design.md
└── src/            sha256.[ch], shaserv.c, shabench.c, sha256test.c
```

## Build

Needs the cross toolchains in `../tools/` (`cross-mint`, `d72uni` + `t4`).

```bash
cd sam-yum-tt
make host   # then: build/sha256test FILE  vs  sha256sum FILE
make tt     # build/shabench.ttp
make t4     # build/shaserv.btl
```

## Run on the TT

1. Copy `build/shabench.ttp` and `build/shaserv.btl` to the TT.
2. Run `fpgabios.tos`, then `shabench SHASERV.BTL 512` (the second argument is the buffer in KB).
3. Without an argument, `shabench` measures only the 68030 (works without an ATW800/2, e.g. Hatari).

`shabench` resets the T425, which stops the `sam-ssh-tt` offload server; the next SSH login boots
it again. XPtr, NetHack and Helios take the T425 the same way.

## Hardware

- Atari TT030 (MC68030 @ 32 MHz), FreeMiNT or TOS
- ATW800/2 with its T425, `fpgabios.tos` resident (link driver)
