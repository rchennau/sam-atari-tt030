# TosWin2 idle poll back-off

`idle-backoff.patch` against [freemint/toswin2](https://github.com/freemint/toswin2) `af2e9e2` (TosWin2 2.7). It
changes three files (`event.c`, `proc.c`, `proc.h`): 27 lines added, 6 removed. Upstream PR text: [`PR.md`](PR.md). It is
packaged for the TT as `toswin2-2.7-1.sam1` by `scripts/build_toswin2_rpm.sh` (pinned sources, reproducible).

## The problem

TosWin2 is a GEM terminal and the XaAES console catcher. Its event loop (`event.c`, `event_loop`) calls
`evnt_multi` with `MU_TIMER` at a fixed **50 ms** whenever a window is open or console output is caught. On every
tick, `fd_input` (`proc.c`) calls `Fselect` with a 1 ms timeout over all its pty handles and refreshes any window
with pending text. So TosWin2 wakes 20 times a second even when nothing is happening.

It has to poll because the AES cannot wait on file handles. XaAES has the beginnings of one, `XMU_FSELECT` for
`xevnt_multi`, but that code is `#if 0` in `xaaes/src.km/k_main.c`. The XaAES notes (`newcalls.txt`) also call
`xevnt_multi` unofficial and unimplemented. No AES event fires when a pty has data.

## The change

- `fd_input` now returns whether any handle had data (`Fselect` > 0).
- `event_loop` counts consecutive polls with no output, key, mouse button or AES message.
  - After 40 such polls (2 s), it polls every **250 ms** instead of 50 ms.
  - Any event (output, key, click, message) returns it to 50 ms at once.
- While idle, the cursor blinks on every poll (250 ms). While busy it keeps its old rhythm: every 6th poll, 300 ms.

Nothing else changes: no new API, no new configuration, no change to how text is read or drawn.

## Measurements (Atari TT030, 32 MHz 68030, FreeMiNT 1.19, XaAES, TosWin2 2.7)

CPU time of the `toswin2` process, from `ps` before and after a quiet window on an otherwise idle machine:

| build | window | CPU used | share of the 68030 |
| :--- | :--- | :--- | :--- |
| 2.7 as shipped (`b2587cb`, 50 ms fixed) | 60 s | 3.88 s | **6.5 %** |
| 2.7 as shipped, earlier reading (2026-10-12) | — | — | ~6 % |
| 2.7 + this patch (`af2e9e2` + patch) | 60 s | 1.15 s | **1.9 %** |
| 2.7 + this patch | 180 s | 3.35 s | **1.9 %** |

Idle cost falls by about 3.4×, returning ~4.6 % of the 68030 to other work. It is not the full 5× the timer ratio
suggests: each wake-up still pays the AES `evnt_multi` round-trip and the 1 ms `Fselect`.

**Unchanged.** We paused TosWin2 (`SIGSTOP`) and compared. A local-socket round-trip between two other processes
measured 6.1–6.5 ms with TosWin2 running and with it paused. So TosWin2 does not delay other processes' wake-ups; it
only consumes CPU.

**Checked on screen** (TT framebuffer grabs, `src/atwbg/atwshot.c`, 2026-10-14):
- keys typed into a TosWin2 shell window echo and run normally (`echo typed-ok` → `typed-ok`);
- `echo first-line; sleep 5; echo after-5s-idle` draws both lines, the second after the back-off had engaged;
- console output still opens and fills the Console window;
- idle with a shell window open: 1.2 s / 60 s = 2.0 %.

**What was not measured:**
- The latency of the first output after an idle spell. It follows from the code (at most 250 ms, on average about
  125 ms), not from a measurement.
- Other CPUs (68000, 68040/060, ColdFire) and emulators.

Two samples per build: enough to see a 3× change, not enough to quote a variance.

## Pros and cons

| | Pro | Con |
| :--- | :--- | :--- |
| CPU | Idle cost 6.5 % → 1.9 % on a TT030. On a 68000 ST the saving matters more, since the same per-poll work is a larger share of a slower CPU (not measured). | — |
| Interactivity | Keys, clicks and AES messages still wake TosWin2 immediately and switch it back to 50 ms, so typing and its echo are unaffected. | The first output after ≥ 2 s of silence (e.g. a background job printing) can appear up to 250 ms later, and its first chunk is read at the slower rate. |
| Cursor | — | While idle the cursor blinks every 250 ms instead of 300 ms; slightly faster, barely visible. |
| Risk | One counter and a returned flag. No new AES, MiNT or library calls. The binary needs nothing new. | The 2 s and 250 ms values are fixed constants, not settings. Neither has been tuned beyond "clearly helps, clearly not annoying". |
| Fast machines | On a 68060, a ColdFire or an emulator the absolute saving is small, but never negative. | — |

## Alternatives considered

1. **A longer fixed timer** (e.g. 250 ms always). Smaller patch, but every burst of output would be read and drawn
   5× less often, so scrolling output looks jerky. Rejected: it penalises the busy case to fix the idle one.
2. **A helper thread or process that blocks in `Fselect` and wakes TosWin2 with `appl_write`.** No polling at all,
   the ideal. But the helper must follow every change to the handle mask (windows opening and closing), share the
   handles across a process boundary, and send AES messages from a non-GEM context. That is far more code and risk
   than this change.
3. **AES support for waiting on handles** (finish `XMU_FSELECT` in XaAES). The right long-term fix, and it would
   benefit every terminal and network GEM app. But it is an AES project, and TosWin2 would still need its timer for
   other AESes (MagiC, N.AES, TOS).
4. **Make the values configurable** in `toswin2.cfg`. Possible later. Not added because the defaults work, and
   options nobody has asked for are maintenance.

## Why this is better overall

The idle state is where a terminal spends most of its life. This patch cuts its cost about 3.4× on a 32 MHz
machine. The price is at most 250 ms of extra delay for the first output after two seconds of silence. Typing and
any burst of output keep today's 50 ms behaviour. It adds no new interfaces and is easy to review.

Option 3 would be better still, and it needs AES work. Until it exists, the back-off gets most of the gain for
almost none of the risk.
