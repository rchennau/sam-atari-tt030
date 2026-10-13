# sam-job-tt — TT030 transputer job queue (track `tt030-transputer-job-queue`)

Resident jobwire network on the ATW800/2: T425 dispatcher + both TRAMs (`t4/jobfull.cfs`), and `ttjobd`, the one
68030 process that owns the link and serves local clients over `/var/run/ttjob.sock`.

| file | runs on | does |
| :--- | :--- | :--- |
| `jobwire.h` | both | wire format (plan Eng §2): request/reply headers, ops, status codes |
| `t4/root.c`, `t4/worker1.c`, `t4/worker2.c`, `t4/jobnode.c` | T425, TRAM 1, TRAM 2 | execute or forward one job; adler32 on request and reply |
| `ttjobd.c` | 68030 | takes `/tmp/t425.lock`, boots the image once, serves the socket, `ST_DOWN` when the link is gone |
| `ttjobcli.c` | 68030 | Phase 1 test client: echo + SHA-256 per node vs the 68030, corrupted request, round-trip timing |

Build: `./build_t4.sh` (image → `build/jobfull.btl`); the two `.ttp` files with cross-mint gcc (see progress).

**Phase 1 result (real TT, 2026-10-12):** image booted once; on every node echo 4 B / 1 KB / 16 KB and SHA-256 of
16 KB match the 68030, and a corrupted request returns `ST_BADREQ`. 4-byte round-trip through `ttjobd`: **3.96 ms
(T425), 4.03 ms (TRAM 1), 4.19 ms (TRAM 2)** against the NFR-1 target of ≤ 0.5 ms — the link itself is 0.16 ms, so
~3.8 ms is the Unix-socket hop between two MiNT processes. TRAM hops add ~0.1 ms each.

**Socket-hop experiments (2026-10-12, `ipcbench.c`, operator MCQ option 2).** 4-byte ping-pong between two idle
processes, no link: Unix socketpair 60–62 ms, path-bound socket 58–61 ms, pipe pair 61–63 ms per round-trip;
`select()` in the server, `Syield()` after each write and priority −20 change nothing (all within ±3 ms). The same
hop inside the `ttjobd` path measured 3.96 ms, so FreeMiNT 1.19's wake-up latency between processes, not the IPC
type, sets the floor, and it depends on what else is running. A client that waited by non-blocking polling with
`Syield()` livelocked: `ttjobd` (sleeping in `select()`) never ran, the client spun 4 min until killed — so that
variant was removed. Conclusion: ≤ 0.5 ms through any separate daemon is not reachable on this kernel without
scheduler work.

**Phase 2 (real TT, 2026-10-12):** wire gains a level byte (13-byte header) and a 400 KB per-job cap; the T425 node
carries the compression kernels (`compkern`, `-t4`, 2 MB heap); `libttjob.c` (submit / poll / wait, replies per
connection, results wiped after reading); `sam-yum-tt/src/t4call.c` routes through `ttjobd` when built with
`-DT4CALL_TTJOB` and the socket answers (no daemon → the old direct path). `tgzip` rebuilt with no change to
`tgzip.c`. On a 2,125,272-byte file: **gzip via `ttjobd` = 9 of 9 pieces on the T425**, output decompresses to the
input, and differs from the 68030's in exactly 9 bytes — the OS byte of each gzip member (`03` T425 zlib build vs
`05` 68030 build); compressed data identical. **bzip2 -1 via `ttjobd`: byte-identical** to the 68030's and
decompresses to the input. bzip2 at the default level 9 fails on the T425 (needs more than the 2 MB heap) and falls
back to the 68030; it then reported "op not in the resident image" — now `ST_FAILED` ("kernel failed on the T425"),
built but not yet re-run on the TT. Timings were not captured in this run.

**Phase 2 gaps closed (real TT + fractal, 2026-10-13):** T425 deflate built with `-dATARI` → **gzip via `ttjobd`
is now byte-identical to the 68030's** (md5 `2086e583…` both). Timings on the 2,125,272-byte file: gzip **121 s
via `ttjobd` vs 196 s on the 68030 (0.62×)**; bzip2 -1 **201 s vs 359 s (0.56×)**, byte-identical. bzip2 at
level 9 now reports "kernel failed on the T425" (`ST_FAILED`) and falls back. Host test `test_libttjob.c` (fake
`ttjobd`): no daemon, out-of-order collection, poll, cap too small, damaged checksum, bad op, lost connection —
all pass; a mutant without the reply-checksum check aborts.

**Emulated test lane (2026-10-13, `emu/run_emu_tests.sh`, fractal only, ~2 min at 1,000 jobs):** (1) `test_libttjob`;
(2) `emu/jobtest.c` — JOBS pseudo-random jobs per op through `t4/jobexec.c` (the op code the resident image runs,
factored out of `jobnode.c`), built with host gcc and with INMOS icc `-t4` under the `t4` emulator (`-sm 23`); the
digest listings must be identical, every job must succeed, SHA-256 known answers must match, inflate and bzip2 must
round-trip. Result: **1,000 jobs × 7 ops identical, 0 failures**. Mutation checks: a T425-only SHA fault
(`EMU_MUTANT=1`) fails the diff; a fault shared by both builds fails the known-answer check — the first mutation
attempt (shared fault, no known answers) passed the diff, which is why the known answers exist. Not covered here:
the real link driver, FreeMiNT, the TRAMs' missing FPU (t4 emulates an FPU), timings.

**CI (2026-10-13, FR-9):** every commit in this repo (and 03:40 nightly) runs `scripts/ci.sh` on fractal via
`sam-tt030-ci.{path,timer,service}` (`iac/fractal`). Result: `~sam/.local/state/tt030-ci/last.txt`, full log
`last.log` alongside, and the `tt030.ci.run` event in the central log; `journalctl -u sam-tt030-ci`.

**Phase 3 crypto ops (real TT + emulated lane, 2026-10-13):** `X` X25519 (xserv2's c25519 + 32-bit field maths),
`V` Ed25519 verify (`edsign`, 32-bit SHA-512), `C` ChaCha20 (`t4/chacha20.c`, RFC 8439, 32-bit only — Monocypher
needs 64-bit types icc lacks); on every node. Known answers from outside this code: RFC 7748 §5.2, RFC 8032 §7.1
test 1 (valid and one-bit-corrupted), RFC 8439 §2.4.2 (ciphertext computed with Python `cryptography`). Emulated
lane: host vs `t4` identical incl. random X/V jobs (signatures made with `edsign_sign`, every second one corrupted).
On the TT via `ttjobd`, all KATs pass on all nodes: X25519 T425 2,730 ms / TRAM 1 5,655 / TRAM 2 5,490;
Ed25519 verify 6,430 / 13,015 / 12,965 ms. TRAM 2's 4-byte round-trip read 27.0 ms this run (4.19 before) — not
yet explained; my SSH polling was running. First start failed "T425 lock not ours": Dropbear's own per-login
X25519 offload held it — the conflict FR-5 removes.

**FR-5 Dropbear on `libttjob` (real TT, 2026-10-13; test binary on port 2222, production sshd untouched):**
`staging/DROPBEAR/atwx25519.c` asks `ttjobd` first (`atw_via_ttjobd`): reachable → X25519 / Ed25519 verify through
it; reachable but failing → the 68030 (never the direct path, whose lock `ttjobd` holds); no daemon → the old direct
path. Measured: with no `ttjobd`, the new binary used the direct path (2.8 s X25519) — fallback works; with
`ttjobd`, logins log "x25519 via ttjobd took 2735 ms" / "ed25519 verify via ttjobd took 6445 ms" (same as direct);
**a login during a 2.1 MB `tgzip` run got T425 offload via `ttjobd`** (before: "T425 busy" → 68030), login 50 s vs
17 s idle, `tgzip` 144 s vs 121 s alone, output still byte-identical. Starting `ttjobd` raced each SSH login's
direct-path lock twice ("T425 lock not ours"); a shell retry loop worked — FR-2's boot step must retry the lock.
Cosmetic: curve25519-monocypher.c's own line still says "on the T425" after a ttjobd call.
