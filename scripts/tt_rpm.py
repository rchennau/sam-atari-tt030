#!/usr/bin/env python3
"""tt_rpm.py — host CLI for the TT030 package mirror (tt030-rpm-pipeline FR-7).

  tt_rpm.py index MIRROR     rebuild MIRROR/index.tsv from MIRROR/{m68kmint,noarch,built}/*.rpm
  tt_rpm.py synth OUT.rpm    write the Phase-1 format-gate test RPM (symlink + mode-0750 file)

index.tsv: name version release arch path size sha256 requires(csv) provides(csv), one package
per line so the TT reads it with awk. A malformed RPM is named on stderr and the run exits 1
WITHOUT writing: the index never shrinks silently (FR-2).
"""
import hashlib
import os
import re
import sys

import rpm_header as R

POOLS = ("m68kmint", "noarch", "built")


def rpmvercmp(a, b):
    """rpm's segment compare: digits vs digits numerically, alpha vs alpha lexically, digit > alpha."""
    if a == b:
        return 0
    sa, sb = re.findall(r"\d+|[A-Za-z]+", a), re.findall(r"\d+|[A-Za-z]+", b)
    for x, y in zip(sa, sb):
        if x.isdigit() != y.isdigit():
            return 1 if x.isdigit() else -1
        if x.isdigit():
            x, y = int(x), int(y)
        if x != y:
            return 1 if x > y else -1
    return (len(sa) > len(sb)) - (len(sa) < len(sb))


def newer(p, q):
    """True if p should replace q in the index: rpmvercmp newest wins, built/ wins a tie (FR-7)."""
    c = rpmvercmp(p["version"], q["version"]) or rpmvercmp(p["release"], q["release"])
    return c > 0 or (c == 0 and p["pool"] == "built" and q["pool"] != "built")


def scan(mirror):
    pkgs, bad = [], []
    for pool in POOLS:
        d = os.path.join(mirror, pool)
        for f in sorted(os.listdir(d)) if os.path.isdir(d) else []:
            if not f.endswith(".rpm"):
                continue
            data = open(os.path.join(d, f), "rb").read()
            try:
                _, m, _ = R.rpm_header(data)
            except R.BadRpm as e:
                bad.append(f"{pool}/{f}: {e}")
                continue
            pkgs.append(dict(name=m[R.NAME], version=m[R.VERSION], release=m[R.RELEASE],
                             arch=m.get(R.ARCH) or "noarch", pool=pool, path=f"{pool}/{f}",
                             size=len(data), sha256=hashlib.sha256(data).hexdigest(),
                             requires=R.requires(m), provides=m.get(R.PROVIDENAME, []),
                             files=R.file_paths(m)))
    return pkgs, bad


def build_index(pkgs):
    best = {}
    for p in pkgs:
        if p["name"] not in best or newer(p, best[p["name"]]):
            best[p["name"]] = p
    # file-provides, bounded to paths something actually requires (FR-2)
    wanted = {r for p in best.values() for r in p["requires"] if r.startswith("/")}
    lines = []
    for p in sorted(best.values(), key=lambda p: p["name"]):
        prov = dict.fromkeys([p["name"], *p["provides"], *sorted(wanted & set(p["files"]))])
        lines.append("\t".join([p["name"], p["version"], p["release"], p["arch"], p["path"],
                                str(p["size"]), p["sha256"], ",".join(dict.fromkeys(p["requires"])),
                                ",".join(prov)]))
    return "".join(line + "\n" for line in lines)


def cmd_index(mirror):
    pkgs, bad = scan(mirror)
    for b in bad:
        print(f"REJECTED {b}", file=sys.stderr)
    if bad:
        print(f"index NOT written: {len(bad)} malformed RPM(s)", file=sys.stderr)
        return 1
    out = os.path.join(mirror, "index.tsv")
    with open(out + ".tmp", "w") as fh:
        fh.write(build_index(pkgs))
    os.replace(out + ".tmp", out)
    print(f"{out}: {len(pkgs)} RPMs scanned")
    return 0


def synth_gate_rpm():
    """Phase-1 format gate: the cases a hand-rolled v3 cpio writer typically breaks on."""
    return R.write_rpm("samgate", "1.0", "1", [
        ("/usr/share/samgate/hello.txt", 0o100644, b"sam tt030 rpm format gate\n"),
        ("/usr/share/samgate/run.sh", 0o100750, b"#!/bin/sh\necho samgate ok\n"),
        ("/usr/share/samgate/link", 0o160777, b"hello.txt"),
    ], summary="tt030-rpm-pipeline Phase 1 format gate (remove with rpm -e samgate)")


def main(argv):
    if len(argv) == 3 and argv[1] == "index":
        return cmd_index(argv[2])
    if len(argv) == 3 and argv[1] == "synth":
        open(argv[2], "wb").write(synth_gate_rpm())
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
