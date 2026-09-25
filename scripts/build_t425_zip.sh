#!/bin/sh
# zip 2.3 with deflate on the T425 (tt030-t425-kernel-ports W1), as two RPMs:
#   build/zip-2.3-4.m68kmint.rpm          zip, zipcloak, zipnote, zipsplit + /usr/lib/t425/zip.btl (+ docs, man)
#   build/zip-t425-on-2.3-4.m68kmint.rpm  /etc/t425/enabled/zip   (held until the FR-5 gate)
# Source: SpareMiNT's zip-2.3-2 SRPM (tools/rpm-src/x/) + its two patches, built with USE_ZLIB against the
# pinned zlib 1.3.2 (zip 2.3's own zlib path): the T425 runs the same zlib (compserv op 'r' = raw
# deflate, the parameters of zip's zl_deflate_init), so the 68030 fallback in this binary and the T425
# produce the same bytes. Output differs from SpareMiNT zip's built-in deflate; unzip reads both.
# T425 patch (zipup.c, filecompress): switch on, no -l/-ll, 128 KB..1.5 MB (measured break-even) -> whole file in one t4call;
# any failure rewinds the input (crc, isize reset) and runs zlib here, saying why once.
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
SRPM=$REPO/tools/rpm-src/x/zip-2.3-2
RT=$REPO/sam-yum-tt/src
Z=$REPO/tools/rpm-src/zlib-1.3.2
W=/tmp/zip-t425-build
XBIN=$REPO/tools/cross-mint/usr/bin
CC="$XBIN/m68k-atari-mint-gcc"
rm -rf "$W" && mkdir -p "$W" && tar xzf "$SRPM/zip23.tar.gz" -C "$W"
cd "$W/zip-2.3"
patch -s -p1 < "$SRPM/zip23.patch"
patch -s -p1 < "$SRPM/zip-2.3-cpp.patch"
python3 - <<'PY'
p = "zipup.c"
s = open(p, encoding="latin-1").read()
decl = "local unsigned file_read(buf, size)\n"      # after the file's local globals (ifile, crc, isize)
assert decl in s, "zipup.c: file_read definition not found"
s = s.replace(decl, r'''
#if defined(T425) && defined(USE_ZLIB)
#include "t4call.h"
#include <sys/stat.h>
#define ZIP_T4_MIN (128L * 1024)	/* measured 2026-09-24: 16 KB 1.25 -> 2.15 s, 64 KB ~1.0,
					   128 KB 6.53 -> 6.46 s, 256 KB 0.78-0.84 (zlib is fast here) */
#define ZIP_T4_MAX (1536L * 1024)	/* in + out must fit the T425's 4 MB board */
#define ZIP_T4_BTL "/usr/lib/t425/zip.btl"
/* Deflate the whole input on the T425 (tt030-t425-kernel-ports). Returns the compressed size, or -1
 * with the input rewound so the zlib path runs here. Mirrors that path: data_type -> att, and STORE
 * when the file fit one input buffer and did not shrink. */
local long t4_deflate_file(int lvl, FILE *zipfile, int *cmpr_method, ush *att, unsigned ibsz)
{
  static int said;
  struct stat st;
  char *in = NULL, *out = NULL;
  const char *why = "failed";
  long n, got = 0, len = -1, cap;
  unsigned k;

  if (translate_eol || !t4_enabled("zip") || fstat(ifile, &st) != 0
      || st.st_size < ZIP_T4_MIN || st.st_size > ZIP_T4_MAX)
    return -1;
  n = (long)st.st_size;
  cap = n + n / 2 + 1024;
  in = (char *)malloc(n);
  out = (char *)malloc(cap);
  if (in == NULL || out == NULL)
    why = "had no memory for the file";
  else {
    while (got < n) {
      k = file_read(in + got, (unsigned)(n - got > (long)SBSZ ? (long)SBSZ : n - got));
      if (k == 0 || k == (unsigned)EOF)
        break;
      got += (long)k;
    }
    if (got != n)
      why = "input changed size while reading";
    else
      len = t4call(ZIP_T4_BTL, 'r', lvl, in, n, out, cap, &why);
  }
  if (len >= 1) {
    if (*att == (ush)UNKNOWN)
      *att = (ush)(out[0] == Z_ASCII ? ASCII : BINARY);
    if (n < (long)ibsz && len - 1 >= n && fseekable(zipfile)) {
      if (zfwrite(in, 1, (extent)n, zipfile) != (extent)n)
        ziperr(ZE_TEMP, "error writing to zipfile");
      *cmpr_method = STORE;
      len = n;
    } else {
      if (zfwrite(out + 1, 1, (extent)(len - 1), zipfile) != (extent)(len - 1))
        ziperr(ZE_TEMP, "error writing to zipfile");
      len -= 1;
    }
  } else {
    if (!said++)
      fprintf(stderr, "zip: T425 %s; compressing on the 68030\n", why);
    if (got) {                          /* file_read already counted it: start the file again */
      lseek(ifile, 0L, SEEK_SET);
      isize = 0L;
      crc = CRCVAL_INITIAL;
    }
    len = -1;
  }
  free(in);
  free(out);
  return len;
}
#endif

''' + decl, 1)
anchor = """    if (level <= 2) {
        z_entry->flg |= 4;
    } else if (level >= 8) {
        z_entry->flg |= 2;
    }
"""
assert anchor in s, "zipup.c: level flag block not found"
s = s.replace(anchor, anchor + """#ifdef T425
    {
        long t4len = t4_deflate_file(level, zipfile, cmpr_method, &z_entry->att, ibuf_sz);
        if (t4len >= 0)
            return (ulg)t4len;
    }
#endif
""", 1)
open(p, "w", encoding="latin-1").write(s)
PY
OBJS=
for c in t4call t4lock t4adler atwboot; do
    "$CC" -m68020-60 -O2 -c -I"$RT" -o "$W/$c.o" "$RT/$c.c"; OBJS="$OBJS $W/$c.o"
done
for c in adler32 crc32 deflate trees zutil; do
    "$CC" -m68020-60 -O2 -c -I"$Z" -o "$W/z_$c.o" "$Z/$c.c"; OBJS="$OBJS $W/z_$c.o"
done
PATH=$XBIN:$PATH make -s -f unix/Makefile zips CC=m68k-atari-mint-gcc BIND=m68k-atari-mint-gcc \
    CFLAGS="-std=gnu89 -O2 -m68020-60 -I. -DUNIX -DUSE_ZLIB -DT425 -I$RT -I$Z" \
    LFLAGS2="-s $OBJS" > "$W/make.log" 2>&1 || { tail -25 "$W/make.log"; exit 1; }
sh "$REPO/sam-yum-tt/t4compress.sh" /tmp/cb > /dev/null
mkdir -p "$W/stock" && (cd "$W/stock" && 7z e -y "/mnt/vault/tt030/m68kmint/zip-2.3-2.m68kmint.rpm" > /dev/null \
    && cpio -idm --quiet < zip-2.3-2.m68kmint.cpio)
cd "$REPO"
python3 - "$W" <<'PY'
import os, sys
sys.path.insert(0, "scripts")
import rpm_header as R
w = sys.argv[1]
files = [(f"/usr/bin/{b}", 0o100755, open(f"{w}/zip-2.3/{b}", "rb").read())
         for b in ("zip", "zipcloak", "zipnote", "zipsplit")]
stock = os.path.join(w, "stock")
for root, dirs, names in sorted(os.walk(stock)):
    for n in sorted(names):
        p = os.path.join(root, n)
        rel = "/" + os.path.relpath(p, stock)
        if rel.startswith(("/usr/doc/", "/usr/man/")):
            files.append((rel, 0o100644, open(p, "rb").read()))
files.append(("/usr/lib/t425/zip.btl", 0o100644, open("/tmp/cb/compserv.b4h", "rb").read()))
open("build/zip-2.3-4.m68kmint.rpm", "wb").write(R.write_rpm("zip", "2.3", "4", files,
    summary="zip 2.3 (zlib); deflate offloads to the ATW800/2 T425 when zip-t425-on is installed (sam)",
    provides=["zip"]))
open("build/zip-t425-on-2.3-4.m68kmint.rpm", "wb").write(R.write_rpm("zip-t425-on", "2.3", "4",
    [("/etc/t425/enabled/zip", 0o100644, b"")], summary="turn on T425 offload for zip (after its FR-5 gate)",
    requires_=["zip"]))
print("build/zip-2.3-4.m68kmint.rpm build/zip-t425-on-2.3-4.m68kmint.rpm", len(files), "files")
PY
