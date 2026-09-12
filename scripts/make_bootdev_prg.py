#!/usr/bin/env python3
"""Write BOOTDEV.PRG — an 18-byte AUTO program that clears the high byte of _bootdev ($446).

Why: when CBHD boots the disk through ICD's root-sector loader (C:\\ICDBOOT.SYS = CBHD.PRG),
_bootdev is left as 0x0202 instead of 0x0002 (Hatari debugger, 2026-09-12). FreeMiNT reads the
whole word, cannot find \\MINT\\1-19-4EB\\mint.cnf (`u:/9/...`), loads no modules and drops to MiS.
With the byte cleared it boots from C: into XaAES. Put it in C:\\AUTO before MINT-4EB.PRG.

Usage: make_bootdev_prg.py OUT.PRG
"""
import struct
import sys

CODE = bytes.fromhex(
    "42A7"        # clr.l  -(sp)          Super(0): enter supervisor mode
    "3F3C0020"    # move.w #$20,-(sp)
    "4E41"        # trap   #1
    "5C8F"        # addq.l #6,sp
    "42380446"    # clr.b  $446.w         _bootdev high byte -> 0
    "4267"        # clr.w  -(sp)          Pterm0
    "4E41"        # trap   #1
)

# GEMDOS header: magic, text, data, bss, symbols, reserved, flags, absflag; then no relocations.
header = struct.pack(">HIIIIIIH", 0x601A, len(CODE), 0, 0, 0, 0, 0, 0)
open(sys.argv[1], "wb").write(header + CODE + struct.pack(">I", 0))
