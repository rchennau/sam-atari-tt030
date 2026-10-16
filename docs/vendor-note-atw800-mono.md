# Note for the ATW800/2 team: 1-bit modes (speed and colour)

Setup: Atari TT030 (68030), ATW800/2, xVDI "68000 Jul 30 2026" build, FreeMiNT + XaAES, 1920×1080 @ 60 Hz.
Measured 2026-10-16 with a small VDI benchmark (medians of 3 runs; source in this repo, `src/vdibench.c`).

## 1. 1-bit drawing is much slower than 8-bit

| Operation | 1920×1080 mono | 1920×1080 256 colours |
| :--- | ---: | ---: |
| 256×256 screen-to-screen `vro_cpyfm` (S_ONLY) per s | 268 | 1212 |
| 100×100 solid `v_bar` per s | 707–775 | 2062 |
| full-height lines per s | 432 | 744 |

Mono moves 8× fewer bytes than 256 colours, yet copies are 4.5× slower. The rates match CPU copies over the bus
in mono and the card's copy engine at 8 bits and up.

As a test, we drove the card's copy engine ourselves for byte-aligned mono copies (a small resident hook). Results in mono, checked bit-exact against the driver's own output:

| Operation | driver | engine-driven |
| :--- | ---: | ---: |
| 256×256 copy per s | 268 | 2222–4444 |
| 400×400 solid bar per s | 137 | 187 |
| 800×800 solid bar per s | 47 | 89 |

**Request:** use the copy engine in 1-bit modes: byte-aligned middle on the engine, partial edge bytes on the CPU;
for solid fills, draw one row and replicate it with the engine.

## 2. 1-bit modes cannot show colours

The Programmer's Manual's control register selects 1 bit/pixel or "8 bit/pixel with LUT". In 1-bit modes
`vs_color` updates the LUT (read back from the card), but the screen stays black and white; setting ctrl_reg bit g
in a 1-bit mode made no difference either.

**Request:** route 1-bit pixels through LUT entries 0 and 1 (or two dedicated registers), so a mono mode can show
any two colours, e.g. amber on black like an MDA monitor.

Thank you for the card and the drivers.
