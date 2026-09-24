#!/usr/bin/env python3
"""t425_icc_trial.py — FR-1 `icc_portable` column of tt030-t425-kernel-ports.

  t425_icc_trial.py SRCROOT OUT.tsv [PKG...]

For each KERNEL package, compile its named compute files (KERNEL_FILES, globs under the unpacked SRPM
in SRCROOT/<pkg>-<ver>/) with the INMOS icc under t4, one file at a time, in a short work dir holding
every header of the package flattened plus an empty config.h. Each file gets the first error class:
  ok       compiled
  64bit    `long long` / 64-bit literal / `inttypes`-style 64-bit types (icc is 32-bit only)
  header   a system header icc lacks (sys/*.h, unistd.h, ...) — patchable with a stub
  other    anything else (first error line kept)
Package verdict: yes (all ok) · patchable (only header) · partial (some files 64bit, others ok) ·
no (64bit and nothing else compiles, or C++ only) · check (other).
This is a TRIAL: it says whether the kernel C is within icc's reach, not that a port works.
"""
import glob
import os
import re
import shutil
import subprocess
import sys

R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
WORK = "/tmp/it"                                  # short: icc/ilink ISEARCH breaks past ~100 chars

KERNEL_FILES = {
    "arc": ["arclzw.c", "arcsq.c", "arcusq.c", "arccode.c"],
    "bc": ["*/lib/number.c", "lib/number.c"],
    "bzip2": ["*/blocksort.c", "*/compress.c", "*/huffman.c", "*/crctable.c"],
    "gzip": ["*/deflate.c", "*/trees.c", "*/bits.c", "*/inflate.c", "*/util.c"],
    "zlib": ["*/deflate.c", "*/trees.c", "*/inflate.c", "*/adler32.c", "*/crc32.c"],
    "zip": ["*/deflate.c", "*/trees.c", "*/crc32.c", "*/crypt.c"],
    "unzip": ["*/inflate.c", "*/explode.c", "*/unshrink.c", "*/unreduce.c", "*/crc32.c"],
    "rsync": ["*/checksum.c", "*/lib/mdfour.c"],
    "lha": ["*/src/slide.c", "*/src/huf.c", "*/src/maketbl.c", "*/src/crcio.c"],
    "unarj": ["decode.c", "*/decode.c"],
    "lzo": ["*/src/lzo1x_1.c", "*/src/lzo1x_d1.c", "*/src/lzo1x_9x.c", "*/src/lzo_crc.c"],
    "openssl": ["*/crypto/sha/sha256.c", "*/crypto/sha/sha1dgst.c", "*/crypto/md5/md5_dgst.c",
                "*/crypto/aes/aes_core.c", "*/crypto/bn/bn_mul.c", "*/crypto/bn/bn_asm.c",
                "*/crypto/sha/sha512.c"],
    "gnupg": ["*/cipher/sha1.c", "*/cipher/rmd160.c", "*/cipher/blowfish.c", "*/cipher/cast5.c",
              "*/mpi/mpih-mul.c", "*/mpi/generic/mpih-mul1.c"],
    "textutils": ["*/lib/md5.c", "*/src/sum.c", "*/src/cksum.c"],
    "sharutils": ["*/src/uuencode.c", "*/lib/md5.c"],
    "gmp": ["*/mpn/generic/mul_1.c", "*/mpn/generic/addmul_1.c", "*/mpn/generic/mul_basecase.c",
            "*/mpn/generic/divrem_1.c"],
    "mpfr": ["*/mul.c", "*/add1.c", "*/sqrt.c"],
    "mpc": ["*/src/mul.c", "*/src/add.c"],
    "libjpeg": ["*/jfdctint.c", "*/jidctint.c", "*/jchuff.c", "*/jdhuff.c", "*/jcdctmgr.c", "*/jquant2.c"],
    "libpng": ["*/pngwutil.c", "*/pngrutil.c"],
    "libtiff": ["*/libtiff/tif_lzw.c", "*/libtiff/tif_fax3.c", "*/libtiff/tif_packbits.c",
                "*/libtiff/tif_predict.c"],
    "libungif-progs": ["*/lib/egif_lib.c", "*/lib/dgif_lib.c"],
    "giftrans": ["*/giftrans.c", "giftrans.c"],
    "netpbm-progs": ["*/ppm/ppmquant.c", "*/pnm/pnmscale.c"],
    "ImageMagick": ["*/magick/resize.c", "*/magick/quantize.c", "*/magick/compress.c"],
    "ghostscript": ["*/src/sdctc.c", "*/src/slzwe.c", "*/src/szlibe.c", "*/src/sfilter1.c"],
    "ghostscript-68000": ["*/src/sdctc.c", "*/src/slzwe.c", "*/src/szlibe.c", "*/src/sfilter1.c"],
    "libogg": ["*/src/framing.c", "*/src/bitwise.c"],
    "libvorbis": ["*/lib/mdct.c", "*/lib/lpc.c", "*/lib/envelope.c"],
    "sox": ["*/rate.c", "*/resample.c", "*/filter.c", "*/band.c"],
    "gsm": ["*/src/long_term.c", "*/src/short_term.c", "*/src/lpc.c", "*/src/rpe.c", "*/src/add.c"],
    "libmad": ["*/fixed.c", "*/layer3.c", "*/synth.c", "*/huffman.c"],
}
# Stub system headers icc lacks, so the trial reaches the kernel code instead of stopping at the first
# #include. 32-bit types only: code that needs a 64-bit type still fails, and is classed 64bit.
STUBS = {
    "sys/types.h": "#include <time.h>\ntypedef long off_t; typedef long ssize_t; typedef unsigned char u_char;\n"
                   "typedef unsigned short u_short; typedef unsigned int u_int; typedef unsigned long u_long;\n"
                   "typedef long time_t_stub; typedef int pid_t; typedef unsigned short mode_t;\n",
    "unistd.h": "", "sys/stat.h": "", "sys/time.h": "#include <time.h>\n", "pwd.h": "", "sys/wait.h": "",
    "libintl.h": "#define gettext(s) (s)\n",
    "fcntl.h": "#define O_RDONLY 0\n#define O_WRONLY 1\n#define O_RDWR 2\n#define O_CREAT 0x100\n#define O_TRUNC 0x200\n", "strings.h": "#include <string.h>\n",
    "memory.h": "#include <string.h>\n", "sys/param.h": "", "sys/file.h": "", "utime.h": "",
    "stdint.h": "typedef signed char int8_t; typedef unsigned char uint8_t; typedef short int16_t;\n"
                "typedef unsigned short uint16_t; typedef long int32_t; typedef unsigned long uint32_t;\n",
    "inttypes.h": "#include <stdint.h>\n",
}
# A package header with a standard name (a port's own errno.h, e.g. gzip's primos/) must not shadow
# icc's: flattening would otherwise break every file that includes it.
ANSI = {"assert.h", "ctype.h", "errno.h", "float.h", "limits.h", "locale.h", "math.h", "setjmp.h",
        "signal.h", "stdarg.h", "stddef.h", "stdio.h", "stdlib.h", "string.h", "time.h"}
# Headers `configure` or the build would generate, taken from the port's own shipped variant.
# (src glob under the source root, name to install, @VAR@ substitutions; leftover @X@ -> 0)
GENERATED = {
    "libjpeg": [("*/jconfig.st", "jconfig.h", {})],                     # the Atari ST config jpeg ships
    "libpng": [("*/scripts/pnglibconf.h.prebuilt", "pnglibconf.h", {})],
    "gmp": [("*/gmp-h.in", "gmp.h", {"GMP_LIMB_BITS": "32", "GMP_NAIL_BITS": "0", "DEFN_LONG_LONG_LIMB": "",
                                      "LIBGMP_DLL": "0", "CC": '"icc"', "CFLAGS": '""'})],
    "mpfr": [("../gmp-*/gmp-*/gmp-h.in", "gmp.h", {"GMP_LIMB_BITS": "32", "GMP_NAIL_BITS": "0",
                                                  "DEFN_LONG_LONG_LIMB": "", "LIBGMP_DLL": "0",
                                                  "CC": '"icc"', "CFLAGS": '""'})],
    "mpc": [("../gmp-*/gmp-*/gmp-h.in", "gmp.h", {"GMP_LIMB_BITS": "32", "GMP_NAIL_BITS": "0",
                                                 "DEFN_LONG_LONG_LIMB": "", "LIBGMP_DLL": "0",
                                                 "CC": '"icc"', "CFLAGS": '""'}),
            ("../mpfr-*/mpfr-*/mpfr.h", "mpfr.h", {})],
}
# What `configure` would have defined, per package: without these, size probes read as 0 and a
# header falls through to its `long long` branch, which is a harness artifact, not an icc limit.
DEFINES = {
    "gnupg": ["SIZEOF_UNSIGNED_SHORT=2", "SIZEOF_UNSIGNED_INT=4", "SIZEOF_UNSIGNED_LONG=4",
              "BYTES_PER_MPI_LIMB=4"],
    "openssl": ["OPENSSL_NO_SHA512"],        # sha512.c itself is still tried below, without it
}
ONLY_WITHOUT_DEFINES = {"openssl": ["sha512.c"]}
SRPM_OF = {"libungif-progs": "libungif", "netpbm-progs": "netpbm"}
CLASSES = [
    ("64bit", re.compile(r"long long|too large for 32-bit|inconsistent with 'long'|uint64|int64|u64\b", re.I)),
    ("header", re.compile(r"#include file .* wouldn't open", re.I)),
    # the package redeclares a libc object (`extern int errno;`): a one-line patch, like a header
    ("header", re.compile(r"type disagreement for '(_IMS_errno|errno|mem\w+|str\w+|mal\w+)'", re.I)),
]


def classify_log(log):
    errs = [ln for ln in log.splitlines() if ln.startswith(("Serious", "Error", "Fatal"))]
    if not errs:
        return "ok", ""
    for cls, rx in CLASSES:
        hit = next((e for e in errs if rx.search(e)), None)
        if hit:
            return cls, hit[:120]
    return "other", errs[0][:120]


def trial(pkg, srcroot):
    base = SRPM_OF.get(pkg, pkg)
    roots = sorted(glob.glob(os.path.join(srcroot, f"{base}-[0-9]*")))
    if not roots:
        return "check", [], "no unpacked source"
    root = roots[-1]
    files = []
    for g in KERNEL_FILES.get(pkg, []):
        files += glob.glob(os.path.join(root, g), recursive=True)
    files = sorted(set(files))
    if not files:
        cxx = glob.glob(os.path.join(root, "**", "*.cc"), recursive=True)
        return ("no", [], "C++ source only (icc is C)") if cxx else ("check", [], "kernel files not found")
    w = os.path.join(WORK, "w")
    shutil.rmtree(w, ignore_errors=True)
    os.makedirs(w)
    for h in glob.glob(os.path.join(root, "**", "*.h"), recursive=True) + \
            glob.glob(os.path.join(root, "**", "*.ch"), recursive=True):
        if os.path.basename(h) in ANSI:
            continue
        parts = os.path.relpath(h, root).split(os.sep)
        # flat, plus the last directory kept (ogg/ogg.h, magick/config.h, lib/fnmatch.h, lzo/lzoconf.h)
        for dst in ([parts[-1]] + ([os.path.join(parts[-2], parts[-1])] if len(parts) > 1 else [])):
            os.makedirs(os.path.dirname(os.path.join(w, dst)) or w, exist_ok=True)
            shutil.copy(h, os.path.join(w, dst))
        if pkg == "openssl" and "crypto" in parts:          # include/openssl/*.h are made at configure time
            os.makedirs(os.path.join(w, "openssl"), exist_ok=True)
            shutil.copy(h, os.path.join(w, "openssl", parts[-1]))
    for src, name, subs in GENERATED.get(pkg, []):
        hits = glob.glob(os.path.join(root, src))
        if hits:
            body = open(hits[0], errors="replace").read()
            for k, v in subs.items():
                body = body.replace(f"@{k}@", v)
            open(os.path.join(w, name), "w").write(re.sub(r"@[A-Z_]+@", "0", body))
    open(os.path.join(w, "config.h"), "a").close()
    for name, body in STUBS.items():
        os.makedirs(os.path.dirname(os.path.join(w, name)), exist_ok=True)
        if not os.path.exists(os.path.join(w, name)):
            open(os.path.join(w, name), "w").write(f"/* t425_icc_trial stub */\n{body}")
    env = dict(os.environ, D72UNI=os.path.join(R, "tools/d72uni"),
               PATH=os.path.join(R, "tools/d72uni/bin") + ":" + os.path.join(R, "tools/t4") + ":" + os.environ["PATH"])
    env["ISEARCH"] = f"{w}/ {env['D72UNI']}/libs/"
    results = []
    for f in files:
        base = os.path.basename(f).replace("-", "_")   # icc reads "mpih-mul1.c" as an option
        dst = os.path.join(w, base)
        shutil.copy(f, dst)
        for name in os.listdir(w):                       # icc rejects #warning (as in t4compress.sh)
            fp = os.path.join(w, name)
            if name.endswith((".c", ".h")) and os.path.isfile(fp):
                txt = open(fp, errors="replace").read()
                if re.search(r"(?m)^\s*#\s*warning", txt):
                    open(fp, "w").write(re.sub(r"(?m)^\s*#\s*warning.*$", "", txt))
        defs = [] if base in ONLY_WITHOUT_DEFINES.get(pkg, []) else DEFINES.get(pkg, [])
        p = subprocess.run(["icc", base, "-t4", "-dHAVE_CONFIG_H"] + [f"-d{d}" for d in defs] + ["-o", "t.t4h"],
                           cwd=w, env=env, capture_output=True, text=True, timeout=600)
        cls, why = classify_log(p.stdout + p.stderr)
        results.append((os.path.relpath(f, root), cls, why))
    kinds = {c for _, c, _ in results}
    if kinds == {"ok"}:
        verdict = "yes"
    elif "64bit" in kinds:
        # some files need 64-bit types: "no" if nothing compiles, "partial" if the rest can ship
        verdict = "partial" if "ok" in kinds else "no"
    else:
        verdict = "patchable" if kinds <= {"ok", "header"} else "check"
    first = next(((f, w_) for f, c, w_ in results if c != "ok"), ("", ""))
    return verdict, results, f"{first[0]}: {first[1]}" if first[0] else "all kernel files compile"


def main():
    srcroot, out = sys.argv[1], sys.argv[2]
    pkgs = sys.argv[3:] or sorted(KERNEL_FILES)
    with open(out, "a") as fh:
        for pkg in pkgs:
            verdict, results, why = trial(pkg, srcroot)
            ok = sum(1 for _, c, _ in results if c == "ok")
            line = f"{pkg}\t{verdict}\t{ok}/{len(results)}\t{why}"
            print(line, flush=True)
            fh.write(line + "\n")
            for f, c, w in results:
                fh.write(f"#\t{pkg}\t{f}\t{c}\t{w}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
