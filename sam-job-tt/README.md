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
