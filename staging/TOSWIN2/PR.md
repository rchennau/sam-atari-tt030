## Back off the output poll when TosWin2 is idle

TosWin2 polls its pty handles from `MU_TIMER` every 50 ms, because the AES cannot wait on file handles (`XMU_FSELECT`
is `#if 0` in XaAES). On a 32 MHz TT030 that fixed poll costs about 6.5 % of the CPU on an idle machine.

This change keeps the 50 ms poll while anything is happening. After 40 polls (about 2 s) with no output, key,
click or AES message, it polls every 250 ms. Any event returns it to 50 ms at once. `fd_input()` now returns whether
any handle had data.

### Measured (TT030, FreeMiNT 1.19, XaAES, TosWin2 2.7)

| build | idle CPU of `toswin2` |
| :--- | :--- |
| 2.7, 50 ms fixed | 3.88 s / 60 s = **6.5 %** |
| 2.7 + this change | 1.15 s / 60 s and 3.35 s / 180 s = **1.9 %** |

### Behaviour

- Unchanged: keys, clicks and messages wake TosWin2 immediately, so typing and its echo behave as today, and busy
  output is read every 50 ms as before.
- Changed: the first output after 2 s or more of silence can appear up to 250 ms later. While idle, the cursor
  blinks every 250 ms instead of 300 ms.

### Notes

- No new calls or dependencies; 27 lines added and 6 removed in `event.c`, `proc.c` and `proc.h`.
- Built with m68k-atari-mint-gcc and run on a real TT030: it kept running and catching console output (on-screen
  drawing not checked remotely), and idle CPU was
  measured as above.
- Not measured on other CPUs. On a 68000 the saving should be larger, since the per-poll work is a bigger share of a
  slower CPU.
- A fuller analysis, with the alternatives considered (a longer fixed timer, a blocking helper thread, finishing
  `XMU_FSELECT` in XaAES), is available if useful.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01NM9eMxo44K4dNcq7bfykaX
