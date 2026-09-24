#!/usr/bin/env python3
"""t425_inventory.py — FR-1 of tt030-t425-kernel-ports: classify every mirror package for T425 porting.

  t425_inventory.py facts MIRROR OUT.tsv       per package: header facts (group, summary, commands,
                                               requires, consumers) from index.tsv + each RPM header
  t425_inventory.py classify FACTS.tsv OUT.tsv apply the rules below -> inventory.tsv

Classes (plan FR-1): KERNEL = the package's main work is `buffer in -> buffer out` with no file,
terminal or network call inside the loop; IO/INTERACTIVE; DATA/DOCS. The rules are explicit and
ordered, every row carries the rule that decided it, and anything the rules cannot decide is
BORDERLINE, to be settled by measurement (t4bench), never by judgement alone.
"""
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import rpm_header as R  # noqa: E402

# Families and their packages (plan Eng §1). A package here is KERNEL unless a later rule says it
# is only headers/docs (-devel, -doc). Libraries are KERNEL; their consumers are listed separately.
FAMILIES = {
    "deflate": ["gzip", "zip", "unzip", "zlib", "libpng", "rsync"],
    "bwt": ["bzip2"],
    "lz": ["lzip", "lzo", "lha", "unarj", "xz", "arc"],
    "digest-cipher": ["openssl", "gnupg", "textutils", "sharutils"],
    "bignum": ["gmp", "mpfr", "mpc", "bc"],
    "image": ["libjpeg", "libtiff", "libungif", "libungif-progs", "giftrans", "netpbm-progs",
              "ImageMagick", "ghostscript", "ghostscript-68000"],
    "audio": ["libogg", "libvorbis", "sox", "gsm", "libmad"],
}
FAMILY_OF = {p: f for f, ps in FAMILIES.items() for p in ps}
# Already on the T425 (t425-compress): recorded, not re-ported.
DONE = {"t425-compress"}

# Rows the generic rules get wrong, each with its reason (reviewed 2026-09-24 against the summaries).
IO = "IO/INTERACTIVE"
OVERRIDES = {
    **{n: (IO, "", "text/markup transform, I/O-bound per byte") for n in (
        "dos2unix", "unix2dos", "info2html", "texi2html", "hypermail", "antiword", "wv", "abcMIDI",
        "uudeview", "units", "libwmf", "audiofile")},
    "logrotate": (IO, "", "rotates logs; compression is done by calling gzip (a KERNEL row)"),
    **{n: (IO, "", "interactive X program; its codec work is in libjpeg/libpng/zlib (consumer)") for n in (
        "xv", "xpaint")},
    "bmeps": ("BORDERLINE", "image", "bitmap -> EPS: deflate/LZW/ASCII85 per image, measure with t4bench"),
    "mikmod": ("BORDERLINE", "audio", "module mixing: integer or float per build, measure"),
    "freetype": ("BORDERLINE", "image", "glyph rasterizer: compute-bound but small calls, measure the small-call ratio"),
    "tetex": ("BORDERLINE", "", "TeX engine: CPU-bound but large working set and file I/O in the loop"),
    "tetex-dvips": (IO, "", "DVI -> PostScript: I/O-bound"),
    "fdlibm": ("BORDERLINE", "", "float maths library: soft-float on the T425 (NFR-5), likely DECLINED"),
    "pml": ("BORDERLINE", "", "float maths library: soft-float on the T425 (NFR-5), likely DECLINED"),
    "pml-mshort": ("DATA/DOCS", "", "obsolete 16-bit int build of pml"),
    "gnubg": ("BORDERLINE", "", "backgammon neural-net evaluation: float (NFR-5), measure"),
    "xpdf": ("BORDERLINE", "image", "PDF render: inflate + DCT per page inside an X viewer, measure"),
}

DATA_GROUPS = ("Documentation", "Fonts", "Development/Libraries/Headers")
DATA_SUFFIXES = ("-devel", "-doc", "-docs", "-fonts", "-afm", "-data", "-static")


def facts(mirror, out):
    idx = os.path.join(mirror, "index.tsv")
    raw = open(idx, "rb").read()
    rows = [line.split("\t") for line in raw.decode().splitlines() if line and not line.startswith("#")]
    provides = {}
    for r in rows:
        for p in r[8].split(","):
            provides.setdefault(p, r[0])
    consumers = {}
    for r in rows:
        for q in r[7].split(","):
            prov = provides.get(q)
            if prov and prov != r[0]:
                consumers.setdefault(prov, set()).add(r[0])
    with open(out, "w") as fh:
        fh.write(f"#snapshot\t{idx}\tsha256={hashlib.sha256(raw).hexdigest()}\trows={len(rows)}\n")
        fh.write("package\tversion\tgroup\tsummary\tcmds\trequires\tconsumers\n")
        for r in rows:
            with open(os.path.join(mirror, r[4]), "rb") as f:
                data = f.read(512 * 1024)          # the header is at the front; payload not needed
            try:
                _, m, _ = R.rpm_header(data)
            except R.BadRpm:
                _, m, _ = R.rpm_header(open(os.path.join(mirror, r[4]), "rb").read())
            clean = lambda v: (v[0] if isinstance(v, list) else v or "").replace("\t", " ").replace("\n", " ")  # noqa: E731
            fh.write("\t".join([r[0], f"{r[1]}-{r[2]}", clean(m.get(R.GROUP)), clean(m.get(R.SUMMARY)),
                                r[9] if len(r) > 9 else "", r[7], ",".join(sorted(consumers.get(r[0], ())))]) + "\n")
    return 0


def rule(p):
    """-> (class, family, rule text). First matching rule wins; the text is the row's reason."""
    name, group, summary = p["package"], p["group"], p["summary"]
    if name in OVERRIDES:
        return OVERRIDES[name]
    if name in DONE:
        return "KERNEL", "deflate+bwt", "already on the T425 (t425-compress)"
    if name.endswith(DATA_SUFFIXES) or group.startswith(DATA_GROUPS):
        return "DATA/DOCS", "", f"headers/docs/fonts ({group or name})"
    if name in FAMILY_OF:
        return "KERNEL", FAMILY_OF[name], f"seed list, family {FAMILY_OF[name]}: {summary}"
    if not p["cmds"] and not group.startswith(("System Environment/Libraries", "Development/Libraries")):
        return "DATA/DOCS", "", f"no commands and not a library ({group})"
    words = (summary + " " + group).lower()
    if any(k in words for k in ("compress", "encrypt", "checksum", "codec", "encod", "arbitrary precision",
                                "image", "audio", "sound", "convert")):
        return "BORDERLINE", "", f"summary names compute-like work, not in a family: {summary}"
    return "IO/INTERACTIVE", "", f"{group or 'no group'}: {summary}"


def classify(facts_path, out):
    lines = open(facts_path).read().splitlines()
    snap = lines[0]
    head = lines[1].split("\t")
    rows = [dict(zip(head, ln.split("\t"))) for ln in lines[2:]]
    counts = {}
    with open(out, "w") as fh:
        fh.write(snap + "\n")
        fh.write("package\tclass\tfamily\treason\tconsumers\trep_input\trep_68030_ms\trep_t425_ms\trep_ratio"
                 "\tsmall_ratio\tthreshold\ticc_portable\tdecision\n")
        for p in rows:
            cls, fam, why = rule(p)
            counts[cls] = counts.get(cls, 0) + 1
            cons = p["consumers"] if cls == "KERNEL" else ""
            fh.write("\t".join([p["package"], cls, fam, why, cons] + [""] * 6
                               + ["pending" if cls == "KERNEL" else "", ""]) + "\n")
    print(" · ".join(f"{k} {v}" for k, v in sorted(counts.items())), f"(total {len(rows)})")
    return 0


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    if cmd == "facts" and len(sys.argv) == 4:
        sys.exit(facts(sys.argv[2], sys.argv[3]))
    if cmd == "classify" and len(sys.argv) == 4:
        sys.exit(classify(sys.argv[2], sys.argv[3]))
    print(__doc__, file=sys.stderr)
    sys.exit(2)
