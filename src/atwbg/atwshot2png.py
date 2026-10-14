#!/usr/bin/env python3
"""atwshot2png.py FILE.raw OUT.png — convert an atwshot dump (1024x768 RGB565, little-endian) to PNG."""
import struct
import sys
import zlib

W, H = 1024, 768


def png(path, rows):
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    raw = b"".join(b"\0" + r for r in rows)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def main(src, out):
    data = open(src, "rb").read()
    assert len(data) == W * H * 2, f"expected {W * H * 2} bytes, got {len(data)}"
    px = struct.unpack(f"<{W * H}H", data)
    rows = []
    for y in range(H):
        row = bytearray()
        for p in px[y * W:(y + 1) * W]:
            row += bytes(((p >> 11 & 31) * 255 // 31, (p >> 5 & 63) * 255 // 63, (p & 31) * 255 // 31))
        rows.append(bytes(row))
    png(out, rows)


if __name__ == "__main__":
    main(*sys.argv[1:3])
