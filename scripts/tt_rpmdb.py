#!/usr/bin/env python3
"""tt_rpmdb — write the TT's rpm 3.0.6 database on the remote system management host, without the TT's rpm.

    scripts/tt_rpmdb.py check DBDIR [PKG.rpm ...]   prove the model against a real db (run this first)
    scripts/tt_rpmdb.py add   DBDIR PKG.rpm ...     register packages, like `rpm -i --justdb` on the TT

DBDIR is a copy of the TT's /var/lib/rpm (pull it with debugfs `rdump`, write it back with debugfs `write`).
Format, read from tools/rpm-src/rpm-3.0.6-4: packages.rpm is a falloc file (lib/falloc.c) of headers
without magic; the seven *.rpm indexes are big-endian db1.85 hashes of key -> {u32 offset, u32 filenum}[].
The stored header is the package header after rpm's install edits: changelog trimmed to
%_instchangelog (5), FILESTATES (all normal), INSTALLTIME, INSTPREFIXES = PREFIXES.
Old-format packages only (OLDFILENAMES); every SpareMiNT package on the cards is one.

Index writes go through the real db1.85 (tools/rpm-src/db1-1.85-1), built for the host on first use.
Measured 2026-10-10 on card 2: 6 stored headers rebuilt byte-identical, all 7 indexes regenerated
exactly from 73 headers; the TT's rpm under Hatari then read and updated the result (tt030-rpm-pipeline C9).
"""
import collections
import os
import struct
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DB1 = os.path.join(REPO, "tools/rpm-src/db1-1.85-1/db.1.85")
TOOL = os.path.join(REPO, "build/db1/tt_db1tool")
FA_MAGIC = 0x02050920
ALIGN = {3: 2, 4: 4, 5: 8}
WIDTH = {0: 0, 1: 1, 2: 1, 3: 2, 4: 4, 5: 8, 7: 1}
INDEXES = ("nameindex.rpm", "groupindex.rpm", "triggerindex.rpm", "conflictsindex.rpm",
           "requiredby.rpm", "providesindex.rpm", "fileindex.rpm")


def db1tool():
    """Build db1.85 + tt_db1tool into build/db1 once. 64-bit is fine for the hash code we use."""
    if os.path.exists(TOOL):
        return TOOL
    out = os.path.dirname(TOOL)
    os.makedirs(out, exist_ok=True)
    srcs = [f"{DB1}/{d}/{f}" for d in ("hash", "db", "mpool") for f in sorted(os.listdir(f"{DB1}/{d}"))
            if f.endswith(".c") and f not in ("hsearch.c", "ndbm.c")]
    inc = [f"-I{DB1}/PORT/linux/include", f"-I{DB1}/include"]
    stubs = f"{out}/stubs.c"                       # db.c references the btree/recno openers; hash needs neither
    open(stubs, "w").write("#include <stddef.h>\nvoid *__bt_open(){return NULL;}\nvoid *__rec_open(){return NULL;}\n")
    subprocess.run(["gcc", "-O1", "-w", "-std=gnu89", "-fcommon", "-D__DBINTERFACE_PRIVATE", "-Ddbopen=__dbopen",
                    *inc, "-o", TOOL, os.path.join(REPO, "scripts/tt_db1tool.c"), stubs, *srcs], check=True)
    return TOOL


def header(b, off):
    """(entries, data, length) of a magic-less header at off."""
    il, dl = struct.unpack_from(">II", b, off)
    ents = [struct.unpack_from(">iiii", b, off + 8 + 16 * i) for i in range(il)]
    return ents, b[off + 8 + 16 * il: off + 8 + 16 * il + dl], 8 + 16 * il + dl


def package_header(path):
    b = open(path, "rb").read()
    o = 96                                          # lead, then the signature header padded to 8
    il, dl = struct.unpack_from(">II", b, o + 8)
    o += 16 + 16 * il + dl
    o += (-o) % 8
    return header(b, o + 8)[:2]


def entries(ents, data):
    """[(tag, type, count, bytes)] with each entry's exact bytes."""
    out = []
    for t, ty, o, c in ents:
        if ty in (6, 8, 9):
            e = o
            for _ in range(1 if ty == 6 else c):
                e = data.index(b"\0", e) + 1
        else:
            e = o + c * WIDTH[ty]
        out.append((t, ty, c, data[o:e]))
    return out


def unload(es):
    """rpm's headerUnload: entries sorted by tag, data packed in that order with type alignment."""
    idx = data = b""
    for t, ty, c, b in sorted(es, key=lambda e: e[0]):
        data += b"\0" * ((-len(data)) % ALIGN.get(ty, 1))
        idx += struct.pack(">iiii", t, ty, len(data), c)
        data += b
    return struct.pack(">II", len(es), len(data)) + idx + data


def db_header(pkg, installtime):
    es = entries(*package_header(pkg))
    T = {e[0]: e for e in es}
    if 1027 not in T:
        sys.exit(f"{pkg}: no OLDFILENAMES; compressed-filelist packages are not modelled")
    if 1080 in T and T[1080][2] > 5:                # trimChangelog, %_instchangelog 5: keep the first 5
        for t in (1080, 1081, 1082):
            _, ty, c, b = T[t]
            b = b[:20] if t == 1080 else b"".join(x + b"\0" for x in b.split(b"\0")[:5])
            es[es.index(T[t])] = (t, ty, 5, b)
    n = T[1027][2]
    es += [(1008, 4, 1, struct.pack(">i", installtime)), (1029, 1, n, b"\0" * n)]
    if 1098 in T:
        es.append((1099, 8, T[1098][2], T[1098][3]))
    return unload(es)


def strs(T, tag):
    return [s.decode("latin-1") for s in T[tag][3].split(b"\0")[:T[tag][2]]] if tag in T else []


def index_entries(ents, data, off):
    """(index, key, offset, filenum) exactly as rpmdbAdd (lib/rpmdb.c) adds them."""
    T = {e[0]: e for e in entries(ents, data)}
    trig = strs(T, 1066)
    return ([("nameindex.rpm", strs(T, 1000)[0], off, 0),
             ("groupindex.rpm", (strs(T, 1016) or ["Unknown"])[0], off, 0)]
            + [("triggerindex.rpm", k, off, 0) for i, k in enumerate(trig) if k not in trig[:i]]
            + [("conflictsindex.rpm", k, off, 0) for k in strs(T, 1054)]
            + [("requiredby.rpm", k, off, 0) for k in strs(T, 1049)]
            + [("providesindex.rpm", k, off, 0) for k in strs(T, 1047)]
            + [("fileindex.rpm", p.rsplit("/", 1)[-1], off, i) for i, p in enumerate(strs(T, 1027))])


def records(b):
    """(header offset, block size) of every allocated falloc block."""
    off = 8
    while off < len(b):
        size, _, _, free = struct.unpack_from(">IIII", b, off)
        if not free:
            yield off + 16, size
        off += size


def check(dbdir, pkgs):
    b = open(f"{dbdir}/packages.rpm", "rb").read()
    want = collections.defaultdict(lambda: collections.defaultdict(list))
    stored = {}
    for off, _ in records(b):
        e, d, n = header(b, off)
        T = {x[0]: x for x in entries(e, d)}
        stored[strs(T, 1000)[0]] = (b[off:off + n], struct.unpack(">i", T[1008][3])[0])
        for f, k, o, fn in index_entries(e, d, off):
            want[f][k].append(f"{o}:{fn}")
    ok = True
    for p in pkgs:                                  # header model: rebuild a stored record from its package
        name = strs({x[0]: x for x in entries(*package_header(p))}, 1000)[0]
        if name in stored:
            same = db_header(p, stored[name][1]) == stored[name][0]
            ok &= same
            print(f"header {name}: {'identical' if same else 'DIFFERS'}")
    for f in INDEXES:                               # index model: regenerate every index from the headers
        out = subprocess.run([db1tool(), "dump", f"{dbdir}/{f}"], capture_output=True, check=True).stdout
        got = {k: sorted(v.split(",")) for k, v in (l.split("\t")[:2] for l in out.decode("latin-1").splitlines())}
        same = got == {k: sorted(v) for k, v in want[f].items()}
        ok &= same
        print(f"index {f}: {len(got)} keys {'exact' if same else 'DIFFERS'}")
    print(f"{len(stored)} packages", "OK" if ok else "FAILED")
    return ok


def add(dbdir, pkgs):
    tool = db1tool()                                # build before touching the db: a failed build must not leave it half-written
    p = f"{dbdir}/packages.rpm"
    b = bytearray(open(p, "rb").read())
    magic, first_free = struct.unpack_from(">II", b, 0)
    # ponytail: appends only; a db with free blocks (after an rpm -e) needs fadAlloc's first-fit search
    assert (magic, first_free) == (FA_MAGIC, 0), "expected a falloc file with an empty free list"
    have = {strs({x[0]: x for x in entries(*header(b, o)[:2])}, 1000)[0] for o, _ in records(b)}
    lines = collections.defaultdict(list)
    now = int(time.time())
    for pkg in pkgs:
        rec = db_header(pkg, now)
        name = strs({x[0]: x for x in entries(*header(rec, 0)[:2])}, 1000)[0]
        if name in have:
            sys.exit(f"{name} is already registered; refusing a duplicate")
        size = len(rec) + 24
        size += (-size) % 64                        # fadAlloc: header+footer overhead, 64-byte multiple
        off = len(b) + 16
        b += struct.pack(">IIII", size, 0, 0, 0) + rec + b"\0" * (size - 24 - len(rec)) + struct.pack(">II", size, 0)
        e, d, _ = header(b, off)
        for f, k, o, fn in index_entries(e, d, off):
            assert "\t" not in k and "\n" not in k
            lines[f].append(f"{k}\t{o}\t{fn}\n")
        print(f"{name}: header {len(rec)} B at {off}")
    open(p, "wb").write(b)
    for f, ls in lines.items():
        subprocess.run([tool, "add", f"{dbdir}/{f}"], input="".join(ls).encode("latin-1"), check=True)


if __name__ == "__main__":
    if len(sys.argv) < 3 or sys.argv[1] not in ("check", "add"):
        sys.exit(__doc__)
    if sys.argv[1] == "check":
        sys.exit(0 if check(sys.argv[2], sys.argv[3:]) else 1)
    add(sys.argv[2], sys.argv[3:])
