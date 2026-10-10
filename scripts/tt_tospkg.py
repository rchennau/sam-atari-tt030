#!/usr/bin/env python3
"""tt_tospkg — package native TOS/GEM programs from a card's FAT drives as an RPM (PMS tt030-fhs-layout FR-3).

    scripts/tt_tospkg.py packaging/<name>.json HD00.hda OUT_DIR

The recipe (JSON, in git) names files only; the bytes come from the HD00 image (gitignored build/), so commercial
software is never committed (plan C4/C5). Its RPM belongs on the LAN mirror and the card, never on GitHub.
Recipe keys:
  name, version, release, summary, licence      package identity
  dir        "D:\\APPS\\WORDPLUS"                   FAT directory copied as a tree
  dest       "/opt/wordplus"                       where it lands; file names keep their upper case (TOS programs
                                                    open their resources by the name they were written with)
  exclude    ["INSTALL\\", "*.ACC"]                 relative paths or *.EXT patterns left behind (installers, ACC/CPX)
  commands   {"wordplus": "WORDPLUS.PRG"}          /usr/bin/<cmd> wrappers: cd into dest, then run the program (C3)
  replace    {"WORDPLUS.INF": [["D:\\WORDPLUS\\", "U:\\OPT\\WORDPLUS\\"]]}   byte rewrites in config files that
                                                    hold the old FAT path (each pair must match at least once)
  config     ["WORDPLUS.INF"]                      files the program rewrites (settings): %config, kept on update
"""
import fnmatch
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import atari_fat16 as A  # noqa: E402
import rpm_header as R  # noqa: E402


def fat_tree(img, top):
    """{relative path with '/' : bytes} for every file under the FAT directory `top` (DRIVE:\\DIR)."""
    drive, sub = top.split(":", 1)
    for d, off, size in A.partitions(img):
        if d == drive.upper():
            part = A.Part(img[off:off + size])
            pre = f"{d}:{sub.rstrip(chr(92))}\\".upper()
            return {p[len(pre):].replace("\\", "/"): data for p, data in A.walk(part.root, d + ":")
                    if data is not None and p.upper().startswith(pre)}
    sys.exit(f"tt_tospkg: drive {drive} not in this image")


def excluded(rel, patterns):
    up = rel.upper()
    return any(up.startswith(p.upper().replace("\\", "/")) if not p.startswith("*") else fnmatch.fnmatch(up, p.upper())
               for p in patterns)


def main(recipe, hd00, out_dir):
    r = json.load(open(recipe))
    tree = fat_tree(open(hd00, "rb").read(), r["dir"])
    if not tree:
        sys.exit(f"tt_tospkg: {r['dir']} is empty or missing in {hd00}")
    for rel, pairs in r.get("replace", {}).items():
        if rel not in tree:
            sys.exit(f"tt_tospkg: replace: {rel} is not in {r['dir']}")
        for old, new in pairs:
            if old.encode("latin-1") not in tree[rel]:
                sys.exit(f"tt_tospkg: replace: {old!r} not found in {rel}")
            tree[rel] = tree[rel].replace(old.encode("latin-1"), new.encode("latin-1"))
    files = [(f"{r['dest']}/{rel}", 0o100755 if rel.upper().endswith((".PRG", ".APP", ".TTP", ".TOS", ".GTP")) else 0o100644, b)
             for rel, b in sorted(tree.items()) if not excluded(rel, r.get("exclude", []))]
    dirs = {r["dest"]} | {p.rsplit("/", 1)[0] for p, _, _ in files}
    for d in sorted(dirs):                       # every parent too, so rpm -e removes the whole tree
        while d != r["dest"] and d.rsplit("/", 1)[0] not in dirs:
            d = d.rsplit("/", 1)[0]
            dirs.add(d)
    files = [(d, 0o40755, b"") for d in sorted(dirs)] + files
    have = {p for p, _, _ in files}
    for cmd, prog in r.get("commands", {}).items():
        target = f"{r['dest']}/{prog}"
        if target not in have:
            sys.exit(f"tt_tospkg: command {cmd}: {target} is not in the package")
        d, base = target.rsplit("/", 1)
        files.append((f"/usr/bin/{cmd}", 0o100755, f'#!/bin/sh\ncd "{d}" && exec "./{base}" "$@"\n'.encode()))
    cfg = {f"{r['dest']}/{c}" for c in r.get("config", [])}
    if cfg - have:
        sys.exit(f"tt_tospkg: config not in the package: {sorted(cfg - have)}")
    out = os.path.join(out_dir, f"{r['name']}-{r['version']}-{r['release']}.m68kmint.rpm")
    open(out, "wb").write(R.write_rpm(r["name"], r["version"], r["release"], files, summary=r["summary"],
                                      description=f"Licence: {r['licence']}. Packaged from the TT's own disk by "
                                                  "tt_tospkg (sam); not for redistribution.",
                                      fileflags={p: 1 for p in cfg}))   # RPMFILE_CONFIG
    print(f"{out}: {len(files)} files")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
