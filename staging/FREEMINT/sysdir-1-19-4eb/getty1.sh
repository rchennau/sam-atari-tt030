#!/bin/bash
# Serial console on Modem 2 (SCC) at 57600 baud (fractal: picocom /dev/atari-tt 57600).
# Device /dev/ttyS1 = SCC Modem 2 (scc.xdd, minor 5, paired with /dev/modem2 minor 4).
# Started from mint.cnf. Only one USB-serial cable: move it from Modem 1 to Modem 2 for the
# console, and stop atari-tt-slip.service on fractal first (Modem 1 stays the TOS/STiNG SLIP link).
# Not Modem 1: FreeMiNT mfp.xdd reports the TT's ST-MFP "unsupported", so modem1 has no termios.
# Not mgetty/stty/login: SpareMiNT's (mintlib 0.59) all fail tcgetattr with ENOSYS on MiNT 1.19
# (Hatari, 2026-09-12). ttygetty (src/ttygetty.c, mintlib 0.60) sets the line up and runs the
# snapshot's bash, respawning it on exit. No password: local null-modem cable only.
S=/c/mint/1-19-4eb
$S/ttygetty /dev/ttyS1 57600 $S/bash < /dev/null > /dev/console 2>&1 &
