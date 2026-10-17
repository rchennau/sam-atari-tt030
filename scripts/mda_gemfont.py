#!/usr/bin/env python3
"""mda_gemfont.py TTF OUT.FNT — build an Atari GEM bitmap font (8x16 cell) from the IBM MDA character set.

Source font: Px437_IBM_MDA.ttf from "The Ultimate Oldschool PC Font Pack" by VileR (https://int10h.org/oldschool-pc-fonts/),
licensed CC BY-SA 4.0; the generated .FNT is a derivative under the same licence (credit VileR / int10h.org).
The TTF stores the MDA's 9x14 pixels as square outlines and renders them exactly at size 16 (measured: full block =
9x14, no grey pixels). Column 9 is dropped (the MDA only used it to extend line-drawing glyphs) and one blank row is
added above and below, giving the 8x16 cell xVDI's large system font slot expects (XVDI.INF `16x8=`).
Header layout copied from a working GEM font on the TT (monaco10.fnt): little-endian header, flags 8 (monospaced),
one byte per glyph per row, form 256 bytes x 16 rows, glyphs 0-255 in CP437 order.
"""
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

# CP437 graphic glyphs for the control-code positions (Python's cp437 codec maps these to control characters)
LOW = " ☺☻♥♦♣♠•◘○◙♂♀♪♫☼►◄↕‼¶§▬↨↑↓→←∟↔▲▼"


def cp437(i):
    if i < 32:
        return LOW[i]
    if i == 127:
        return "⌂"
    return bytes([i]).decode("cp437")


def build(ttf):
    font = ImageFont.truetype(ttf, 16)
    rows = [bytearray(256) for _ in range(16)]
    for i in range(1, 256):
        im = Image.new("L", (9, 14), 0)
        ImageDraw.Draw(im).text((0, 0), cp437(i), font=font, fill=255)
        px = im.load()
        for y in range(14):
            rows[y + 1][i] = sum(0x80 >> x for x in range(8) if px[x, y] > 127)
        if 0xB0 <= i <= 0xDF:                # line and block glyphs: extend into the pad rows so they join up
            rows[0][i], rows[15][i] = rows[1][i], rows[14][i]
    off = struct.pack("<257H", *range(0, 257 * 8, 8))
    dat = b"".join(rows)
    hdr = struct.pack("<HH32s15HHIIIHHI", 5437, 10, b"IBM MDA (VileR, CC BY-SA 4.0)",
                      0, 255,                  # first_ade, last_ade
                      13, 11, 8, 2, 2,         # top, ascent, half, descent, bottom (as monaco10, 16-row cell)
                      8, 8, 0, 0,              # max_char_width, max_cell_width, left/right offset
                      1, 1, 0x5555, 0x5555,    # thicken, ul_size, lighten, skew
                      8,                       # flags: monospaced
                      88, 88, 88 + len(off),   # hor_table (none: = off_table), off_table, dat_table
                      256, 16, 0)
    assert len(hdr) == 88
    return hdr + off + dat


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    data = build(sys.argv[1])
    open(sys.argv[2], "wb").write(data)
    print(f"mda_gemfont: {len(data)} bytes -> {sys.argv[2]}")
