# Spike: HighWire JPEG decoding on the transputers (2026-10-17)

Question (operator): validate offloading HighWire's image work to the ATW800/2's transputers, as an architecture
spike, against the proxy-transcoding route.

## Measured on the TT

**HighWire's own image timings** (`LOGGING`, local page, mode 32, 8 bpp; see `docs/transputer-evaluation.md`):

| JPEG | Dithered | No dither (`FAST_IMAGES`) | Via the transcoding proxy (palette PNG) |
| :--- | ---: | ---: | ---: |
| 320×240 | 7.9 s | 9.9 s | 2.1 s |
| 640×480 | 27.5 s | 20.8 s | 5.8 s |
| 1024×768 | 68.3 s | 53.1 s | 11.2 s |

**`djpeg` (libjpeg-t425-on) on the same files**, decode plus writing the PPM; output byte-identical on both paths:

| JPEG | 68030 (ttjobd running, so the T425 is busy) | T425 direct (ttjobd stopped) | T425 ÷ 68030 |
| :--- | ---: | ---: | ---: |
| 320×240 | 5.5 s | 4.0 s | 0.73 |
| 640×480 | 34.5 s* | 12.3 s | (0.36*) |
| 1024×768 | 51.3 s | 28.0 s | **0.55** |

\*The 68030 run overlapped an SSH poll by the spike operator (about 25 s of login crypto), so it is inflated. Use the
1024×768 ratio. Small images pay a fixed cost to load the T425 image.

The direct path **does not work in production**. With ttjobd running (the normal state since 2026-10-13), djpeg
logs `T425 busy (pid 13); using the 68030` and falls back. Stopping and restarting ttjobd for the spike was clean:
it booted 3 of 3 nodes again.

## Architecture options

| | A. Direct T425 call from HighWire | B. New ttjobd op `J` | C. Transcoding proxy (CT114) |
| :--- | :--- | :--- | :--- |
| Works with ttjobd | no: lock held, always falls back | yes | n/a |
| 640×480 page image, estimated | — | ~11 s: ~10 s decode, plus 0.7 s to return 8 bpp if the T425 also dithers | **9.2 s** measured: 5.8 s decode + ~3.4 s for the larger file |
| 1024×768, estimated | — | ~22 s: return 786 KB at 8 bpp, which is over JOB_MAX (400 KB), so bands | **~20 s**: 11.2 s decode + ~9 s transfer |
| Several images on a page | — | T425 + 2 TRAMs + 68030 in parallel (TRAM ≈ 2–2.8× slower each) | proxy work is ~0.1–0.2 s per image |
| Screen in mode 32 | — | **blanks during every decode**; TRAM jobs relay through the T425, unmeasured | no effect |
| New code | — | T425 jpegserv as a ttjobd op, dither on the T425, band protocol, HighWire hook in `img_jpg.c` | done (merged; CT114 deploy) |
| Without the proxy (offline LAN, local files) | — | helps | does not help |

## Verdict

1. **The proxy wins for web pages.** It is about as fast as the best T425 design, does not blank the screen, and
   needs no TT code. HighWire was written for it (`UA-pixels`).
2. **T425 JPEG offload is technically sound** (identical output, ~0.55× the 68030 time). It is worth building only
   for images that never pass the proxy (local files, `file://`), and only once one of these holds:
   - the default display mode moves to ≤ 102.6 MHz, so jobs stop blanking (mode 39, 1664×936, was the boot mode
     2026-10-17 to 2026-10-20; since then it is mode 37 at 137.7 MHz); or
   - TRAM-only jobs are shown not to blank, so images could run on the TRAMs.
3. **Prerequisites, if built:**
   - a ttjobd `J` op with band output (JOB_MAX);
   - dithering on the T425, so only one byte per pixel returns;
   - a HighWire hook in `img_jpg.c` with a fallback to the 68030;
   - a measured check that TRAM jobs do or do not blank (operator at the screen).

## Update 2026-10-17 16:40: blanking not reproduced (operator watching)

In mode 32, none of these blanked:
- X25519 jobs pinned to TRAM 1, TRAM 2 and the T425;
- 300 KB SHA-256 jobs on each node;
- an SSH login with T425 offload.

So the blanking that drove FR-6, NFR-5 and this spike's verdict did not show up today, for compute or for 300 KB
bulk data. The cause of the earlier blanks (tgzip and logins, 2026-10-16) is unknown. Dropbear offload is back on
in mode 32.

**Revised verdict:**
- The "blanks in mode 32" blocker for option B (ttjobd op `J`) is lifted, pending a watched tgzip re-test.
- B becomes the path for images the proxy never sees (local files).
- B can spread a page's images over the T425 and both TRAMs: measured 15–16 s per 300 KB on a TRAM against 4 s
  on the T425, for SHA-256.
- For web pages the proxy (C) is still at least as fast, and needs no TT code.
