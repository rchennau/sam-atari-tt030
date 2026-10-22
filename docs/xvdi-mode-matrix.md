# xVDI mode matrix (ATW800/2 on the TT030)

Every mode in the card's live `C:\AUTO\XVDI.BIB` (md5 d238c3ac, 39 records: 36 vendor + custom 37–39), with its
programmed timing and what was measured on the TT. Snapshot 2026-10-21; boot mode **32** (1920×1080, 256 colours, amber palette).

- **Timing columns are decoded, not measured:** pixel clock = 27 MHz × PLL feedback ÷ input divider ÷ 5; H kHz and
  V Hz follow from the totals. "(label)" is the refresh the record claims; modes 31 and 35 claim 50/60 Hz but are
  programmed for 40/30 Hz.
- **2 MB** = the mode's VRAM fits the card's 2 MB window. Modes marked NO need more memory than the window shows.
- **Monitor** = ASUS VS247H-P (accepts 30–80 kHz H, 55–75 Hz V, at most 1080 lines). "untested" = never booted.
- **T425 offload** = picture during T425 jobs, watched by the operator. Blanking tracks pixel clock: stable at
  ≤ 102.6 MHz, blanks at ≥ 108 MHz for bulk-data jobs. Details, benchmarks and PLL limits:
  [`resolution-management.md`](resolution-management.md).
- Boot mode: `scripts/atw_mode.sh show | set <mode>` then reboot (XVDIMENU applies a mode at boot only).

## Regenerate

```
ssh atari-tt 'cat /c/auto/xvdi.bib' > staging/XVDI_CARD/XVDI.BIB; ssh atari-tt 'cat /c/auto/xvdimenu.inf' > staging/XVDI_CARD/XVDIMENU.INF
python3 scripts/atw_modes.py --md staging/XVDI_CARD/XVDI.BIB staging/XVDI_CARD/XVDIMENU.INF docs/atw-mode-status.tsv
```

Measured results live in `docs/atw-mode-status.tsv`; edit that, not the table below.

## Matrix

| Mode | Name | Visible | Colours | VRAM KB | 2 MB | Pixel MHz | H kHz | V Hz (label) | Total H×V | H fp/sync/bp | V fp/sync/bp | Sync | Monitor | T425 offload | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 640 x 400 2 72Hz | 640×400 | 2 | 31 | yes | 25.9 | 32.4 | 72.2 (72) | 800×449 | 16/96/48 | 12/2/35 | -H -V | untested | — |  |
| 2 | 640 x 400 256 72Hz | 640×400 | 256 | 250 | yes | 25.9 | 32.4 | 72.2 (72) | 800×449 | 16/96/48 | 12/2/35 | -H +V | untested | — |  |
| 3 | 640 x 400 65536 72Hz | 640×400 | 65536 | 500 | yes | 25.9 | 32.4 | 72.2 (72) | 800×449 | 16/96/48 | 12/2/35 | -H -V | untested | — |  |
| 4 | 640 x 400 32 bit 72Hz | 640×400 | 4294967296 | 1000 | yes | 25.9 | 32.4 | 72.2 (72) | 800×449 | 16/96/48 | 12/2/35 | -H -V | untested | — |  |
| 5 | 640 x 480 2 60Hz | 640×480 | 2 | 38 | yes | 25.0 | 31.2 | 59.5 (60) | 800×525 | 16/96/48 | 10/2/33 | -H -V | untested | — |  |
| 6 | 640 x 480 256 60Hz | 640×480 | 256 | 300 | yes | 25.0 | 31.2 | 59.5 (60) | 800×525 | 16/96/48 | 10/2/33 | -H -V | untested | — |  |
| 7 | 640 x 480 65536 60Hz | 640×480 | 65536 | 600 | yes | 25.0 | 31.2 | 59.5 (60) | 800×525 | 16/96/48 | 10/2/33 | -H -V | untested | — |  |
| 8 | 640 x 480 32 bit 60Hz | 640×480 | 4294967296 | 1200 | yes | 25.0 | 31.2 | 59.5 (60) | 800×525 | 16/96/48 | 10/2/33 | -H -V | untested | — |  |
| 9 | 800 x 600 2 60Hz | 800×600 | 2 | 59 | yes | 40.0 | 37.8 | 60.3 (60) | 1056×628 | 40/128/88 | 1/4/23 | +H +V | untested | — |  |
| 10 | 800 x 600 256 60Hz | 800×600 | 256 | 469 | yes | 40.0 | 37.8 | 60.3 (60) | 1056×628 | 40/128/88 | 1/4/23 | +H +V | shown | — |  |
| 11 | 800 x 600 65536 60Hz | 800×600 | 65536 | 938 | yes | 40.0 | 37.8 | 60.3 (60) | 1056×628 | 40/128/88 | 1/4/23 | +H +V | shown | — |  |
| 12 | 800 x 600 32 bit 60Hz | 800×600 | 4294967296 | 1875 | yes | 40.0 | 37.8 | 60.3 (60) | 1056×628 | 40/128/88 | 1/4/23 | -H -V | untested | — |  |
| 13 | 1024 x 768 2 60Hz | 1024×768 | 2 | 96 | yes | 64.8 | 48.2 | 59.8 (60) | 1344×806 | 24/136/160 | 3/6/29 | -H -V | untested | — |  |
| 14 | 1024 x 768 256 60Hz | 1024×768 | 256 | 768 | yes | 64.8 | 48.2 | 59.8 (60) | 1344×806 | 24/136/160 | 3/6/29 | -H -V | shown | — |  |
| 15 | 1024 x 768 65536 60Hz | 1024×768 | 65536 | 1536 | yes | 64.8 | 48.2 | 59.8 (60) | 1344×806 | 24/136/160 | 3/6/29 | -H -V | shown | stable | full offload, no blanking |
| 16 | 1024 x 768 32 bit 60Hz | 1024×768 | 4294967296 | 3072 | NO | 64.8 | 48.2 | 59.8 (60) | 1344×806 | 24/136/160 | 3/6/29 | -H -V | untested | — |  |
| 17 | 1280 x 720 256 60Hz (Split) | 640×720 | 256 | 900 | yes | 75.6 | 45.8 | 61.1 (60) | 1650×750 | 110/40/220 | 5/5/20 | +H +V | untested | — |  |
| 18 | 1280 x 720 256 60Hz | 1280×720 | 256 | 900 | yes | 75.6 | 45.8 | 61.1 (60) | 1650×750 | 110/40/220 | 5/5/20 | +H +V | shown | — |  |
| 19 | 1280 x 800 256 60Hz (Split) | 640×800 | 256 | 1000 | yes | 82.8 | 49.3 | 59.5 (60) | 1680×828 | 64/136/200 | 1/3/24 | +H +V | untested | — |  |
| 20 | 1280 x 800 2 60Hz | 1280×800 | 2 | 125 | yes | 82.8 | 49.3 | 59.5 (60) | 1680×828 | 64/136/200 | 1/3/24 | +H +V | untested | — |  |
| 21 | 1280 x 800 256 60Hz | 1280×800 | 256 | 1000 | yes | 82.8 | 49.3 | 59.5 (60) | 1680×828 | 64/136/200 | 1/3/24 | +H +V | shown | — |  |
| 22 | 1280 x 800 65536 60Hz | 1280×800 | 65536 | 2000 | yes | 82.8 | 49.3 | 59.5 (60) | 1680×828 | 64/136/200 | 1/3/24 | +H +V | shown | stable | full offload, 65536 colours |
| 23 | 1280 x 960 2 60Hz | 1280×960 | 2 | 150 | yes | 102.6 | 59.9 | 60.3 (60) | 1712×994 | 80/136/216 | 1/3/30 | -H +V | untested | — |  |
| 24 | 1280 x 960 256 60Hz | 1280×960 | 256 | 1200 | yes | 102.6 | 59.9 | 60.3 (60) | 1712×994 | 80/136/216 | 1/3/30 | -H +V | shown | stable | FR-6 2026-10-17: 2 runs, 0 blanks |
| 25 | 1280 x 1024 2 60Hz | 1280×1024 | 2 | 160 | yes | 108.0 | 64.0 | 60.0 (60) | 1688×1066 | 48/112/248 | 1/3/38 | +H +V | untested | — |  |
| 26 | 1280 x 1024 256 60Hz | 1280×1024 | 256 | 1280 | yes | 108.0 | 64.0 | 60.0 (60) | 1688×1066 | 48/112/248 | 1/3/38 | +H +V | shown | blanks | 108 MHz: blanks during T425 jobs |
| 27 | 1440 x 960 256 60Hz (Split) | 640×960 | 256 | 1350 | yes | 102.6 | 53.9 | 54.2 (60) | 1904×994 | 80/152/232 | 1/3/30 | -H +V | untested | — |  |
| 28 | 1440 x 960 256 60Hz | 1440×960 | 256 | 1350 | yes | 102.6 | 53.9 | 54.2 (60) | 1904×994 | 80/152/232 | 1/3/30 | -H +V | shown | stable | FR-6 2026-10-17; 54.2 Hz, under the monitor's stated 55 Hz, still shown |
| 29 | 1600 x 1200 2 50Hz | 1600×1200 | 2 | 234 | yes | 135.0 | 62.5 | 50.0 (50) | 2160×1250 | 64/192/304 | 1/3/46 | +H +V | untested | — |  |
| 30 | 1600 x 1200 256 50Hz | 1600×1200 | 256 | 1875 | yes | 135.0 | 62.5 | 50.0 (50) | 2160×1250 | 64/192/304 | 1/3/46 | +H +V | OUT OF RANGE | — | 50 Hz |
| 31 | 1600 x 1200 65536 50Hz | 1600×1200 | 65536 | 3750 | NO | 108.0 | 50.0 | 40.0 (40) | 2160×1250 | 64/192/304 | 1/3/46 | +H +V | untested | — | label says 50 Hz; record is programmed for 40 Hz |
| 32 **(boot)** | 1920 x 1080 256 60Hz | 1920×1080 | 256 | 2025 | yes | 137.7 | 66.2 | 59.6 (60) | 2080×1111 | 48/32/80 | 3/5/23 | -H +V | shown | compute OK, bulk blanks | Boot mode since 2026-10-21 21:3x PDT (operator: amber at 1920×1080; DEFAULT.PAL amber). 2026-10-17: logins and X25519 jobs do not blank; tgzip-style bulk jobs did. PLL VCO 1377 MHz, over the GW2AR-18 spec |
| 33 | 1920 x 1080 65536 50Hz | 1920×1080 | 65536 | 4050 | NO | 116.1 | 55.8 | 50.2 (50) | 2080×1111 | 48/32/80 | 3/5/23 | -H +V | untested | — |  |
| 34 | 1920 x 1200 2 40Hz | 1920×1200 | 2 | 281 | yes | 129.6 | 50.0 | 40.3 (40) | 2592×1242 | 128/208/336 | 1/3/38 | -H +V | OUT OF RANGE | — | 40 Hz |
| 35 | 1920 x 1200 256 60Hz | 1920×1200 | 256 | 2250 | NO | 97.2 | 37.5 | 30.2 (30) | 2592×1242 | 128/208/336 | 1/3/38 | -H +V | untested | — | label says 60 Hz; record is programmed for 30 Hz |
| 36 | 1920 x 1200 256 50Hz | 1920×1200 | 256 | 2250 | NO | 126.9 | 61.0 | 49.6 (50) | 2080×1231 | 48/32/80 | 3/5/23 | -H +V | untested | — |  |
| 37 | 1920 x 1080 2 60Hz | 1920×1080 | 2 | 253 | yes | 137.7 | 66.2 | 59.6 (60) | 2080×1111 | 48/32/80 | 3/5/23 | -H +V | shown | not watched | custom (mode 32 timing, 1 bit). Boot mode 2026-10-20 to 10-21; black background, white text (/etc/atw-mono-colours.sh); atwmono copy-engine hook: blits 268 → 4444/s |
| 38 | 1600 x 1200 256 60Hz | 1600×1200 | 256 | 1875 | yes | 129.6 | 73.6 | 59.8 (60) | 1760×1231 | 48/32/80 | 3/5/23 | +H +V | OUT OF RANGE | — | custom 1600×1200 reduced blanking; the monitor rejects > 1080 lines |
| 39 | 1664 x 936 256 58Hz | 1664×936 | 256 | 1521 | yes | 102.6 | 56.2 | 58.2 (58) | 1824×967 | 48/32/80 | 3/5/23 | -H +V | shown | not watched | custom 1664×936×8 at 102.6 MHz (in-spec VCO on C8); boot mode 2026-10-17 to 2026-10-20 and briefly 2026-10-21 |
