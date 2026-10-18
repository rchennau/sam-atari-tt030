# Note for the xVDI author: vrt_cpyfm bus error with a negative destination y

Setup: Atari TT030 (68030), ATW800/2, xVDI "68000 Jul 30 2026" build (`XVDI.PRG` md5 304d7909d7ff35d7e1387973a318126b),
FreeMiNT 1.19 + XaAES, 1664×936 in 256 colours. Found 2026-10-18 when TurboCAD 2 (`TBCAD2TT.PRG`) crashed at startup.

## What happens

`vrt_cpyfm` from a 1-plane memory form to the screen (destination MFDB address 0) with **clipping off** and a
destination rectangle starting at **y = -2** writes far past the framebuffer, and the program dies with a bus error:

    BUS ERROR: User PC=10291A0, Address 53FF97F

The PC is the pixel store `move.b d7,(a2)+` at text offset +0x2a94 of the 8-plane `vrt_cpyfm` routine (+0x293e).
The address is the screen base 0xFEC00000 + 65534 × 1664 + 1663, modulo 2^32. The routine computes the destination
row with `mulu` (offset +0x29de), so y = -2 becomes 65534. The routine clips (offset +0x2988) only when clipping is on.
With clipping on, the same call returns normally.

## Reproduce

`src/atwbg/vrtneg.c` in this repo draws a 16×16 1-plane pattern with `vrt_cpyfm`, MD_REPLACE, destination x = 100:

| call | result |
| :--- | :--- |
| `vrtneg pos`: clipping off, y = 10 | returns |
| `vrtneg clip`: clipping on, y = -2 | returns |
| `vrtneg neg`: clipping off, y = -2 | bus error (SIGBUS, rc 138) |

## Suggested fix

Clip a screen destination to the screen even when the caller's clipping is off. Failing that,
reject or skip rows with a negative destination y. The same routine also writes 8-bit pixels into a memory
destination without checking that destination's plane count.

## Local workaround

`atwmono` (this repo, `src/atwbg/atwmono.c`) hooks the live op table's `vrt_cpyfm` slot. For screen
destinations with clipping off and a corner off the screen, it forces the driver's own clip to the screen for that
call.
