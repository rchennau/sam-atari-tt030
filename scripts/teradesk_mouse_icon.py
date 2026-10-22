#!/usr/bin/env python3
"""teradesk_mouse_icon.py — redraw TeraDesk's unused UPLOAD icon as a mouse named MOUSE (tt030-remote-keyboard, the
TT's TTKBD.PRG desktop icon; operator 2026-10-21).

  teradesk_mouse_icon.py ICONS.RSC CICONS.RSC OUTDIR     -> OUTDIR/ICONS.RSC, OUTDIR/CICONS.RSC

Same slot, same sizes, so no offset in either resource moves: the 32x32 mono image and mask, the 12-byte name, and
in CICONS.RSC every colour plane (all planes = the mono image, so the outline is colour 15 / black and the body
colour 0 / white) and the selected image. ponytail: replaces UPLOAD rather than appending a 39th icon, which would
mean rebuilding both resources' offset tables; nothing on the TT uses UPLOAD. Fails loudly if UPLOAD is absent.
"""
import struct
import sys

ART = """
................#...............
................#...............
...............#................
...............#................
................#...............
................#...............
...........##########...........
.........###....#....###........
........##......#.......##......
.......#........#........#......
.......#........#........#......
......#.........#.........#.....
......#.........#.........#.....
......#.........#.........#.....
......#.........#.........#.....
......####################......
......#...................#.....
......#...................#.....
......#...................#.....
......#...#####...#####...#.....
......#.....#.......#.....#.....
......#.....#.......#.....#.....
......#.....#.......#.....#.....
......#.....#.......#.....#.....
......#...................#.....
.......#.................#......
.......#.................#......
........##.............##.......
.........###.........###........
...........###########..........
................................
................................
""".split()


def bitmaps():
    assert len(ART) == 32 and all(len(r) == 32 for r in ART)
    data = b"".join(int(r.replace("#", "1").replace(".", "0"), 2).to_bytes(4, "big") for r in ART)
    mask = []                                  # filled: everything between the leftmost and rightmost ink per row
    for r in ART:
        i, j = r.find("#"), r.rfind("#")
        mask.append(0 if i < 0 else int("0" * i + "1" * (j - i + 1) + "0" * (31 - j), 2))
    return data, b"".join(m.to_bytes(4, "big") for m in mask)


def name12(s):
    return s.encode().ljust(12, b"\0")


def patch_icons(b, data, mask):
    b = bytearray(b)
    h = struct.unpack(">18H", b[:36])
    for i in range(h[13]):                     # nib ICONBLKs at h[3]
        o = h[3] + 34 * i
        pm, pd, pt = struct.unpack(">3l", b[o:o + 12])
        if b[pt:b.index(b"\0", pt)] == b"UPLOAD":
            assert struct.unpack(">2h", b[o + 22:o + 26]) == (32, 32)
            b[pm:pm + 128], b[pd:pd + 128] = mask, data
            b[pt:pt + 7] = b"MOUSE\0\0"
            return bytes(b)
    raise SystemExit("teradesk_mouse_icon: no UPLOAD icon in ICONS.RSC")


def patch_cicons(b, data, mask):
    b = bytearray(b)
    rssize = struct.unpack(">18H", b[:36])[17]
    table = struct.unpack(">l", b[rssize + 4:rssize + 8])[0]
    n = 0
    while struct.unpack(">l", b[table + 4 * n:table + 4 * n + 4])[0] != -1:
        n += 1
    p = table + 4 * (n + 1)
    for _ in range(n):
        w, hh = struct.unpack(">2h", b[p + 22:p + 26])
        ms = (w + 15) // 16 * 2 * hh
        nc = struct.unpack(">l", b[p + 34:p + 38])[0]
        md, mm, tx = p + 38, p + 38 + ms, p + 38 + 2 * ms
        hit = bytes(b[tx:tx + 12]).split(b"\0")[0] == b"UPLOAD"
        if hit:
            assert (w, hh) == (32, 32)
            b[md:md + ms], b[mm:mm + ms], b[tx:tx + 12] = data, mask, name12("MOUSE")
        q = tx + 12
        for _ in range(nc):
            np_, _cd, _cm, sd, _sm, _nx = struct.unpack(">h5l", b[q:q + 22])
            q += 22
            for sel in ((False, True) if sd else (False,)):
                if hit:
                    b[q:q + np_ * ms] = (bytes(x ^ 0xFF for x in data) if sel else data) * np_
                    b[q + np_ * ms:q + np_ * ms + ms] = mask
                q += np_ * ms + ms
        if hit:
            return bytes(b)
        p = q
    raise SystemExit("teradesk_mouse_icon: no UPLOAD icon in CICONS.RSC")


if __name__ == "__main__":
    icons, cicons, out = sys.argv[1:4]
    d, m = bitmaps()
    open(f"{out}/ICONS.RSC", "wb").write(patch_icons(open(icons, "rb").read(), d, m))
    open(f"{out}/CICONS.RSC", "wb").write(patch_cicons(open(cicons, "rb").read(), d, m))
