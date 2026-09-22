#!/usr/bin/env python3
"""rpm_va_classify.py — classify every line of a TT `rpm -Va` (tt030-rpm-pipeline FR-5 acceptance).

  rpm_va_classify.py RPM_VA_OUTPUT        (fetch it with: ssh atari-tt 'rpm -Va' > out; ~16 min on the 68030)

Each line becomes EXPECTED (with the rule that explains it) or REAL. Exit 0 no REAL · 1 any REAL.
Unsatisfied dependencies are reported separately as DEPGAP: FR-5 registers with --nodeps and the
missing providers arrive later through yum, so they are gaps to list, not file drift.
"""
import os
import re
import sys

OVERLAY = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "staging", "HD10_OVERLAY")

# (name, predicate(flags, is_config, path), reason). First match wins; anything unmatched is REAL.
RULES = [
    ("group-wheel", lambda f, c, p: f == "......G.",
     "SpareMiNT packages own files as group `wheel`, which this card has no entry for: rpm fell back to "
     "root ('group wheel does not exist - using root' on every install). Group only, content intact."),
    ("overlay-config", lambda f, c, p: c and os.path.isfile(OVERLAY + p),
     "config file the card build replaces from staging/HD10_OVERLAY (e.g. inetd.conf, byte-identical "
     "to the overlay copy, 2026-09-21)."),
    ("mtime-only", lambda f, c, p: f == ".......T",
     "content, size, mode and owner match; only the timestamp differs (card built by copy, not rpm)."),
]


def classify(text):
    out = []
    for line in text.splitlines():
        if not line.strip() or re.fullmatch(r"\d+", line.strip()):
            continue
        m = re.match(r"^Unsatisfied dependencies for (\S+): (.*)$", line)
        if m:
            out.append(("DEPGAP", m.group(1), m.group(2).strip(), ""))
            continue
        m = re.match(r"^(missing|[.SM5DLUGT?]{8})\s+(c\s+)?(/\S*)$", line)
        if not m:
            out.append(("REAL", "unparsed", line, "line not understood; treat as a mismatch"))
            continue
        flags, is_cfg, path = m.group(1), bool(m.group(2)), m.group(3)
        for name, pred, why in RULES:
            if flags != "missing" and pred(flags, is_cfg, path):
                out.append(("EXPECTED", name, f"{flags} {path}", why))
                break
        else:
            out.append(("REAL", flags, path, "file on record does not match the disk"))
    return out


def main(argv):
    if len(argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    rows = classify(open(argv[1], errors="replace").read())
    counts = {}
    for verdict, rule, what, _ in rows:
        counts[(verdict, rule)] = counts.get((verdict, rule), 0) + 1
    for verdict in ("REAL", "DEPGAP"):
        for v, rule, what, why in rows:
            if v == verdict:
                print(f"{v:8} {rule:14} {what}")
    for (v, rule), n in sorted(counts.items()):
        print(f"summary  {v:8} {rule:14} {n}")
    return 1 if any(r[0] == "REAL" for r in rows) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
