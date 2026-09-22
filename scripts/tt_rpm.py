#!/usr/bin/env python3
"""tt_rpm.py — host CLI for the TT030 package mirror (tt030-rpm-pipeline FR-7).

  tt_rpm.py keygen          make the index signing key (prints the public key)
  tt_rpm.py sync MIRROR      mirror freemint/sparemint RPMS/{m68kmint,noarch}, then index
  tt_rpm.py add MIRROR RPM  publish RPM into MIRROR/built/ and re-index
  tt_rpm.py index MIRROR     rebuild MIRROR/index.tsv from MIRROR/{m68kmint,noarch,built}/*.rpm
  tt_rpm.py push MIRROR PKG... --host H [--nodeps]   install on a TT that cannot reach the mirror
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


def publish(tmp, dest):
    """Atomic replace with an explicit world-readable mode: Caddy serves these files, and a
    caller's umask 077 once left index.signed 0600 and the mirror unreadable (2026-09-21)."""
    os.chmod(tmp, 0o644)
    os.replace(tmp, dest)


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
    """Parse every pool RPM. Records are cached in .scan-cache.json keyed on (size, mtime_ns): a cold
    re-read of the 426 MB mirror over the NAS link took minutes on every reindex (2026-09-21)."""
    import json
    cache_path = os.path.join(mirror, ".scan-cache.json")
    try:
        cache = json.load(open(cache_path))
    except (OSError, ValueError):
        cache = {}
    pkgs, bad, fresh = [], [], {}
    for pool in POOLS:
        d = os.path.join(mirror, pool)
        for f in sorted(os.listdir(d)) if os.path.isdir(d) else []:
            if not f.endswith(".rpm"):
                continue
            st = os.stat(os.path.join(d, f))
            key, stamp = f"{pool}/{f}", [st.st_size, st.st_mtime_ns]
            hit = cache.get(key)
            if hit and hit["stamp"] == stamp:
                pkgs.append(hit["pkg"])
                fresh[key] = hit
                continue
            data = open(os.path.join(d, f), "rb").read()
            try:
                _, m, _ = R.rpm_header(data)
            except R.BadRpm as e:
                bad.append(f"{pool}/{f}: {e}")
                continue
            pkg = dict(name=m[R.NAME], version=m[R.VERSION], release=m[R.RELEASE],
                       arch=m.get(R.ARCH) or "noarch", pool=pool, path=f"{pool}/{f}",
                       size=len(data), sha256=hashlib.sha256(data).hexdigest(),
                       requires=R.requires(m), provides=m.get(R.PROVIDENAME, []),
                       files=R.file_paths(m))
            pkgs.append(pkg)
            fresh[key] = {"stamp": stamp, "pkg": pkg}
    with open(cache_path + ".tmp", "w") as fh:
        json.dump(fresh, fh)
    publish(cache_path + ".tmp", cache_path)
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


def serial_for(mirror):
    """Strictly greater than the serial already published (clock steps backwards are survived)."""
    import time
    try:
        with open(os.path.join(mirror, "index.tsv")) as fh:
            first = fh.readline().split("\t")
        prev = int(first[1]) if first[0] == "#serial" else 0
    except (OSError, ValueError, IndexError):
        prev = 0
    return max(int(time.time()), prev + 1)


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
    # NFR-4: ONE file, index.signed = 256-byte RSA signature + index, replaced atomically, so a GET
    # can never pair a new index with an old signature. The first line carries a monotonic serial
    # the client compares with the last one it accepted (replay of an older signed index).
    body = f"#serial\t{serial_for(mirror)}\n{build_index(pkgs)}".encode()
    sig = subprocess.run(["openssl", "dgst", "-sha256", "-sign", SIGN_KEY], input=body,
                         capture_output=True, check=True).stdout
    assert len(sig) == 256, "index-sign.key must be RSA-2048 (client splits at 256 bytes)"
    for name, data in (("index.signed", sig + body), ("index.tsv", body)):   # index.tsv: human copy
        with open(os.path.join(mirror, name + ".tmp"), "wb") as fh:
            fh.write(data)
        publish(os.path.join(mirror, name + ".tmp"), os.path.join(mirror, name))
    stale = os.path.join(mirror, "index.tsv.sig")
    if os.path.exists(stale):
        os.remove(stale)
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
        publish(dest + ".tmp", dest)
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


def cmd_add(mirror, rpm):
    """Publish a built/obtained RPM into built/ (never pruned by sync) and re-index."""
    import shutil
    data = open(rpm, "rb").read()
    try:
        R.rpm_header(data)
    except R.BadRpm as e:
        print(f"add: {rpm}: {e}", file=sys.stderr)
        return 1
    os.makedirs(os.path.join(mirror, "built"), exist_ok=True)
    dest = os.path.join(mirror, "built", os.path.basename(rpm))
    shutil.copyfile(rpm, dest + ".tmp")
    publish(dest + ".tmp", dest)
    return cmd_index(mirror)


def read_index(mirror):
    with open(os.path.join(mirror, "index.tsv")) as fh:
        return {r[0]: r for r in (line.rstrip("\n").split("\t") for line in fh if not line.startswith("#"))}


def ssh_run(host, cmd, stdin=None):
    import subprocess
    p = subprocess.run(["ssh", host, cmd], input=stdin, capture_output=True, timeout=600)
    return p.returncode, p.stdout.decode(errors="replace") + p.stderr.decode(errors="replace")


def cmd_push(mirror, pkgs, host, nodeps=False, run=ssh_run):
    """FR-7: install PKGS on a TT that cannot reach the mirror. Each RPM goes over ssh (binary-safe,
    unlike the FTP path this replaced), its SHA-256 is re-checked ON THE TT against the signed index,
    and only then does one plain `rpm -i` run. No dependency walk: name what to push; rpm reports
    anything missing. UNIXMODE is set explicitly (mintlib text mode would corrupt the digest)."""
    index = read_index(mirror)
    missing = [p for p in pkgs if p not in index]
    if missing:
        print(f"push: not on the mirror: {' '.join(missing)}", file=sys.stderr)
        return 1
    remote = []
    for p in pkgs:
        path, sha = index[p][4], index[p][6]
        dest = "/tmp/push-" + os.path.basename(path)
        data = open(os.path.join(mirror, path), "rb").read()
        rc, out = run(host, f"cat > {dest} && UNIXMODE=/brUs openssl dgst -sha256 {dest}", stdin=data)
        got = out.strip().rsplit("= ", 1)[-1] if rc == 0 else ""
        if got != sha:
            print(f"push: {p}: SHA-256 on the TT is {got or '(transfer failed)'}, index says {sha}; not installed",
                  file=sys.stderr)
            run(host, f"rm -f {' '.join(remote + [dest])}")
            return 1
        print(f"push: {p} transferred, SHA-256 verified on the TT")
        remote.append(dest)
    rc, out = run(host, f"UNIXMODE=/brUs rpm -i {'--nodeps ' if nodeps else ''}{' '.join(remote)}; r=$?; "
                        f"rm -f {' '.join(remote)}; exit $r")
    print(out.strip())
    if rc:
        print(f"push: rpm -i failed (rc {rc})", file=sys.stderr)
        return 1
    print(f"push: installed {' '.join(pkgs)}")
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
    if len(argv) == 2 and argv[1] == "keygen":
        return cmd_keygen()
    if len(argv) == 3 and argv[1] == "sync":
        return cmd_sync(argv[2])
    if len(argv) == 4 and argv[1] == "add":
        return cmd_add(argv[2], argv[3])
    if len(argv) >= 5 and argv[1] == "push" and "--host" in argv:
        a = argv[3:]
        host = a[a.index("--host") + 1]
        pkgs = [x for i, x in enumerate(a) if x not in ("--host", "--nodeps") and a[i - 1] != "--host"]
        return cmd_push(argv[2], pkgs, host, nodeps="--nodeps" in a)
    if len(argv) == 3 and argv[1] == "synth":
        open(argv[2], "wb").write(synth_gate_rpm())
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
