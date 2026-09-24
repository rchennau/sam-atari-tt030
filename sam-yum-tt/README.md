# SAM YUM TT (`sam-yum-tt`) — package installs for the Atari TT030

> **Status: working on the real TT.** `yum makecache | list [pat] | search pat | install [--nodeps] pkg… |
> update [pkg…] | remove pkg…`, plus build-on-miss for packages the mirror lacks.

`sam-yum-tt` is the TT-side half of the SAM package pipeline: `yum install <package>` at the TT's own
shell, pulling SpareMiNT RPMs from a LAN mirror (plan: SAM monorepo
`maestro/tracks/tt030-rpm-pipeline/`, locked 2026-09-14, revisions 1–5). The mirror is
`http://mirror.sam.int/tt030/` (NAS `/vault/tt030`, served by CT104's Caddy to LAN sources only), 420
packages, index RSA-signed on fractal.

| Part | What it does |
| :--- | :--- |
| `src/yum` (bash) | the client: fetch + verify the index, resolve, fetch + SHA-256 each package, one `rpm -i` |
| `src/resolve.awk` | dependency closure over installed names, on-disk paths and mirror provides |
| `src/update.awk` | rpmvercmp against the index: what has a newer version (never downgrades) |
| `../src/ttmqtt.c` | MQTT 3.1.1 `pub` / `sub -W -E -S` for build-on-miss |
| `../src/ttsign.c` | Ed25519 (Monocypher) signature on each build request — the card holds the key |
| `etc/index.pub` | the mirror's public signing key, shipped in the `yum-tt` RPM |

Built and packaged by `../scripts/build_yum_tt.sh`; installed on the TT like any other package.

**Measured on the TT:** `makecache` 17.7 s · `install less` 46.7 s · `install pv` with pv absent (built
on fractal first) 1 m 47 s · `update` of the client itself 35 s · dependency walk over a 415-row index
6.1–6.6 s with `mawk`.

**Limits:** versioned requirements are not resolved (the index keeps the newest entry per name); a
package with no recipe is queued for a human; the TT must have `mawk`, `wget`, `ttmqtt` and bash.

## Why the client is pure software (the T425 probe)

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
is clearly faster than the 68030. Otherwise the client stays pure software on the 68030.

## Results so far

| Check | Result |
|---|---|
| SHA-256 core vs `sha256sum` (11 files, padding edges, 1 MiB, 3 MiB) | all match (2026-09-17) |
| Core on the T425 emulator `t4` | matches (2026-09-17) |
| 68030 build in Hatari | digest correct; **137 KB/s** (emulated timing) |
| **Real TT (2026-09-19)** | 68030 **70 KB/s** (7,225 ms); link one way **459 KB/s**; T425 incl. transfer **69 KB/s** (7,335 ms); digests agree |

**Verdict (measured 2026-09-19): do NOT offload hashing to the T425.** Net of the 1,115 ms
transfer, the T425 hashes ~512 KB in ~6,220 ms (~82 KB/s) against the 68030's 70 KB/s — about
1.2×, and shipping the data there costs more than that gain. The `sh` + `awk` client
stands; no native C client is justified by hashing.

Hatari had estimated the 68030 at 137 KB/s — **the real machine is half that**, and below the
~115 KB/s WiFi download, so SHA-256 is the slowest step of an install either way. The remaining
offload candidate is unaffected: an Ed25519 *signature* check is a tiny payload, where the T425's
measured 1.44× still applies.

## Rev. 7: compression on the T425 (2026-09-24)

Operator: packages that have to be built or ported should use the transputer by default — measure
first. `compbench` (68030) and `compserv` (T425) run the **same** zlib 1.3.2 / bzip2 1.0.8 C
(`src/compkern.c`; T425 build `t4compress.sh`, `make comp`) on the first 256 KB of a file. Every
T425 output was **byte-identical** to the 68030's. Real TT, times include sending the input and
receiving the output over the link:

| Input (256 KB) | Kernel | 68030 | T425 incl. transfer | T425 / 68030 |
|---|---|---|---|---|
| `/etc/termcap` (text) | deflate -1 | 6,240 ms | 4,655 ms | 0.74 |
| | deflate -6 | 11,050 ms | 8,475 ms | 0.76 |
| | bzip2 -1 | 23,340 ms | 16,545 ms | 0.70 |
| `/bin/bash` (binary) | deflate -1 | 9,445 ms | 6,315 ms | 0.66 |
| | deflate -6 | 17,170 ms | 11,945 ms | 0.69 |
| | bzip2 -1 | 24,955 ms | 20,215 ms | 0.81 |

64 KB of `index.tsv`: 0.66 / 0.62 / 0.74. So for compression-class work the T425 is **1.2–1.6×**
faster *including* the link, unlike SHA-256 (1.0×), because compression does more work per byte
moved. One run per input (n = 1), not a distribution.

**Link finding (root cause found the same day):** a ~96 KB T425 result reached the TT **damaged**
(the T425's own adler32 of it was right) and the stream then misaligned. It first looked like an
FPGA burst limit and was worked around with TT-paced 4 KB chunks. **The cause was on the TT side:**
the reader asked the fpgabios driver for the whole ~96 KB in one `OP_READ` (106) call, and the
driver takes at most `0x7FFF` bytes per call (iserver's `ReadLink` refuses more). A/B against the
same unpaced server: uncapped read → damaged, then no reply; capped at `0x7000` → byte-identical on
all three kernels. `atwboot.c`'s `link_read` now caps every call, like `link_write` always did, and
the pacing is gone (it had cost nothing measurable either). Other link clients (Dropbear's X25519
offload, `sam_scp_tt`, `atwxserv`, `xclient`) never read more than 1,024 bytes per call.
`bzip2 -1` on 256 KB also needs
`IBOARDSIZE #400000` (4 MB); the `t4` emulator runs out of memory at that size.

### First user: `tgzip` / `tbzip2` (`t425-compress` RPM, 2026-09-24)

`yum install t425-compress` → `/usr/bin/tgzip`, `/usr/bin/tbzip2` (`src/tgzip.c`, built by
`scripts/build_t425_compress.sh`). The input goes to the T425 in 256 KB pieces, each returned as a
gzip member / bzip2 stream (they concatenate, so stock `gunzip` / `bunzip2` read the file). A piece
falls back to the 68030 (same code) when fpgabios is absent, `/tmp/t425.lock` names a live pid, or
a piece fails or arrives damaged (adler32 check), and the fallback is printed. Separate names, not a replacement
for SpareMiNT's `gzip`: `tar -z` and rpm scripts rely on its full flag set.

| Real TT | stock | T425 | output |
|---|---|---|---|
| `/etc/termcap` 429 KB, gzip -6 | 22.0 s, 146,087 B | **18.3 s**, 147,424 B (2/2 pieces) | `gzip -t` ok, round-trip identical |
| `/bin/bash` 542 KB, bzip2 | -9: 57.5 s, 253,793 B | **47.1 s** (-9 → 3), 254,651 B (3/3) | `bzip2 -t` ok, round-trip identical |

Times include booting `compserv` (a run resets the T425; Dropbear's X25519 offload restarts at the
next SSH login). Piece boundaries cost ~0.3-0.9 % in size.

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
