# Unattended TT runs (`ttrun`)

The TT only has SSH under FreeMiNT. Boot it into plain TOS or EmuTOS and it goes dark, so an
experiment that needs those boot sets normally needs someone at the keyboard. `ttrun` closes that
loop: it is autostarted by the desktop, does the work, and always ends by putting the machine back
into a boot set that has SSH.

## How it works

| Piece | Where | Role |
|---|---|---|
| `C:\TTRUN.PRG` | card root | the runner (`src/ttrun.c`) |
| `#Z 01 C:\TTRUN.PRG@` | `C:\NEWDESK.INF` (TOS) and `C:\EMUDESK.INF` (EmuTOS) | desktop autostart |
| `C:\XBOOT\XBOOT.CFG` | card | XBOOT's default boot set, stored as a **plain string** at offset `0x10B` |
| `C:\XBOOT\XBOOT.MIN` | card | a copy of that file with the set set back to `MINT_ATW` |
| `C:\XBOOT\XBOOT.SAV`, `C:\NEWDESK.SAV` | card | untouched originals |
| `C:\TTRUN.LOG` | card | one line per step, so a half-finished run says where it stopped |

Under TOS 3.06 (OS version `0x0306` from `_sysbase`) it runs `VDIBENCH.PRG` and then launches
`EMUTOS.PRG`, which restarts the machine into RAM-loaded EmuTOS. Under EmuTOS it runs the benchmark
again, restores `XBOOT.CFG` and `NEWDESK.INF` from the saved copies, and resets through the ROM's
reset vector with the memory-valid markers cleared, so the restart takes the full power-on path.

## Running one

```bash
ssh atari-tt 'rm -f /c/TTRUN.LOG /c/VDIBENCH.TXT'       # start clean
# set the autostart and the target boot set, then:
ssh atari-tt 'sync; (sleep 2; reboot) &'
until ssh atari-tt 'test -f /c/TTRUN.LOG'; do sleep 20; done
ssh atari-tt 'cat /c/TTRUN.LOG /c/VDIBENCH.TXT'
```

## What it cannot do

- **A cold power cycle.** The reset path clears the memory-valid markers, so the restart does run
  the full memory test, but it is still not a power-on. NFR-1's stopwatch number needs the plug.
- **Choosing at the XBOOT menu.** XBOOT reads the keyboard; unattended runs rely on its timeout and
  on the default set written into `XBOOT.CFG`.

## If a run stops halfway

The machine sits at a desktop with no SSH. Recovery is one power cycle and picking `MINT_ATW` at
the XBOOT menu by hand; `XBOOT.SAV` and `NEWDESK.SAV` are the originals to copy back.
