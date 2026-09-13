#!/usr/bin/env python3
"""Copy a FreeMiNT keyboard table with the keys right of L and P (scancodes 0x27, 0x1A; UK ; and [, German Ö and Ü) typing a space.

Why: the TT's space bar is dead (2026-09-12); the operator touch-types, so the space moves to
the keys right of L and P. Unshifted and caps tables get ' '; Shift still types the shifted character.
FreeMiNT loads <sysdir>/keyboard.tbl at boot; TOS gets the same patch from src/helpspc.c.

Format (measured on en_uk.tbl / de_de.tbl): 4-byte header (0x2772, version), then 128-byte
unshifted, shifted and caps tables indexed by scancode. Space (0x39) is 0x20 in all three.

Usage: make_space_keys_tbl.py IN.tbl OUT.tbl
"""
import sys

SC_KEYS, SC_SPACE = (0x27, 0x1A), 0x39
UNSHIFT, SHIFT, CAPS = 4, 4 + 128, 4 + 256

t = bytearray(open(sys.argv[1], "rb").read())
if t[:2] != b"\x27\x72":
    sys.exit("not a FreeMiNT keyboard table")
for base in (UNSHIFT, SHIFT, CAPS):
    if t[base + SC_SPACE] != 0x20:
        sys.exit(f"unexpected layout: no space at table offset {base}")
for base in (UNSHIFT, CAPS):
    for sc in SC_KEYS:
        t[base + sc] = 0x20
open(sys.argv[2], "wb").write(t)
