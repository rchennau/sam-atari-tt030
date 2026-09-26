#!/usr/bin/env python3
"""wbase_build — cross-build a SpareMiNT package from its own spec for one CPU (tt030-t425-kernel-ports FR-8 W-BASE).

    scripts/wbase_build.py SPECDIR CPU [--cache K=V ...] [--cflags X] [--make ARGS] [--sed FILE:EXPR ...] [--dirs "lib src"]
      SPECDIR  tools/rpm-src/x/<name-ver-rel> (the extracted SRPM: spec, tarball, patches)
      CPU      68000 (the stock control) or 68020-60 (the W-CPU build)
    -> /tmp/wc/<name>/b<CPU>/<srcdir>, built with the spec's own %prep patches (in order) and %build configure
       line, with RPM_OPT_FLAGS = -O2 -fomit-frame-pointer and the cross toolchain. --cache pre-seeds autoconf
       results a cross configure cannot run (bash's job-control lesson): write them down, never guess them.
Only the %prep patch list and the first ./configure of %build are taken from the spec; anything else a spec does
(extra make targets, stack --fix) is the caller's to repeat.
"""
import os
import re
import shlex
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CROSS = os.path.join(REPO, "tools/cross-mint/usr/bin")
OPT = "-O2 -fomit-frame-pointer"


def parse_spec(path):
    s = open(path, encoding="latin-1").read()
    s = re.sub(r"\\\n", " ", s)                               # join continued lines
    macros = {"_prefix": "/usr", "_bindir": "/usr/bin", "_mandir": "/usr/share/man", "_infodir": "/usr/share/info",
              "_sysconfdir": "/etc", "_libdir": "/usr/lib", "_datadir": "/usr/share"}
    for m in re.finditer(r"^%define\s+(\S+)\s+(.*)$", s, re.M):
        macros[m.group(1)] = m.group(2).strip()
    def expand(t):
        for _ in range(3):
            t = re.sub(r"%\{(\w+)\}", lambda m: macros.get(m.group(1), m.group(0)), t)
        return t.replace("${RPM_OPT_FLAGS}", OPT).replace("$RPM_OPT_FLAGS", OPT)
    for tag in ("Name", "Version"):
        macros[tag.lower()] = re.search(rf"^{tag}\s*:\s*(\S+)", s, re.M).group(1)
    s = expand(s)
    patches = {m.group(1) or "0": m.group(2) for m in re.finditer(r"^Patch(\d*)\s*:\s*(\S+)", s, re.M)}
    source0 = re.search(r"^Source0?\s*:\s*(\S+)", s, re.M).group(1).rsplit("/", 1)[-1]
    prep = s[s.index("%prep"):s.index("%build")]
    applied = [(m.group(1) or "0", m.group(2)) for m in re.finditer(r"^%patch(\d*)\s+(.*)$", prep, re.M)]
    build = s[s.index("%build"):s.index("%install")]
    conf = next(l for l in build.splitlines() if "./configure" in l)
    return source0, [(patches[n], args) for n, args in applied], conf


def main():
    a = sys.argv[1:]
    specdir, cpu = os.path.abspath(a[0]), a[1]
    cache, cflags, make, seds, dirs = [], "", "", [], [""]
    i = 2
    while i < len(a):
        if a[i] == "--cache":
            cache.append(a[i + 1])
        elif a[i] == "--cflags":
            cflags = a[i + 1]
        elif a[i] == "--make":
            make = a[i + 1]
        elif a[i] == "--dirs":                                 # build only these subdirs (skip docs/man)
            dirs = a[i + 1].split()
        elif a[i] == "--sed":                                  # FILE:EXPR, applied after the spec's patches
            seds.append(a[i + 1].split(":", 1))
        i += 2
    spec = next(f for f in os.listdir(specdir) if f.endswith(".spec"))
    source0, patches, conf = parse_spec(os.path.join(specdir, spec))
    name = spec[:-5]
    w = f"/tmp/wc/{name}/b{cpu}"
    subprocess.run(["rm", "-rf", w], check=True)
    os.makedirs(w)
    subprocess.run(["tar", "xf", os.path.join(specdir, source0)], cwd=w, check=True)
    src = os.path.join(w, next(d for d in os.listdir(w) if os.path.isdir(os.path.join(w, d))))
    for p, args in patches:
        pl = [x for x in shlex.split(args) if x.startswith("-p")]
        with open(os.path.join(specdir, p), "rb") as f:
            r = subprocess.run(["patch", "-s", "-f", *pl], stdin=f, cwd=src)
        print(f"patch {p} {' '.join(pl)}: {'ok' if r.returncode == 0 else 'FAILED'}")
        if r.returncode:
            sys.exit(1)
    for f, expr in seds:
        subprocess.run(["sed", "-i", expr, f], cwd=src, check=True)
        print(f"sed {f}: {expr}")
    # the spec's env assignments + its configure flags, cross-targeted
    words = shlex.split(conf)
    k = words.index("./configure")
    env = dict(w_.split("=", 1) for w_ in words[:k])
    spec_cflags = env.pop("CFLAGS", OPT)
    for fl in ("LDFLAGS", "CPPFLAGS"):                          # the caller's (a -L/-I sysroot) plus the spec's
        if fl in env and os.environ.get(fl):
            env[fl] = f"{os.environ[fl]} {env[fl]}"
    cc = f"m68k-atari-mint-gcc -m{cpu}"
    env.update(PATH=f"{CROSS}:{os.environ['PATH']}", CC=cc, AR="m68k-atari-mint-ar", RANLIB="m68k-atari-mint-ranlib", CFLAGS=f"{spec_cflags} -fcommon -std=gnu89 {cflags}".strip())
    with open(os.path.join(src, "config.cache"), "w") as f:
        for kv in cache:
            f.write(f"{kv.split('=', 1)[0]}=${{{kv.split('=', 1)[0]}={kv.split('=', 1)[1]}}}\n")
    args = [x for x in words[k + 1:]] + ["--build=i686-pc-linux-gnu", "--host=m68k-atari-mint",
                                          "--cache-file=./config.cache"]
    print("configure", " ".join(args), "| CC=" + cc, "CFLAGS=" + env["CFLAGS"])
    r = subprocess.run(["./configure", *args], cwd=src, env={**os.environ, **env},
                       stdout=open(os.path.join(w, "configure.out"), "w"), stderr=subprocess.STDOUT)
    if r.returncode:
        sys.exit(f"configure failed: {w}/configure.out")
    log = open(os.path.join(w, "make.out"), "w")
    for d in dirs:
        r = subprocess.run(["make", "-j8", *shlex.split(make)], cwd=os.path.join(src, d), env={**os.environ, **env},
                           stdout=log, stderr=subprocess.STDOUT)
        if r.returncode:
            sys.exit(f"make failed ({d or '.'}): {w}/make.out")
    print(f"built {src}")


if __name__ == "__main__":
    main()
