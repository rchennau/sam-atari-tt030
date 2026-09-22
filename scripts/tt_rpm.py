#!/usr/bin/env python3
"""tt_rpm.py — host CLI for the TT030 package mirror (tt030-rpm-pipeline FR-7).

  tt_rpm.py keygen          make the index signing key (prints the public key)
  tt_rpm.py sync MIRROR      mirror freemint/sparemint RPMS/{m68kmint,noarch}, then index
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


# RSA, not Ed25519: the TT's openssl 1.0.0c has no Ed25519 (measured 2026-09-21). The private key
# never leaves fractal; the public half ships in the yum-tt RPM as /etc/yum.tt/index.pub.
SIGN_KEY = os.path.expanduser("~/.config/tt030-mirror/index-sign.key")


def cmd_keygen():
    import subprocess
    if os.path.exists(SIGN_KEY):
        print(f"keygen: {SIGN_KEY} exists, refusing to replace it", file=sys.stderr)
        return 1
    os.makedirs(os.path.dirname(SIGN_KEY), mode=0o700, exist_ok=True)
    old = os.umask(0o077)
    try:
        subprocess.run(["openssl", "genrsa", "-out", SIGN_KEY, "2048"], check=True, capture_output=True)
    finally:
        os.umask(old)
    pub = subprocess.run(["openssl", "rsa", "-in", SIGN_KEY, "-pubout"], check=True,
                         capture_output=True, text=True).stdout
    sys.stdout.write(pub)
    return 0


def cmd_index(mirror):
    import subprocess
    pkgs, bad = scan(mirror)
    for b in bad:
        print(f"REJECTED {b}", file=sys.stderr)
    if bad:
        print(f"index NOT written: {len(bad)} malformed RPM(s)", file=sys.stderr)
        return 1
    if not os.path.exists(SIGN_KEY):
        print(f"index NOT written: no signing key {SIGN_KEY} (tt_rpm.py keygen)", file=sys.stderr)
        return 1
    out = os.path.join(mirror, "index.tsv")
    with open(out + ".tmp", "w") as fh:
        fh.write(build_index(pkgs))
    subprocess.run(["openssl", "dgst", "-sha256", "-sign", SIGN_KEY, "-out", out + ".sig.tmp", out + ".tmp"],
                   check=True)
    # ponytail: two renames, not atomic as a pair; a client racing them fails verify and re-runs.
    os.replace(out + ".sig.tmp", out + ".sig")
    os.replace(out + ".tmp", out)
    print(f"{out}: {len(pkgs)} RPMs scanned, signed")
    return 0


TREE_URL = "https://api.github.com/repos/freemint/sparemint/git/trees/master?recursive=1"
RAW_URL = "https://raw.githubusercontent.com/freemint/sparemint/master/"
# base-29 NVRs build_hd10_ext2.sh unpacked: sync never prunes them (FR-5 rpm -Va needs them)
PINNED_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "staging", "SPAREMINT")


def git_blob_sha1(data):
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def sync(mirror, entries, fetch, pinned):
    """Mirror upstream RPMs into m68kmint/ and noarch/. entries: git tree entries.
    Verifies each download against its git blob sha; never touches built/; prunes an upstream
    file only if upstream dropped it and it is not pinned. Returns (added, pruned, bad)."""
    want, added, bad = {}, 0, []
    for e in entries:
        parts = e["path"].split("/")
        if len(parts) == 4 and parts[:2] == ["sparemint", "RPMS"] and parts[2] in ("m68kmint", "noarch") \
                and parts[3].endswith(".rpm"):
            want[(parts[2], parts[3])] = e
    for (pool, name), e in sorted(want.items()):
        dest = os.path.join(mirror, pool, name)
        if os.path.exists(dest) and git_blob_sha1(open(dest, "rb").read()) == e["sha"]:
            continue
        data = fetch(RAW_URL + e["path"])
        if git_blob_sha1(data) != e["sha"]:
            bad.append(f"{pool}/{name}: git blob sha mismatch")
            continue
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest + ".tmp", "wb") as fh:
            fh.write(data)
        os.replace(dest + ".tmp", dest)
        added += 1
    pruned = 0
    for pool in ("m68kmint", "noarch"):
        d = os.path.join(mirror, pool)
        for name in sorted(os.listdir(d)) if os.path.isdir(d) else []:
            if name.endswith(".rpm") and (pool, name) not in want and name not in pinned:
                os.remove(os.path.join(d, name))
                pruned += 1
    return added, pruned, bad


def cmd_sync(mirror):
    import json
    import urllib.request

    def fetch(url):
        with urllib.request.urlopen(url, timeout=120) as r:
            return r.read()
    tree = json.loads(fetch(TREE_URL))
    if tree.get("truncated"):
        print("sync: git tree listing truncated, refusing a partial sync", file=sys.stderr)
        return 1
    pinned = {f for f in os.listdir(PINNED_DIR) if f.endswith(".rpm")}
    added, pruned, bad = sync(mirror, tree["tree"], fetch, pinned)
    for b in bad:
        print(f"REJECTED {b}", file=sys.stderr)
    print(f"sync: {added} added, {pruned} pruned, {len(bad)} rejected")
    return cmd_index(mirror) or (1 if bad else 0)


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
    if len(argv) == 2 and argv[1] == "keygen":
        return cmd_keygen()
    if len(argv) == 3 and argv[1] == "sync":
        return cmd_sync(argv[2])
    if len(argv) == 3 and argv[1] == "synth":
        open(argv[2], "wb").write(synth_gate_rpm())
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
