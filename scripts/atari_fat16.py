#!/usr/bin/env python3
"""Rebuild Atari/ICD FAT16 partitions (8192 B logical sectors) inside a BlueSCSI .hda image.

No Linux tool writes these (vfat/fsck.fat/pyfatfs/mtools cap at 4096 B/sector), so this rebuilds
each partition from a tree instead of editing in place: parse -> apply plan -> lay files out
contiguously -> re-parse and compare every file byte-for-byte. The source image is never written.

Usage: atari_fat16.py SRC.hda PLAN.json OUT.hda
Plan ops (paths are DRIVE:\\DIR\\NAME, 8.3 upper-case):
  {"op": "move",  "src": "C:\\WORDPLUS", "dst": "D:\\APPS\\WORDPLUS"}
  {"op": "del",   "path": "C:\\AUTO\\00JAR032.PRG"}
  {"op": "put",   "path": "C:\\STING\\TCP.STX", "host": "staging/STING/TCP.STX"}   # file or dir
  {"op": "mkdir", "path": "E:\\ARCHIVE"}
"""
import json
import struct
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


class Node:
    def __init__(self, name, attr, raw=None, data=b"", kids=None):
        self.name, self.attr, self.raw, self.data = name, attr, raw, data
        self.kids = kids if kids is not None else []   # ordered: AUTO order is directory order

    @property
    def is_dir(self):
        return bool(self.attr & 0x10)


class Part:
    def __init__(self, blob):
        self.blob = blob
        (self.bps, self.spc, self.res, self.nfats, self.nroot, t16) = struct.unpack_from("<HBHBHH", blob, 11)
        self.fatsz = struct.unpack_from("<H", blob, 22)[0]
        self.total = t16 or struct.unpack_from("<I", blob, 32)[0]
        self.fat_off = self.res * self.bps
        self.root_off = self.fat_off + self.nfats * self.fatsz * self.bps
        self.data_off = self.root_off + self.nroot * 32
        self.csize = self.spc * self.bps
        self.nclus = min((len(blob) - self.data_off) // self.csize, self.fatsz * self.bps // 2 - 2)
        fat = blob[self.fat_off:self.fat_off + self.fatsz * self.bps]
        self.label = []
        self.root = Node("", 0x10, kids=self._read(blob[self.root_off:self.data_off], fat, top=True))

    def _read(self, raw, fat, top=False):
        out = []
        for i in range(0, len(raw), 32):
            e = raw[i:i + 32]
            if e[0] == 0:
                break
            if e[0] == 0xE5 or e[11] == 0x0F or e[:2] in (b". ", b".."):
                continue
            if e[11] & 0x08:
                if top:
                    self.label.append(bytes(e))
                continue
            clus, size = struct.unpack_from("<H", e, 26)[0], struct.unpack_from("<I", e, 28)[0]
            body = b"".join(self.blob[self.data_off + (c - 2) * self.csize:
                                      self.data_off + (c - 1) * self.csize] for c in self._chain(fat, clus))
            n = Node(name83(e[:11]), e[11], raw=bytes(e))
            if n.is_dir:
                n.kids = self._read(body, fat)
            else:
                n.data = body[:size]
            out.append(n)
        return out

    @staticmethod
    def _chain(fat, c):
        seen = 0
        while 2 <= c < 0xFFF0 and seen < 1 << 16:
            yield c
            c, seen = struct.unpack_from("<H", fat, c * 2)[0], seen + 1

    def build(self):
        """Lay the tree out contiguously; returns the new partition bytes (same length)."""
        out = bytearray(len(self.blob))
        out[:self.fat_off] = self.blob[:self.fat_off]          # boot sector + reserved, untouched
        fat = [0] * (self.nclus + 2)
        fat[0], fat[1] = 0xFF00 | self.blob[21], 0xFFFF
        nxt = [2]

        def alloc(nbytes):
            n = max(1, -(-nbytes // self.csize))
            first = nxt[0]
            if first + n - 2 > self.nclus:
                raise SystemExit(f"partition full ({self.nclus * self.csize >> 20} MiB)")
            for c in range(first, first + n):
                fat[c] = c + 1
            fat[first + n - 1] = 0xFFFF
            nxt[0] += n
            return first

        def place(node):
            if node.is_dir:
                node.clus = alloc((len(node.kids) + 2) * 32)
            else:
                node.clus = alloc(len(node.data)) if node.data else 0
            for k in node.kids:
                place(k)

        for k in self.root.kids:
            place(k)

        def entries(node, parent_clus):
            b = bytearray()
            if node is not self.root:
                b += dirent(b".          ", 0x10, node.clus, 0, node.raw)
                b += dirent(b"..         ", 0x10, parent_clus, 0, node.raw)
            for k in node.kids:
                b += dirent(pad83(k.name).encode("latin1"), k.attr, k.clus,
                            0 if k.is_dir else len(k.data), k.raw)
            return b

        def emit(node, parent_clus):
            if node.is_dir:
                self._put(out, node.clus, entries(node, parent_clus))
                for k in node.kids:
                    emit(k, node.clus)
            elif node.data:
                self._put(out, node.clus, node.data)

        root = b"".join(self.label) + entries(self.root, 0)
        if len(root) > self.nroot * 32:
            raise SystemExit("root directory full")
        out[self.root_off:self.root_off + len(root)] = root
        for k in self.root.kids:
            emit(k, 0)
        fatb = struct.pack(f"<{len(fat)}H", *fat)
        for i in range(self.nfats):
            o = self.fat_off + i * self.fatsz * self.bps
            out[o:o + len(fatb)] = fatb
        return bytes(out)

    def _put(self, out, clus, data):
        o = self.data_off + (clus - 2) * self.csize
        out[o:o + len(data)] = data


def name83(raw11):
    base, ext = raw11[:8].decode("latin1").rstrip(), raw11[8:].decode("latin1").rstrip()
    return base + ("." + ext if ext else "")


def pad83(name):
    base, _, ext = name.upper().partition(".")
    if not (0 < len(base) <= 8 and len(ext) <= 3):
        raise SystemExit(f"not an 8.3 name: {name!r}")
    return base.ljust(8) + ext.ljust(3)


def dirent(name11, attr, clus, size, raw):
    t = time.localtime()
    e = bytearray(raw) if raw else bytearray(32)
    if not raw:
        e[22:26] = struct.pack("<HH", (t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec // 2),
                               ((t.tm_year - 1980) << 9) | (t.tm_mon << 5) | t.tm_mday)
    e[0:11] = name11.ljust(11)[:11]
    e[11] = attr
    struct.pack_into("<HI", e, 26, clus, size)
    return bytes(e)


def host_node(path, name):
    if not path.is_dir():
        return Node(name, 0x20, data=path.read_bytes())
    kids, used = [], set()
    for p in sorted(path.iterdir()):
        n = short(p.name, used)
        used.add(n)
        kids.append(host_node(p, n))
    return Node(name, 0x10, kids=kids)


def short(name, used=()):
    """8.3 name for a host file; long names become BASE~N (unique in the dir), never truncated silently."""
    base, _, ext = name.upper().partition(".")
    if 0 < len(base) <= 8 and len(ext) <= 3 and "." not in ext:
        return base + ("." + ext if ext else "")
    ext = ext.rsplit(".", 1)[-1][:3]
    for i in range(1, 100):
        cand = f"{base.replace('.', '').replace(' ', '')[:8 - 1 - len(str(i))]}~{i}" + ("." + ext if ext else "")
        if cand not in used:
            return cand
    raise SystemExit(f"cannot shorten {name!r}")


def partitions(img):
    """Atari root sector: 4 x 12-byte entries at 0x1C6 (flag, id[3], start, size in 512 B units)."""
    out = []
    for i in range(4):
        flag, pid, start, size = struct.unpack_from(">B3sII", img, 0x1C6 + 12 * i)
        if flag & 1:
            out.append((chr(ord("C") + len(out)), start * 512, size * 512))
    return out


def walk(node, prefix):
    for k in node.kids:
        p = f"{prefix}\\{k.name}"
        if k.is_dir:
            yield p + "\\", None
            yield from walk(k, p)
        else:
            yield p, k.data


def main(src, plan, dst):
    img = bytearray(Path(src).read_bytes())
    layout = partitions(img)
    parts = {d: Part(bytes(img[o:o + n])) for d, o, n in layout}

    def locate(path, create=False):
        drive, *names = path.split("\\")
        node = parts[drive.rstrip(":")].root
        for name in names[:-1]:
            nxt = next((k for k in node.kids if k.name == name and k.is_dir), None)
            if nxt is None:
                if not create:
                    raise SystemExit(f"no such directory in {path}")
                nxt = Node(name, 0x10)
                pad83(name)
                node.kids.append(nxt)
            node = nxt
        return node, names[-1]

    def pop(path):
        parent, name = locate(path)
        for i, k in enumerate(parent.kids):
            if k.name == name:
                return parent.kids.pop(i)
        raise SystemExit(f"not found: {path}")

    def insert(path, node):
        parent, name = locate(path, create=True)
        pad83(name)
        if any(k.name == name for k in parent.kids):
            raise SystemExit(f"already exists: {path}")
        node.name = name
        parent.kids.append(node)

    for op in json.loads(Path(plan).read_text()):
        kind = op["op"]
        if kind == "move":
            insert(op["dst"], pop(op["src"]))
        elif kind == "del":
            pop(op["path"])
        elif kind == "mkdir":
            insert(op["path"], Node("", 0x10))
        elif kind == "put":
            host = Path(op["host"]) if Path(op["host"]).is_absolute() else REPO / op["host"]
            parent, name = locate(op["path"], create=True)
            parent.kids = [k for k in parent.kids if k.name != name]
            parent.kids.append(host_node(host, name))
        else:
            raise SystemExit(f"unknown op {kind}")

    bad = 0
    for d, o, n in layout:
        built = parts[d].build()
        want = dict(walk(parts[d].root, d + ":"))
        got = dict(walk(Part(built).root, d + ":"))
        for p in sorted(set(want) | set(got)):
            if want.get(p, "missing") != got.get(p, "missing"):
                print(f"MISMATCH {p}", file=sys.stderr)
                bad += 1
        img[o:o + n] = built
        files = [v for v in want.values() if v is not None]
        print(f"{d}: {len(files)} files, {sum(map(len, files)) / 2**20:.1f} MiB")
    if bad:
        raise SystemExit(f"{bad} mismatches — output NOT written")
    Path(dst).write_bytes(img)
    print(f"wrote {dst}")


if __name__ == "__main__":
    main(*sys.argv[1:4])
