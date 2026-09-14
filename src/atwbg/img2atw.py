#!/usr/bin/env python3
"""Convert an image to the ATW800/2's 1024x768 16-bit framebuffer layout for atwbg.

Layout measured on the real card (2026-09-13): standard RGB565 stored LITTLE-endian (Intel byte order).
A first reading from four solid test bars (blue/red/green/white) suggested a BRG layout; a gradient chart
showed each ramp wrapping into its neighbour's colour, which is exactly RGB565 with the bytes swapped.

Usage: img2atw.py [--fit] IN.(jpg|png|webp|...) OUT.raw  |  img2atw.py --chart OUT.raw
  default: fill the screen and crop the overflow; --fit: show all of it, padded with its corner colour
"""
import sys
from PIL import Image

W, H = 1024, 768


def pack(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def chart():
    im = Image.new("RGB", (W, H))
    px = im.load()
    bands = [(1, 0, 0), (0, 1, 0), (0, 0, 1), (1, 1, 1)]
    for y in range(H):
        cr, cg, cb = bands[min(y * 4 // H, 3)]
        for x in range(W):
            v = x * 255 // (W - 1)
            px[x, y] = (v * cr, v * cg, v * cb)
    return im


def main():
    args = sys.argv[1:]
    fit = args[:1] == ["--fit"]
    args = args[1:] if fit else args
    if len(args) != 2:
        sys.exit(__doc__)
    sys.argv[1:] = args
    im = chart() if sys.argv[1] == "--chart" else Image.open(sys.argv[1]).convert("RGB")
    if fit and im.size != (W, H):  # whole picture visible, centred, padded with its corner colour
        s = min(W / im.width, H / im.height)
        im = im.resize((round(im.width * s), round(im.height * s)), Image.LANCZOS)
        bg = Image.new("RGB", (W, H), im.getpixel((0, 0)))
        bg.paste(im, ((W - im.width) // 2, (H - im.height) // 2))
        im = bg
    if im.size != (W, H):  # cover the screen, keep the aspect ratio, crop the overflow
        s = max(W / im.width, H / im.height)
        im = im.resize((round(im.width * s), round(im.height * s)), Image.LANCZOS)
        l, t = (im.width - W) // 2, (im.height - H) // 2
        im = im.crop((l, t, l + W, t + H))
    out = bytearray()
    for r, g, b in im.getdata():
        out += pack(r, g, b).to_bytes(2, "little")
    open(sys.argv[2], "wb").write(out)


if __name__ == "__main__":
    main()
