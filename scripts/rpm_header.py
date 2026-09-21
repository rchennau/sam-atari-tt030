#!/usr/bin/env python3
"""Dependency-free RPM header reader and v3 RPM writer (tt030-rpm-pipeline FR-2, FR-6).

fractal has no `rpm` binary, so the index is built from the bytes: 96-byte lead, signature
header (padded to 8), main header, then the gzip cpio payload. Tag numbers are rpm's own.
"""
import gzip
import hashlib
import struct

LEAD_MAGIC = b"\xed\xab\xee\xdb"
HDR_MAGIC = b"\x8e\xad\xe8\x01"

NAME, VERSION, RELEASE = 1000, 1001, 1002
SIZE, OS, ARCH = 1009, 1021, 1022
FILENAMES, FILESIZES, FILEMODES = 1027, 1028, 1030
FILERDEVS, FILEMTIMES, FILEMD5S, FILELINKTOS = 1033, 1034, 1035, 1036
FILEFLAGS, FILEUSERNAME, FILEGROUPNAME = 1037, 1039, 1040
PROVIDENAME, REQUIREFLAGS, REQUIRENAME, REQUIREVERSION = 1047, 1048, 1049, 1050
DIRINDEXES, BASENAMES, DIRNAMES = 1116, 1117, 1118
SUMMARY, DESCRIPTION, GROUP = 1004, 1005, 1016
SIG_SIZE, SIG_MD5 = 1000, 1004
I18NTABLE, RPMVERSION = 100, 1064

T_INT16, T_INT32, T_STRING, T_BIN, T_STRING_ARRAY, T_I18N = 3, 4, 6, 7, 8, 9


class BadRpm(ValueError):
    pass


def _read_header(data, off):
    """Parse one header structure at `off`; return (tags, end_offset)."""
    if data[off:off + 4] != HDR_MAGIC:
        raise BadRpm(f"no header magic at {off}")
    nidx, hsize = struct.unpack(">II", data[off + 8:off + 16])
    store = off + 16 + nidx * 16
    end = store + hsize
    if nidx > 10000 or end > len(data):
        raise BadRpm("header index runs past end of file")
    tags = {}
    for i in range(nidx):
        tag, typ, o, cnt = struct.unpack(">IIII", data[off + 16 + i * 16:off + 32 + i * 16])
        p = store + o
        if p > end:
            raise BadRpm(f"tag {tag} offset past store")
        if typ in (T_STRING, T_STRING_ARRAY, T_I18N):
            vals = []
            for _ in range(1 if typ == T_STRING else cnt):
                z = data.index(b"\0", p, end)
                vals.append(data[p:z].decode("latin1"))
                p = z + 1
            tags[tag] = vals[0] if typ == T_STRING else vals
        elif typ == T_INT32:
            tags[tag] = list(struct.unpack(f">{cnt}i", data[p:p + 4 * cnt]))
        elif typ == T_INT16:
            tags[tag] = list(struct.unpack(f">{cnt}H", data[p:p + 2 * cnt]))
        elif typ == T_BIN:
            tags[tag] = data[p:p + cnt]
        else:  # char/int8/int64: not needed by the index
            tags[tag] = None
    return tags, end


def rpm_header(data):
    """Return (signature_tags, main_tags, payload_offset). Raises BadRpm on anything malformed."""
    if len(data) < 96 or data[:4] != LEAD_MAGIC:
        raise BadRpm("no RPM lead magic")
    try:
        sig, end = _read_header(data, 96)
        main, end = _read_header(data, 96 + ((end - 96 + 7) & ~7))
    except (struct.error, ValueError) as e:
        raise BadRpm(str(e)) from e
    for t in (NAME, VERSION, RELEASE):
        if not isinstance(main.get(t), str):
            raise BadRpm(f"missing tag {t}")
    return sig, main, end


def file_paths(main):
    if BASENAMES in main:
        dirs = main[DIRNAMES]
        return [dirs[i] + b for i, b in zip(main[DIRINDEXES], main[BASENAMES])]
    return list(main.get(FILENAMES, []))


def requires(main):
    return [r for r in main.get(REQUIRENAME, []) if not r.startswith("rpmlib(")]


# ---- writer --------------------------------------------------------------------------

def _header(entries):
    """entries: list of (tag, type, value) -> header bytes. Tags must be sorted ascending."""
    index, store = b"", b""
    for tag, typ, val in sorted(entries):
        align = {T_INT32: 4, T_INT16: 2}.get(typ, 1)
        store += b"\0" * (-len(store) % align)
        if typ == T_STRING:
            blob, cnt = val.encode("latin1") + b"\0", 1
        elif typ in (T_STRING_ARRAY, T_I18N):
            blob, cnt = b"".join(v.encode("latin1") + b"\0" for v in val), len(val)
        elif typ == T_INT32:
            blob, cnt = struct.pack(f">{len(val)}i", *val), len(val)
        elif typ == T_INT16:
            blob, cnt = struct.pack(f">{len(val)}H", *val), len(val)
        else:
            blob, cnt = val, len(val)
        index += struct.pack(">IIII", tag, typ, len(store), cnt)
        store += blob
    return HDR_MAGIC + b"\0" * 4 + struct.pack(">II", len(entries), len(store)) + index + store


def _cpio(files):
    """newc cpio. files: list of (path, mode, body); mode carries the file-type bits."""
    out = b""
    for ino, (path, mode, body) in enumerate(files + [("TRAILER!!!", 0, b"")], 1):
        name = path.lstrip("/").encode() + b"\0"  # no "./": SpareMiNT payloads use bin/gzip
        hdr = "070701" + "".join(f"{v:08x}" for v in (
            ino, mode, 0, 0, 1, 0, len(body), 0, 0, 0, 0, len(name), 0))
        out += hdr.encode() + name
        out += b"\0" * (-len(out) % 4) + body
        out += b"\0" * (-len(out) % 4)
    return out


def write_rpm(name, version, release, files, summary="", requires_=(), provides=()):
    """Build a v3 m68kmint binary RPM. files: list of (abs_path, mode, body).

    Symlinks are passed with MiNT's S_IFLNK 0160000 (what SpareMiNT's own payloads use,
    see cpio_symlinks.py) and body = link target. Old-style FILENAMES, no BASENAMES:
    rpm 3.0.6 reads both, and the flat list is the conservative choice until the TT gate.
    """
    files = sorted(files)
    is_link = [(m & 0o170000) == 0o160000 for _, m, _ in files]
    reg = [(m & 0o170000) == 0o100000 for _, m, _ in files]
    main = [
        (I18NTABLE, T_STRING_ARRAY, ["C"]),
        (NAME, T_STRING, name), (VERSION, T_STRING, version), (RELEASE, T_STRING, release),
        (SUMMARY, T_I18N, [summary or name]), (DESCRIPTION, T_I18N, [summary or name]),
        (SIZE, T_INT32, [sum(len(b) for _, _, b in files)]),
        (GROUP, T_I18N, ["sam/built"]), (OS, T_STRING, "mint"), (ARCH, T_STRING, "m68kmint"),
        (FILENAMES, T_STRING_ARRAY, [p for p, _, _ in files]),
        (FILESIZES, T_INT32, [len(b) for _, _, b in files]),
        (FILEMODES, T_INT16, [m for _, m, _ in files]),
        (FILERDEVS, T_INT16, [0] * len(files)),
        (FILEMTIMES, T_INT32, [0] * len(files)),
        (FILEMD5S, T_STRING_ARRAY,
         [hashlib.md5(b).hexdigest() if r else "" for (_, _, b), r in zip(files, reg)]),
        (FILELINKTOS, T_STRING_ARRAY,
         [b.decode("latin1") if lk else "" for (_, _, b), lk in zip(files, is_link)]),
        (FILEFLAGS, T_INT32, [0] * len(files)),
        (FILEUSERNAME, T_STRING_ARRAY, ["root"] * len(files)),
        (FILEGROUPNAME, T_STRING_ARRAY, ["root"] * len(files)),
        # without RPMVERSION, rpm 3.0.6 on big-endian verifies with its "broken MD5" and
        # `rpm -V` flags every file (lib/verify.c; TT 2026-09-21)
        (RPMVERSION, T_STRING, "3.0.6"),
    ]
    if provides:
        main.append((PROVIDENAME, T_STRING_ARRAY, list(provides)))
    if requires_:
        main += [(REQUIREFLAGS, T_INT32, [0] * len(requires_)),
                 (REQUIRENAME, T_STRING_ARRAY, list(requires_)),
                 (REQUIREVERSION, T_STRING_ARRAY, [""] * len(requires_))]
    hdr = _header(main)
    payload = gzip.compress(_cpio(files), mtime=0)
    sig = _header([(SIG_SIZE, T_INT32, [len(hdr) + len(payload)]),
                   (SIG_MD5, T_BIN, hashlib.md5(hdr + payload).digest())])
    sig += b"\0" * (-len(sig) % 8)
    nm = f"{name}-{version}-{release}".encode()[:65]
    # lead as SpareMiNT's own packages write it: v3.0, binary, archnum 13, osnum 255, sigtype 5
    lead = LEAD_MAGIC + bytes([3, 0]) + struct.pack(">HH", 0, 13) + nm.ljust(66, b"\0")
    lead += struct.pack(">HH", 255, 5) + b"\0" * 16
    return lead + sig + hdr + payload
