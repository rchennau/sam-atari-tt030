#!/usr/bin/env python3
"""atw_modes.py — decode the ATW800/2 XVDI mode list (PMS tt030-atw800-video-modes FR-1).

  atw_modes.py [XVDI.BIB [XVDIMENU.INF]]     -> TSV on stdout (defaults: staging/XVDI_ORIG/)

XVDI.BIB is N fixed 86-byte records: a 34-byte text name, big-endian u16 fields (depth code, bytes per
line, bits per pixel, colours, ?, visible w-1/h-1, virtual w-1/h-1, timings), ending in the marker 11 22 33 44 55 66.
Video RAM = bytes per line x virtual height, which also covers the "(Split)" rows (virtual wider than visible).
The mode number is the record's position (1-based); XVDIMENU.INF word 0 is the selected one, 0-BASED (word 0 = mode - 1): the TT's 14 = mode 15 = 1024 x 768 65536,
the mode XaAES boots in (measured 2026-10-15: word 10 booted 800x600x16 by Line-A; atwshot reads RGB565). XVDI_R.BIB names 640x480x2 and is NOT the desktop mode (the redirector's own screen).
Fails loudly (exit 1) on a size that is not a multiple of 86, a missing marker, or a
row length that does not match the virtual width.
"""
import os
import struct
import sys

REC, MARK, WINDOW = 86, bytes.fromhex("112233445566"), 2 * 1024 * 1024


def decode(blob):
    if len(blob) % REC:
        raise ValueError(f"size {len(blob)} is not a multiple of {REC}")
    rows = []
    for i in range(len(blob) // REC):
        r = blob[i * REC:(i + 1) * REC]
        if r[-6:] != MARK:
            raise ValueError(f"record {i + 1}: no end marker")
        _code, bpl, bpp, _colours, _u, vw, vh, xw, xh = struct.unpack(">9H", r[34:52])
        if bpl * 8 // bpp != xw + 1:              # the row length must match the virtual width
            raise ValueError(f"record {i + 1}: {bpl} bytes/line at {bpp} bpp is not {xw + 1} pixels")
        name = r[:34].decode("latin-1").rstrip(" \0")
        vram = bpl * (xh + 1)
        rows.append(dict(mode=i + 1, name=" ".join(name.split()), w=vw + 1, h=vh + 1, virt_w=xw + 1, virt_h=xh + 1,
                         bpp=bpp, bytes_per_line=bpl, vram_kb=round(vram / 1024), fits_2mb=vram <= WINDOW,
                         split="(Split)" in name))
    return rows


def main(argv):
    here = os.path.join(os.path.dirname(__file__), "..", "staging", "XVDI_ORIG")
    bib = argv[1] if len(argv) > 1 else os.path.join(here, "XVDI.BIB")
    inf = argv[2] if len(argv) > 2 else os.path.join(here, "XVDIMENU.INF")
    try:
        rows = decode(open(bib, "rb").read())
    except ValueError as e:
        print(f"atw_modes: {bib}: {e}", file=sys.stderr)
        return 1
    current = struct.unpack(">H", open(inf, "rb").read(2))[0] + 1 if os.path.exists(inf) else None
    cols = ["mode", "name", "w", "h", "virt_w", "virt_h", "bpp", "bytes_per_line", "vram_kb", "fits_2mb", "split"]
    print("\t".join(cols + ["current"]))
    for r in rows:
        print("\t".join(str(r[c]) for c in cols) + ("\tyes" if r["mode"] == current else "\t"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
