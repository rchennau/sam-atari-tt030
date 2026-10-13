# sam-tram-tt — ATW800/2 TRAM probes

Two Transtech TTM15F-45-F TRAMs (Slot 1 s/n 43626004, Slot 2 s/n 8071377) on the ATW800/2. Measured on the real
TT 2026-10-12:

```
T425 link 1 ── link 3 [Slot-1 TRAM] link 1 ── link 0 [2nd transputer]   (T425 host link = 0)
```

| file | runs on | does |
| :--- | :--- | :--- |
| `linkprobe.c` | T425 | boot-protocol PEEK on T425 links 1–3: which links have an unbooted transputer |
| `tramprobe.cfs` + `rootp.c` + `tramp.c` | T425 + Slot-1 TRAM | icconf network: boots the Slot-1 TRAM, which PEEKs its own links and reports |
| `tram2.cfs` + `root2.c` + `relay.c` + `t2.c` | T425 + both TRAMs | 3-node network: boots the 2nd transputer through a relay on Slot 1; reports its loop time, RAM and the 1/i² sum. `tram2a` builds it `-ta`, `tram28` `-t8` (FPU) |
| `tramlink.cfs` + `rootb.c` + `echo1.c` + `echo2.c` | all three | link round-trip benchmark, 4 B–16 KB, 1 and 2 hops |
| `tramchar.cfs` + `rootc.c` + `tramc.c` + `charcommon.h` | T425 + Slot-1 TRAM | device id, loop timing, RAM size of Slot-1 and (by POKE/PEEK) the next transputer |

`./build.sh` builds both into `build/`. Traps: a raw `icollect -t` image forwarded by the T425 does not run on
the TRAM (the C runtime needs the configurer's loader); a wrong `Tram.link[N]` hangs `iserver` — recover with
`scripts/tt_reboot.sh --wait-back`, never `kill -9` (that crashed XaAES, exception 4 at pc 0x3AE0).
`tramchar.cfs` + `rootc.c` + `tramc.c` (2026-10-12): Slot-1 RAM 4 MB and the 2nd transputer 4 MB (marker write wraps at 4 MB; the 2nd is sized over boot-protocol POKE/PEEK, unbooted); Slot-1 runs a fixed loop in 30,012 ticks vs 14,743 on the 40 MHz T425, so **~20 MHz** if cycles per iteration match (estimate: memory wait states differ). `lddevid` read 2 on both the T425 and the TRAM, so it does not tell them apart; CPU type (T805 vs T4) still open. **FPU test (`tramfpu` vs control `tramfps`): Slot-1 halts on `-t8` FPU code and runs the identical `-ta` software-float build correctly (1644924, 184,516 ticks vs 83,724 on the T425) — no working FPU, so a T4-class part, not the T805 the vendor class suggests** (confirm by reading the chip marking). Trap: INMOS C signed `+=` is overflow-checked, so an overflowing loop halts the processor.

**2nd transputer (2026-10-12, `tram2`):** up-link is its **link 0** (first trial). Loop 30,012 ticks (= Slot 1, ~20 MHz), RAM 4 MB, software-float sum correct (1644924, 187,831 ticks). The `-t8` FPU build halts it, like Slot 1: **neither TRAM has an FPU** — both are T4-class parts. `size_self` moved into `charcommon.h` for reuse (tramchar rebuilt, not re-run).

**Link round-trip benchmark (2026-10-12, `tramlink`):** echo round-trips from the T425, 1 hop (Slot 1) and 2 hops
(relayed by Slot 1 to the 2nd transputer). Fixed cost **22 µs per round-trip at 1 hop, 45 µs at 2 hops**; payload
**1.58 MB/s per direction at 1 hop** (16 KB), close to the ~1.7 MB/s ceiling of a 20 Mbit/s link, so the links run
at 20 Mbit/s. 2 hops is exactly half (0.83 MB/s): the relay stores and forwards, nothing overlaps. Since each TRAM
computes at about half the T425's speed (loop 30,012 vs 14,743 ticks), offload to the TRAMs only pays as
parallel work alongside the T425, never as a faster processor; the 68030 ↔ T425 link is not in this measurement.

**The 68030 ↔ T425 hop (2026-10-12, `sam-yum-tt/src/linkrt.c`, op `E` echo through the TT link driver):**
fixed cost **~0.83 ms per round-trip** and **436 KB/s per direction** (16 KB; matches `shabench`'s 459 KB/s one
way), 0 short driver writes. That is ~37× the T425 ↔ TRAM fixed cost and ~3.6× slower per byte, so for any job
that starts on the TT the 68030 hop dominates: round-trip ≈ 0.83 ms + 4.6 µs per byte (in + out).

**Hop optimisation, option 1 (2026-10-12):** `atwboot.c` no longer calls `clock()` (itself a trap under MiNT) on
every driver call — the stall deadline starts only after a zero-progress call — and `linkrt` can merge the 6-byte
request header into the first data write. 4-byte round-trip **837 → 250 µs (lazy clock) → 162 µs (merged
header)**, ~5× lower fixed cost; per direction at 16 KB 436 → 462 KB/s (the per-byte limit is inside the
`fpgabios` driver, not these calls). Installed packages (`t4call.c` clients) get this only when rebuilt.

**SharedMem located from the T425 (2026-10-12, `sam-yum-tt/src/shmprobe.c` + `shmscan.c`, read-only scan):** the
68030 wrote a 16-byte marker at SharedMem (TT `0xFEDF4000`, Programmer's Manual p.5); the T425 found it at
**`0x807F4000`** (alias `0x809F4000`), bytes in the same order. So the ATW800/2 video memory (TT `0xFEC00000`) is
**T425 `0x80600000`–`0x807FFFFF`**, repeating every 2 MB — T425 RAM beyond 6 MB is VRAM, which explains the
2026-09-25 memserv blackout. 68030 copies: **3.77 MB/s into SharedMem, 2.93 MB/s out** (32 KB × 50, supervisor
memcpy), 8× / 6× the link's 462 KB/s. SharedMem is 39 KB (`0xFEDF4000`–`0xFEDFDBFF`), saved and restored by the probe.

**SharedMem coherency (2026-10-12, `sam-yum-tt/src/shmab.c` + `shmserv.c`, `shmwhere.c`, `shmrt.c`):** T425 writes
to SharedMem land where the 68030 reads them (`shmwhere`: marker at `0xFEDF4000` as written). T425 reads 3.9 MB/s,
writes 3.9 MB/s. But **the T425 side keeps a stale copy of video memory it has already read: after the 68030
rewrites a block the T425 read before, the T425 reads the old bytes** (diagnostic: "68030 before 0512, T425 before
faed"). Not a 68030 effect — clearing the 68030 data cache, waiting up to 200 ms, or a 68030 flush write changed
nothing; reading through the 2 MB alias changed nothing. **Reading other video memory first fixes it:** 64-byte
mailbox, wrong rounds of 20 by eviction-read size: ≤4 KB 19–20, 8 KB 9, 16 KB 3, **32 KB 0** (~8 ms), 64 KB 0
(~16 ms). So the stale copy is 16–32 KB. Design rule: before the T425 reads a 68030-written block, read 32 KB of
other video memory (read-only, from `0x80600000`), or accept that only never-before-read addresses are fresh.
Each half of a mailbox is written by one side only.
