# sam-tram-tt — ATW800/2 TRAM probes

Two Transtech TTM15F-45-F TRAMs (Slot 1 s/n 43626004, Slot 2 s/n 8071377) on the ATW800/2. Measured on the real
TT 2026-10-12:

```
T425 link 1 ── link 3 [Slot-1 TRAM] link 1 ── [2nd transputer]   (T425 host link = 0)
```

| file | runs on | does |
| :--- | :--- | :--- |
| `linkprobe.c` | T425 | boot-protocol PEEK on T425 links 1–3: which links have an unbooted transputer |
| `tramprobe.cfs` + `rootp.c` + `tramp.c` | T425 + Slot-1 TRAM | icconf network: boots the Slot-1 TRAM, which PEEKs its own links and reports |
| `tramchar.cfs` + `rootc.c` + `tramc.c` + `charcommon.h` | T425 + Slot-1 TRAM | device id, loop timing, RAM size of Slot-1 and (by POKE/PEEK) the next transputer |

`./build.sh` builds both into `build/`. Traps: a raw `icollect -t` image forwarded by the T425 does not run on
the TRAM (the C runtime needs the configurer's loader); a wrong `Tram.link[N]` hangs `iserver` — recover with
`scripts/tt_reboot.sh --wait-back`, never `kill -9` (that crashed XaAES, exception 4 at pc 0x3AE0).
`tramchar.cfs` + `rootc.c` + `tramc.c` (2026-10-12): Slot-1 RAM 4 MB and the 2nd transputer 4 MB (marker write wraps at 4 MB; the 2nd is sized over boot-protocol POKE/PEEK, unbooted); Slot-1 runs a fixed loop in 30,012 ticks vs 14,743 on the 40 MHz T425, so **~20 MHz** if cycles per iteration match (estimate: memory wait states differ). `lddevid` read 2 on both the T425 and the TRAM, so it does not tell them apart; CPU type (T805 vs T4) still open. Trap: INMOS C signed `+=` is overflow-checked, so an overflowing loop halts the processor.
