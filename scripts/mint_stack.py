#!/usr/bin/env python3
"""mint_stack — read or set a MiNT program's __stksize, as SpareMiNT specs do with `stack --fix=SIZE`
(mintbin's `stack` is not in our cross toolchain). Works on stripped binaries: the MiNT extended header
("MiNT" at 0x12) keeps __stksize's file position at 0x4c, as mintbin's `stack` reads it; nm is the fallback.

    scripts/mint_stack.py BIN            print the stack size
    scripts/mint_stack.py BIN 256k       set it
"""
import os
import struct
import subprocess
import sys

NM = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                  "tools/cross-mint/usr/bin/m68k-atari-mint-nm")


def offset(path):
    head = open(path, "rb").read(0x50)
    if head[0x12:0x16] == b"MiNT" and struct.unpack_from(">I", head, 0x4c)[0]:
        return struct.unpack_from(">I", head, 0x4c)[0]     # g_stkpos
    out = subprocess.run([NM, path], capture_output=True, text=True, check=True).stdout
    addr = next(int(l.split()[0], 16) for l in out.splitlines() if l.endswith(" __stksize"))
    return 0x1c + addr                                    # symbol values are relative to TEXT, which follows the 0x1c header


def main():
    path = sys.argv[1]
    off = offset(path)
    with open(path, "r+b") as f:
        if len(sys.argv) > 2:
            s = sys.argv[2].lower()
            n = int(s[:-1]) * 1024 if s.endswith("k") else int(s)
            f.seek(off)
            f.write(struct.pack(">i", n))
        f.seek(off)
        print(f"{path}: __stksize {struct.unpack('>i', f.read(4))[0]}")


if __name__ == "__main__":
    main()
