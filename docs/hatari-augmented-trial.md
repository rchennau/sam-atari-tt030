# hatari_augmented trial (2026-10-13)

Question: should the TT030 CI use [hatari_augmented](https://github.com/geekychris/hatari_augmented) (upstream
Hatari plus an HTTP/JSON control API) instead of flatpak Hatari driven by a fixed `--run-vbls` count?

Decision (operator): a pinned trial, no fork. Commit `6dc66e5` (2026-10-09), cloned into `tools/hatari-augmented`
(not committed), built in a `debian:stable` container (the build host lacks SDL2 headers), run headless with
`SDL_VIDEODRIVER=dummy`, `--agent-port 7777`, `emulator/hatari.cfg` (TT, TOS 3.06, 4 MB ST + 64 MB TT RAM).

| check | result |
| :--- | :--- |
| `/status` | machine `tt`, TOS 3.06, RAM sizes correct |
| `/screen` | PNG at native resolution, works headless |
| boot to the TOS desktop, `/emu/fastforward` on | stable screen after 35 s |
| `/state/save` at the desktop | 42 KB file |
| cold container start + `/state/load` + `/screen` | **0.87 s**, screen byte-identical to the saved one |
| `/input/click` on the Desk menu | menu opens |

What it changes: GUI checks can start from a snapshot instead of booting (the MiNT config-E boot is 5–9 min with
fast-forward) and can wait on the screen and drive input instead of guessing a VBL count. What it does not change:
Hatari has no ATW800/2, so the transputer work stays on the `t4` emulated lane and the real TT.

Not yet done: porting a real test (the FHS WORDPLUS launch, config E) and measuring its run time and flakiness
against the current VBL-timed run.
