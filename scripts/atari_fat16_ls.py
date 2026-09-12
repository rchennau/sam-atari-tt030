"""Read-only lister for Atari/ICD FAT16 partitions with large logical sectors (8192 B).

Linux vfat, fsck.fat and pyfatfs all cap sector size at 4096, so they cannot read these.
Usage: python atari_fat16_ls.py PART.img [PART.img ...]
"""
import struct
import sys


def lister(path):
    data = open(path, "rb").read()
    bps, spc, res, nfats, nroot, total16 = struct.unpack_from("<HBHBHH", data, 11)
    fatsz = struct.unpack_from("<H", data, 22)[0]
    total = total16 or struct.unpack_from("<I", data, 32)[0]
    fat_off = res * bps
    fat = data[fat_off:fat_off + fatsz * bps]
    root_off = fat_off + nfats * fatsz * bps
    data_off = root_off + nroot * 32
    csize = spc * bps
    nclus = (total * bps - data_off) // csize

    def chain(c):
        while 2 <= c < 0xFFF0:
            yield c
            c = struct.unpack_from("<H", fat, c * 2)[0]

    def entries(raw):
        for i in range(0, len(raw), 32):
            e = raw[i:i + 32]
            if e[0] == 0:
                break
            if e[0] == 0xE5 or e[11] == 0x0F or e[11] & 0x08:
                continue
            name = e[:8].decode("latin1").rstrip()
            ext = e[8:11].decode("latin1").rstrip()
            yield (name + ("." + ext if ext else ""), e[11],
                   struct.unpack_from("<H", e, 26)[0], struct.unpack_from("<I", e, 28)[0])

    def walk(raw, prefix):
        for name, attr, clus, size in entries(raw):
            if name in (".", ".."):
                continue
            full = prefix + name
            if attr & 0x10:
                sub = b"".join(data[data_off + (c - 2) * csize:data_off + (c - 1) * csize]
                               for c in chain(clus))
                yield full + "/", 0
                yield from walk(sub, full + "/")
            else:
                yield full, size

    used = sum(1 for i in range(2, nclus + 2)
               if struct.unpack_from("<H", fat, i * 2)[0] != 0)
    files = list(walk(data[root_off:data_off], ""))
    return (bps, spc, nclus * csize, used * csize), files


if __name__ == "__main__":
    for p in sys.argv[1:]:
        (bps, spc, cap, used), files = lister(p)
        print(f"=== {p}: {bps} B/sector x{spc}, capacity {cap/2**20:.0f} MiB, used {used/2**20:.1f} MiB, {len(files)} entries")
        tops = {}
        for f, s in files:
            top = f.split("/")[0] + ("/" if "/" in f else "")
            tops[top] = tops.get(top, 0) + s
        for t, s in sorted(tops.items(), key=lambda x: -x[1]):
            print(f"  {s/2**20:8.2f} MiB  {t}")
        for f, s in files:
            if f.split("/")[0] in ("AUTO", "STING", "XBOOT") and not f.endswith("/"):
                print(f"    {s:>8}  {f}")
