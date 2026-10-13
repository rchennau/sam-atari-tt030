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
| `tramchar.cfs` + `rootc.c` + `tramc.c` + `charcommon.h` | T425 + Slot-1 TRAM | device id, loop timing, RAM size of Slot-1 and (by POKE/PEEK) the next transputer |

`./build.sh` builds both into `build/`. Traps: a raw `icollect -t` image forwarded by the T425 does not run on
the TRAM (the C runtime needs the configurer's loader); a wrong `Tram.link[N]` hangs `iserver` — recover with
`scripts/tt_reboot.sh --wait-back`, never `kill -9` (that crashed XaAES, exception 4 at pc 0x3AE0).
`tramchar.cfs` + `rootc.c` + `tramc.c` (2026-10-12): Slot-1 RAM 4 MB and the 2nd transputer 4 MB (marker write wraps at 4 MB; the 2nd is sized over boot-protocol POKE/PEEK, unbooted); Slot-1 runs a fixed loop in 30,012 ticks vs 14,743 on the 40 MHz T425, so **~20 MHz** if cycles per iteration match (estimate: memory wait states differ). `lddevid` read 2 on both the T425 and the TRAM, so it does not tell them apart; CPU type (T805 vs T4) still open. **FPU test (`tramfpu` vs control `tramfps`): Slot-1 halts on `-t8` FPU code and runs the identical `-ta` software-float build correctly (1644924, 184,516 ticks vs 83,724 on the T425) — no working FPU, so a T4-class part, not the T805 the vendor class suggests** (confirm by reading the chip marking). Trap: INMOS C signed `+=` is overflow-checked, so an overflowing loop halts the processor.

**2nd transputer (2026-10-12, `tram2`):** up-link is its **link 0** (first trial). Loop 30,012 ticks (= Slot 1, ~20 MHz), RAM 4 MB, software-float sum correct (1644924, 187,831 ticks). The `-t8` FPU build halts it, like Slot 1: **neither TRAM has an FPU** — both are T4-class parts. `size_self` moved into `charcommon.h` for reuse (tramchar rebuilt, not re-run).
