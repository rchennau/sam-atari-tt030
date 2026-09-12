#!/usr/bin/env python3
"""Recreate symlinks that SpareMiNT RPM payloads store with file type 0160000.

GNU cpio only knows the POSIX symlink type 0120000 and rejects these as "unknown file type"
(measured 2026-09-12: gzip's `usr/bin/gzip` is mode 160777, 14 bytes = the link target), so
~13 aliases (usr/bin/gzip, slogin, gtar, reset, zoneinfo links, ...) went missing.

Usage: cpio_symlinks.py ROOT FILE.cpio [...]   (newc/crc archives, as 7z extracts them)
"""
import os
import sys

NONSTD_LINK = 0o160000
TYPE_MASK = 0o170000


def entries(path):
    d = open(path, "rb").read()
    i = 0
    while d[i:i + 6] in (b"070701", b"070702"):
        h = d[i:i + 110]
        mode, fsz, nsz = int(h[14:22], 16), int(h[54:62], 16), int(h[94:102], 16)
        name = d[i + 110:i + 110 + nsz - 1].decode("latin1")
        i = (i + 110 + nsz + 3) & ~3
        body = d[i:i + fsz]
        i = (i + fsz + 3) & ~3
        if name == "TRAILER!!!":
            return
        yield name, mode, body


def main(root, archives):
    made = 0
    for a in archives:
        for name, mode, body in entries(a):
            if mode & TYPE_MASK != NONSTD_LINK:
                continue
            dst = os.path.join(root, name.lstrip("./"))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            if os.path.lexists(dst):
                os.remove(dst)
            os.symlink(body.decode("latin1"), dst)
            made += 1
    print(f"cpio_symlinks: recreated {made} symlinks")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
