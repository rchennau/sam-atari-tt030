#!/usr/bin/env python3
"""wcpu_repack.py — a W-CPU rebuild as an update of the stock package (tt030-t425-kernel-ports FR-8).

  wcpu_repack.py STOCK.rpm OUT_DIR PATH=NEWFILE [PATH=NEWFILE ...] [--drop PATH ...]
      -> OUT_DIR/<name>-<version>-<release>.sam1.m68kmint.rpm

Takes SpareMiNT's own RPM and swaps in only the rebuilt executables (same SRPM, same version, only the
CPU flags changed), so docs, man pages, file modes, symlinks and dependencies stay as stock. The release
gains ".sam1", which rpmvercmp orders after the stock release, so `yum update` installs it and
`rpm -U --oldpackage STOCK.rpm` restores stock. Hard links in the stock payload (gzip/gunzip share one
inode, the body travels with the last) are written as separate copies.
--drop removes a stock file that collides with or shadows another installed package (gawk's /usr/bin/awk
and /bin/awk links against mawk, which yum needs; operator 2026-09-25: `awk` stays mawk). The stock FILEFLAGS (%config, %doc) are carried over per path, so an update keeps rpm's config-file handling.
Exit 0 written · 1 refused or bad input.
"""
import gzip
import bz2
import lzma
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import rpm_header as R  # noqa: E402



def payload(data, off):
    for magic, dec in ((b"\x1f\x8b\x08", gzip.decompress), (b"BZh", bz2.decompress), (b"\xfd7zXZ", lzma.decompress)):
        if data[off:off + len(magic)] == magic:
            return dec(data[off:])
    raise SystemExit("wcpu_repack: unknown payload compression")


def cpio(c):
    """(path, mode, ino, body) per entry of a newc archive; bodies of hard links resolved."""
    out, p, last = [], 0, {}
    while c[p:p + 6] in (b"070701", b"070702"):
        h = c[p:p + 110]
        ino, mode, fs, ns = int(h[6:14], 16), int(h[14:22], 16), int(h[54:62], 16), int(h[94:102], 16)
        name = c[p + 110:p + 110 + ns - 1].decode("latin1")
        q = (p + 110 + ns + 3) & ~3
        if name == "TRAILER!!!":
            break
        body = c[q:q + fs]
        out.append(["/" + name.lstrip("./"), mode, ino, body])
        if fs:
            last[ino] = body
        p = (q + fs + 3) & ~3
    for e in out:                                # a hard link's data is on one member only
        if not e[3] and (e[1] & 0o170000) == 0o100000 and e[2] in last:
            e[3] = last[e[2]]
    return out


def main(argv):
    if len(argv) < 4:
        print(__doc__, file=sys.stderr)
        return 1
    data = open(argv[1], "rb").read()
    sig, main_, off = R.rpm_header(data)
    flags = dict(zip(R.file_paths(main_), main_.get(R.FILEFLAGS) or []))   # %config/%doc carried over
    args, drop = argv[3:], set()
    while "--drop" in args:
        i = args.index("--drop")
        drop.add(args[i + 1])
        del args[i:i + 2]
    swap = dict(a.split("=", 1) for a in args)
    files = cpio(payload(data, off))
    if drop - {e[0] for e in files}:
        print(f"wcpu_repack: --drop not in the stock package: {sorted(drop - {e[0] for e in files})}", file=sys.stderr)
        return 1
    files = [e for e in files if e[0] not in drop]
    seen = set()
    for e in files:
        if e[0] in swap:
            e[3] = open(swap[e[0]], "rb").read()
            seen.add(e[0])
    missing = set(swap) - seen
    if missing:
        print(f"wcpu_repack: not in the stock package: {sorted(missing)}", file=sys.stderr)
        return 1
    name, ver, rel = main_[R.NAME], main_[R.VERSION], main_[R.RELEASE] + ".sam1"
    summary = (main_.get(R.SUMMARY) or [name])[0]
    out = os.path.join(argv[2], f"{name}-{ver}-{rel}.m68kmint.rpm")
    open(out, "wb").write(R.write_rpm(
        name, ver, rel, [(p, m, b) for p, m, _, b in files],
        summary=summary + " (sam: rebuilt for the 68030 + 68882)",
        requires_=R.requires(main_), provides=main_.get(R.PROVIDENAME) or (), fileflags=flags))
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
