#!/usr/bin/env python3
"""atw_palette.py LOOK OUT — build an xVDI DEFAULT.PAL (256 VDI pens x R,G,B big-endian words, 0..1000)
from docs/palettes/normal.pal (the stock 256-colour palette dumped with `atwpal -d`, md5 6ce4f235).
  normal  the stock palette, unchanged
  amber[:K]  amber MDA look, inverted: L = 1000 - (.299R + .587G + .114B), pen -> (L, K*L, 0), so white
          backgrounds turn black and black text turns amber; pen 3 (desktop) forced black. K = green/red:
          0.85 was the first look (2026-10-16, read as too yellow); 0.69 is P3 amber phosphor (~255,176,0)
  amber:R,G,B  same, but the brightest pen is the 0-255 colour R,G,B and every pen scales it: pen -> L * (R,G,B).
          217,137,65 = measured from the operator's reference photo of an amber monitor (2026-10-17, unclipped
          text strokes; the clipped highlights drift yellow-white and were excluded)
Pen order is VDI order: the driver's boot loader calls vs_color(i) for i = 0..255 (display-tuning Tech Spec).
"""
import struct
import sys
from pathlib import Path

NORMAL = Path(__file__).resolve().parent.parent / "docs/palettes/normal.pal"


def build(look):
    v = list(struct.unpack(">768h", NORMAL.read_bytes()))
    if look == "normal":
        return v
    name, _, k = look.partition(":")
    if name != "amber":
        raise SystemExit(f"atw_palette: unknown look {look!r} (normal|amber[:K])")
    k = k or "217,137,65"   # operator-approved 2026-10-17 (measured amber); "amber:0.85" reproduces the 10-16 look (ec78ef25)
    if "," in k:
        tr, tg, tb = (int(c) * 1000 / 255 for c in k.split(","))
        mix = lambda lum: [round(lum * tr / 1000), round(lum * tg / 1000), round(lum * tb / 1000)]
    else:
        kk = float(k) if k else 0.85
        mix = lambda lum: [lum, round(kk * lum), 0]
    out = []
    for i in range(256):
        r, g, b = v[3 * i:3 * i + 3]
        lum = 1000 - round(0.299 * r + 0.587 * g + 0.114 * b)
        out += mix(lum)
    out[9:12] = [0, 0, 0]                    # pen 3: desktop background
    return out


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    data = struct.pack(">768h", *build(sys.argv[1]))
    assert len(data) == 1536
    Path(sys.argv[2]).write_bytes(data)
