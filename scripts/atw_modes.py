#!/usr/bin/env python3
"""atw_modes.py — decode the ATW800/2 XVDI mode list (PMS tt030-atw800-video-modes FR-1).

  atw_modes.py [XVDI.BIB [XVDIMENU.INF]]          -> TSV on stdout (defaults: staging/XVDI_ORIG/)
  atw_modes.py --md XVDI.BIB XVDIMENU.INF STATUS.TSV -> the docs/xvdi-mode-matrix.md table on stdout

XVDI.BIB is N fixed 86-byte records: a 34-byte text name, big-endian u16 fields (depth code, bytes per
line, bits per pixel, colours, ?, visible w-1/h-1, virtual w-1/h-1, then the timing block: label refresh, H front porch,
sync, back porch, display, V front porch, sync, back porch, display, PLL feedback, input and output dividers, 0, ctrl_reg),
ending in the marker 11 22 33 44 55 66. Pixel clock = 27 MHz x fb / in / 5; ctrl_reg bit 1 = +H sync, bit 2 = +V sync
(driver res.h, docs/resolution-management.md).
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
        label_hz, hfp, hs, hbp, hd, vfp, vs, vbp, vd, fb, idiv, _od, _z, ctrl = struct.unpack(">14H", r[52:80])
        if bpl * 8 // bpp != xw + 1:              # the row length must match the virtual width
            raise ValueError(f"record {i + 1}: {bpl} bytes/line at {bpp} bpp is not {xw + 1} pixels")
        name = r[:34].decode("latin-1").rstrip(" \0")
        vram = bpl * (xh + 1)
        rows.append(dict(mode=i + 1, name=" ".join(name.split()), w=vw + 1, h=vh + 1, virt_w=xw + 1, virt_h=xh + 1,
                         bpp=bpp, bytes_per_line=bpl, vram_kb=round(vram / 1024), fits_2mb=vram <= WINDOW,
                         split="(Split)" in name))
        ht, vt = hfp + hs + hbp + hd, vfp + vs + vbp + vd
        pclk = 27.0 * fb / idiv / 5 if idiv else 0.0            # ponytail: 0 = no timing block (test records)
        rows[-1].update(label_hz=label_hz, pclk_mhz=round(pclk, 1), htotal=ht, vtotal=vt,
                        h_khz=round(pclk * 1000 / ht, 1) if ht else 0.0,
                        v_hz=round(pclk * 1e6 / ht / vt, 1) if ht and vt else 0.0,
                        hsync="+" if ctrl & 2 else "-", vsync="+" if ctrl & 4 else "-",
                        h_timing=f"{hfp}/{hs}/{hbp}", v_timing=f"{vfp}/{vs}/{vbp}")
    return rows


def markdown(rows, current, status):
    """One table row per mode; status (mode -> dict from STATUS.TSV) adds the measured columns."""
    out = ["| Mode | Name | Visible | Colours | VRAM KB | 2 MB | Pixel MHz | H kHz | V Hz (label) | Total H×V | H fp/sync/bp | "
           "V fp/sync/bp | Sync | Monitor | T425 offload | Notes |", "|" + "---|" * 16]
    for r in rows:
        st = status.get(r["mode"], {})
        mark = " **(boot)**" if r["mode"] == current else ""
        out.append(f"| {r['mode']}{mark} | {r['name']} | {r['w']}×{r['h']} | {2 ** r['bpp']} | {r['vram_kb']} | "
                   f"{'yes' if r['fits_2mb'] else 'NO'} | {r['pclk_mhz']} | {r['h_khz']} | {r['v_hz']} ({r['label_hz']}) | "
                   f"{r['htotal']}×{r['vtotal']} | {r['h_timing']} | {r['v_timing']} | {r['hsync']}H {r['vsync']}V | "
                   f"{st.get('monitor', 'untested')} | {st.get('t425', '—')} | {st.get('notes', '')} |")
    return "\n".join(out)


def main(argv):
    if len(argv) == 5 and argv[1] == "--md":
        rows = decode(open(argv[2], "rb").read())
        current = struct.unpack(">H", open(argv[3], "rb").read(2))[0] + 1
        lines = [l.rstrip("\n").split("\t") for l in open(argv[4], encoding="utf-8") if not l.startswith("#")]
        status = {int(l[0]): dict(zip(lines[0][1:], l[1:])) for l in lines[1:] if l and l[0]}
        print(markdown(rows, current, status))
        return 0
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
