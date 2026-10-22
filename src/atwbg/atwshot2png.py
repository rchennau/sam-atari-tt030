#!/usr/bin/env python3
"""atwshot2png.py FILE.raw OUT.png — convert an atwshot dump to PNG.
Headerless = the old 1024x768 RGB565 little-endian dump; "ATWSHOT W H PLANES\n" = the mode-aware one (8 planes: 512-byte
big-endian RGB565 LUT + index bytes; 16 planes: RGB565 little-endian)."""
import struct
import sys
import zlib

W, H = 1024, 768


def rgb(p):
    return bytes(((p >> 11 & 31) * 255 // 31, (p >> 5 & 63) * 255 // 63, (p & 31) * 255 // 31))


def png(path, rows):
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    raw = b"".join(b"\0" + r for r in rows)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def main(src, out):
    global W, H
    data = open(src, "rb").read()
    planes = 16
    if data.startswith(b"ATWSHOT "):
        head, data = data.split(b"\n", 1)
        W, H, planes = (int(v) for v in head.split()[1:])
    if planes == 1:                      # bit 1 = VDI colour 1 = white since the 2026-10-20 white-on-black look
        assert len(data) == W // 8 * H, f"expected {W // 8 * H} bytes, got {len(data)}"
        on, off = b"\xff\xff\xff", b"\x00\x00\x00"
        rows = [b"".join(on if data[y * (W // 8) + x // 8] & (0x80 >> (x & 7)) else off for x in range(W)) for y in range(H)]
    elif planes == 8:
        lut = [rgb(p) for p in struct.unpack(">256H", data[:512])]
        px = data[512:]
        assert len(px) == W * H, f"expected {W * H} pixel bytes, got {len(px)}"
        rows = [b"".join(lut[i] for i in px[y * W:(y + 1) * W]) for y in range(H)]
    else:
        assert len(data) == W * H * 2, f"expected {W * H * 2} bytes, got {len(data)}"
        px = struct.unpack(f"<{W * H}H", data)
        rows = [b"".join(rgb(p) for p in px[y * W:(y + 1) * W]) for y in range(H)]
    png(out, rows)


if __name__ == "__main__":
    main(*sys.argv[1:3])
