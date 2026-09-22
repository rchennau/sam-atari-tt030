#!/usr/bin/env python3
"""build_recipe.py — cross-build one source-map package into an m68kmint RPM (tt030-rpm-pipeline FR-6).

  build_recipe.py NAME [--out DIR]      -> DIR/NAME-VER-1.m68kmint.rpm  (default build/)

Autotools template: fetch the pinned tarball (cached in tools/src/), check its SHA-256, configure
--host=m68k-atari-mint --prefix=/usr, make, make install DESTDIR=<stage>, strip every m68k binary,
package with rpm_header.write_rpm. Deterministic by construction: SOURCE_DATE_EPOCH, a fixed build
dir name, files sorted, header mtimes 0 (NFR-3 checks the stripped-binary SHA across two builds).
Exit 0 built · 1 failed (the last lines of the failing step are printed) · 2 no recipe for NAME.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.request

import rpm_header as R

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
T = "m68k-atari-mint"
XBIN = os.path.join(REPO, "tools", "cross-mint", "usr", "bin")
SOURCE_MAP = os.path.join(REPO, "recipes", "source-map.json")
M68K_MAGIC = b"\x60\x1a"   # GEMDOS program header (NFR-3: not a host ELF)


def recipes():
    return {k: v for k, v in json.load(open(SOURCE_MAP)).items() if not k.startswith("_")}


def toolchain():
    v = lambda tool: subprocess.run([os.path.join(XBIN, f"{T}-{tool}"), "--version"],  # noqa: E731
                                    capture_output=True, text=True).stdout.split("\n")[0]
    return f"{v('gcc')}; {v('ld')}"


def fetch(r):
    cache = os.path.join(REPO, "tools", "src")
    os.makedirs(cache, exist_ok=True)
    f = os.path.join(cache, r["url"].rsplit("/", 1)[1])
    if not os.path.exists(f):
        with urllib.request.urlopen(r["url"], timeout=120) as u, open(f + ".tmp", "wb") as out:
            shutil.copyfileobj(u, out)
        os.replace(f + ".tmp", f)
    if hashlib.sha256(open(f, "rb").read()).hexdigest() != r["sha256"]:
        raise RuntimeError(f"source SHA-256 mismatch for {f} (pinned {r['sha256']})")
    return f


def run(cmd, cwd, env, log, deadline):
    timeout = max(1.0, deadline - time.monotonic())   # ONE budget for the whole build, not per step
    with open(log, "a") as fh:
        fh.write(f"$ {' '.join(cmd)}\n")
        fh.flush()
        p = subprocess.run(cmd, cwd=cwd, env=env, stdout=fh, stderr=subprocess.STDOUT, timeout=timeout)
    if p.returncode:
        tail = open(log, errors="replace").read().splitlines()[-15:]
        raise RuntimeError(f"`{cmd[0]}` failed (rc {p.returncode}):\n" + "\n".join(tail))


def build(name, out_dir, timeout=600, progress=print):
    deadline = time.monotonic() + timeout
    r = recipes().get(name)
    if r is None:
        raise LookupError(name)
    src = fetch(r)
    # A FIXED path, not mkdtemp: __FILE__ and debug paths land in objects, and a random dir would
    # make two clean builds differ (NFR-3). Safe because ttbuildd runs one build at a time.
    work = os.path.join(tempfile.gettempdir(), f"ttbuild-{name}")
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    try:
        src_dir = os.path.join(work, "src")      # fixed name: paths inside objects stay identical
        stage = os.path.join(work, "stage")
        log = os.path.join(work, "build.log")
        with tarfile.open(src) as t:
            t.extractall(work, filter="data")
        (top,) = [d for d in os.listdir(work) if os.path.isdir(os.path.join(work, d)) and d != "stage"]
        os.rename(os.path.join(work, top), src_dir)
        env = dict(os.environ, PATH=XBIN + os.pathsep + os.environ["PATH"], CC=f"{T}-gcc", LD=f"{T}-ld",
                   AR=f"{T}-ar", RANLIB=f"{T}-ranlib", STRIP=f"{T}-strip", CFLAGS="-m68020-60 -O2",
                   SOURCE_DATE_EPOCH="0", LC_ALL="C")
        progress("configure")
        run(["./configure", f"--host={T}", "--prefix=/usr", *r.get("configure_args", [])], src_dir, env, log, deadline)
        progress("make")
        run(["make", f"-j{os.cpu_count()}"], src_dir, env, log, deadline)
        run(["make", "install", f"DESTDIR={stage}"], src_dir, env, log, deadline)
        progress("package")
        files, bins = [], {}
        for root, dirs, names in os.walk(stage):
            dirs.sort()
            for n in sorted(names):
                p = os.path.join(root, n)
                rel = "/" + os.path.relpath(p, stage)
                if os.path.islink(p):
                    files.append((rel, 0o160777, os.readlink(p).encode()))
                    continue
                if open(p, "rb").read(2) == M68K_MAGIC:
                    run([f"{T}-strip", p], src_dir, env, log, deadline)
                data = open(p, "rb").read()
                if data[:2] == M68K_MAGIC:
                    bins[rel] = hashlib.sha256(data).hexdigest()
                files.append((rel, 0o100000 | (os.stat(p).st_mode & 0o7777), data))
        if not bins:
            raise RuntimeError("no m68k binary in the install tree (host binaries? check CC)")
        os.makedirs(out_dir, exist_ok=True)
        rpm = os.path.join(out_dir, f"{name}-{r['version']}-1.m68kmint.rpm")
        desc = f"{r.get('summary', name)}\nBuilt by sam ttbuildd from {r['url']} (sha256 {r['sha256']}).\n" \
               f"Toolchain: {toolchain()}. CFLAGS -m68020-60 -O2."
        with open(rpm + ".tmp", "wb") as fh:
            fh.write(R.write_rpm(name, r["version"], "1", files, summary=r.get("summary", name),
                                 provides=[name], description=desc))
        os.replace(rpm + ".tmp", rpm)
        return rpm, bins
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main(argv):
    if len(argv) not in (2, 4) or (len(argv) == 4 and argv[2] != "--out"):
        print(__doc__, file=sys.stderr)
        return 2
    try:
        rpm, bins = build(argv[1], argv[3] if len(argv) == 4 else os.path.join(REPO, "build"))
    except LookupError:
        print(f"build_recipe: no recipe for {argv[1]}", file=sys.stderr)
        return 2
    except (RuntimeError, subprocess.TimeoutExpired) as e:
        print(f"build_recipe: {argv[1]} FAILED: {e}", file=sys.stderr)
        return 1
    print(rpm)
    for path, sha in bins.items():
        print(f"  {path} {sha}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
