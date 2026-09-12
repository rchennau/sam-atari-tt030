#!/usr/bin/env python3
"""Build the TT030 bootstrap floppy image (FR-6) from floppy/manifest.txt — no root needed.

Usage: build_boot_floppy.py OUT.img   (needs pyfatfs; mkfs.fat creates the Atari FAT12 fs)

Manifest lines: `<dest path on A:>  <source path relative to the repo root>`, written in file
order. ORDER IS LOAD-BEARING for AUTO/: TOS runs AUTO programs in directory order, not
alphabetically, so the SCSI driver must be written before STING.PRG.
Then verify by reading every file back and comparing bytes; exits 1 on any mismatch.
"""
import subprocess
import sys
from pathlib import Path

import fs.errors
from pyfatfs.PyFatFS import PyFatFS

REPO = Path(__file__).resolve().parent.parent
MANIFEST = REPO / "floppy" / "manifest.txt"


def entries():
    for raw in MANIFEST.read_text().splitlines():
        line = raw.split("#", 1)[0].strip()
        if line:
            dest, src = line.split()
            yield dest.replace("\\", "/").lstrip("/"), REPO / src


def main(out: str) -> int:
    img = Path(out)
    img.unlink(missing_ok=True)
    # Standard (not -A Atari) layout: pyfatfs rejects the Atari 0x60 boot branch, and TOS 3.06
    # reads DOS-format 1.44 MB disks. The boot sector is not 0x1234-checksummed, so TOS never
    # executes it; the AUTO folder is what runs.
    subprocess.run(["mkfs.fat", "-C", "-F", "12", "-n", "TTBOOT", str(img), "1440"],
                   check=True, stdout=subprocess.DEVNULL)
    items = list(entries())
    fat = PyFatFS(str(img))
    try:
        for dest, src in items:
            parent = str(Path(dest).parent)
            if parent != ".":
                try:
                    fat.makedirs(parent)
                except fs.errors.DirectoryExists:
                    pass
            fat.writebytes(dest, src.read_bytes())
        bad = [d for d, s in items if fat.readbytes(d) != s.read_bytes()]
    finally:
        fat.close()
    for d in bad:
        print(f"MISMATCH {d}", file=sys.stderr)
    print(f"{img}: {len(items)} files, {len(bad)} mismatches")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
