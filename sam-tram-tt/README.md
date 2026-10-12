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

`./build.sh` builds both into `build/`. Traps: a raw `icollect -t` image forwarded by the T425 does not run on
the TRAM (the C runtime needs the configurer's loader); a wrong `Tram.link[N]` hangs `iserver` — recover with
`scripts/tt_reboot.sh --wait-back`, never `kill -9` (that crashed XaAES, exception 4 at pc 0x3AE0).
Not yet measured: CPU type, clock, RAM per TRAM.
