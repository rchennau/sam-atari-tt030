#!/bin/sh
# lha 1.14i with its -lh5-/6-/7- encoder on the T425 (tt030-t425-kernel-ports W1), as two RPMs:
#   build/lha-1.14i-5.m68kmint.rpm          /usr/bin/lha + /usr/lib/t425/lha.btl   (replaces SpareMiNT 1.14i-1)
#   build/lha-t425-on-1.14i-5.m68kmint.rpm  /etc/t425/enabled/lha                 (held until the FR-5 gate)
# Source: SpareMiNT's lha-1.14i-1 SRPM (tools/rpm-src/x/), its two patches, and its spec's switches:
# OPTIMIZE is REPLACED (the spec passes OPTIMIZE="$RPM_OPT_FLAGS -DHAVE_NO_LCHOWN"), so there is no
# SUPPORT_LH7. With it, lha defaults to -lh7- and SpareMiNT's lha cannot read the archives ("make_table()
# Bad table", found by the interop check 2026-09-24); it also changes lh5's window (MAX_DICBIT).
# -std=gnu89 and -include time.h: the cross gcc is C23 (K&R `f()` = no arguments) and modern mintlib's
# <sys/time.h> no longer pulls in <time.h>.
# T425 patch (append.c, encode_lzhuf): with the switch on and the file between 16 KB (below that the
# ~1 s boot loses; measured) and 1.5 MB (in + out must fit the T425's 4 MB), the whole file goes to lha.btl in one t4call (a member's stream cannot be split); any
# failure seeks the input back and runs lha's own encode() on the 68030, saying why once.
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
SRPM=$REPO/tools/rpm-src/x/lha-1.14i-1
RT=$REPO/sam-yum-tt/src
W=/tmp/lha-t425-build
XBIN=$REPO/tools/cross-mint/usr/bin
rm -rf "$W" && mkdir -p "$W" && tar xzf "$SRPM/lha-114i.tar.gz" -C "$W"
cd "$W/lha-114i"
patch -s -p0 < "$SRPM/lha-1.14i-make.patch"
patch -s -p0 < "$SRPM/lha-1.14e-ext.patch"
python3 - <<'PY'
p = "src/append.c"
s = open(p, encoding="latin-1").read()
inc = '#include "lha.h"\n'
assert inc in s, "append.c: lha.h include not found"
s = s.replace(inc, inc + '''
#ifdef T425
#include "t4call.h"
#define LHA_T4_MAX (1536L * 1024)	/* in + out must fit the T425's 4 MB board */
#define LHA_T4_MIN (16L * 1024)		/* measured break-even, 2026-09-24: 4 KB 1.04 -> 1.78 s,
					   16 KB 2.42 -> 2.44 s, 64 KB 8.07 -> 5.34 s (boot ~1 s) */
#define LHA_T4_BTL "/usr/lib/t425/lha.btl"
/* Encode all of infp on the T425 (tt030-t425-kernel-ports). 0 = done, -1 = run encode() here. */
static int
t4_encode(FILE *infp, FILE *outfp, long size, int method, long *packed)
{
	static int said;
	unsigned char *in, *out;
	const char *why = "failed";
	long pos = ftell(infp), len = -1;

	if (method < 5 || size < LHA_T4_MIN || size > LHA_T4_MAX || !t4_enabled("lha"))
		return -1;
	in = (unsigned char *) malloc(size);
	out = (unsigned char *) malloc(size + 66);
	if (!in || !out)
		why = "had no memory for the file";
	else if ((long) fread(in, 1, size, infp) != size)
		why = "could not read the input";
	else
		len = t4call(LHA_T4_BTL, '0' + method, 0, in, size, out, size + 66, &why);
	if (len >= 2 && (long) fwrite(out + 2, 1, len - 2, outfp) != len - 2) {
		why = "result not written";
		len = -1;
	}
	if (len < 2) {
		if (!said++)
			fprintf(stderr, "lha: T425 %s; compressing on the 68030\\n", why);
		fseek(infp, pos, SEEK_SET);
	} else {
		*packed = len - 2;
		crc = out[0] | (out[1] << 8);
	}
	free(in);
	free(out);
	return len < 2 ? -1 : 0;
}
#endif
''', 1)
old = '''		interface.original = size;
		start_indicator(name, size, "Freezing", 1 << dicbit);
		encode(&interface);
		*packed_size_var = interface.packed;
		*original_size_var = interface.original;'''
assert old in s, "append.c: encode call not found"
s = s.replace(old, '''		interface.original = size;
#ifdef T425
		if (t4_encode(infp, outfp, size, interface.method, packed_size_var) == 0)
			*original_size_var = size;
		else
#endif
		{
		start_indicator(name, size, "Freezing", 1 << dicbit);
		encode(&interface);
		*packed_size_var = interface.packed;
		*original_size_var = interface.original;
		}''', 1)
open(p, "w", encoding="latin-1").write(s)

# extract side: lh5/lh6 decode on the T425 (op 'd'); lha checks the returned crc against the header
p = "src/extract.c"
s = open(p, encoding="latin-1").read()
inc = '#include "lha.h"\n'
assert inc in s, "extract.c: lha.h include not found"
s = s.replace(inc, inc + '''
#ifdef T425
#include "t4call.h"
#define LHA_T4_DMIN (256L * 1024)	/* original size. Measured 2026-09-24: decode is cheap here, so the
					   ~1 s boot loses below this (16 KB 0.92 -> 1.82 s); 256 KB text
					   6.9 -> 5.9 s (0.86), binary 7.9 -> 7.0 s (0.89) */
#define LHA_T4_DMAX (1536L * 1024)	/* packed + original must fit the T425's 4 MB board */
/* Decode one member on the T425 (tt030-t425-kernel-ports). 0 = done (crc set), -1 = decode() here. */
static int
t4_decode(FILE *infp, FILE *outfp, long original, long packed, int method)
{
	static int said;
	unsigned char *in, *out;
	const char *why = "failed";
	long pos = ftell(infp), len = -1;

	if ((method != LZHUFF5_METHOD_NUM && method != LZHUFF6_METHOD_NUM) || verify_mode || text_mode
	    || outfp == NULL || original < LHA_T4_DMIN || original > LHA_T4_DMAX || packed <= 0
	    || packed > original + 1024 || !t4_enabled("lha"))
		return -1;
	in = (unsigned char *) malloc(packed + 4);
	out = (unsigned char *) malloc(original + 66);
	if (!in || !out)
		why = "had no memory for the member";
	else if ((long) fread(in + 4, 1, packed, infp) != packed)
		why = "could not read the member";
	else {
		in[0] = (unsigned char) original;
		in[1] = (unsigned char) (original >> 8);
		in[2] = (unsigned char) (original >> 16);
		in[3] = (unsigned char) (original >> 24);
		len = t4call("/usr/lib/t425/lha.btl", 'd', method, in, packed + 4, out, original + 66, &why);
	}
	if (len == original + 2 && (long) fwrite(out + 2, 1, original, outfp) != original) {
		why = "result not written";
		len = -1;
	}
	if (len != original + 2) {
		if (!said++)
			fprintf(stderr, "lha: T425 %s; extracting on the 68030\\n", why);
		fseek(infp, pos, SEEK_SET);
	} else
		crc = out[0] | (out[1] << 8);
	free(in);
	free(out);
	return len == original + 2 ? 0 : -1;
}
#endif
''', 1)
old = """	interface.packed = packed_size;

	switch (method) {"""
assert old in s, "extract.c: decode_lzhuf switch not found"
s = s.replace(old, """	interface.packed = packed_size;

#ifdef T425
	if (t4_decode(infp, outfp, original_size, packed_size, method) == 0)
		return crc;
#endif
	switch (method) {""", 1)
open(p, "w", encoding="latin-1").write(s)
PY
T4O=
for c in t4call t4lock t4adler atwboot; do
    "$XBIN/m68k-atari-mint-gcc" -m68020-60 -O2 -c -I"$RT" -o "$W/$c.o" "$RT/$c.c"
    T4O="$T4O $W/$c.o"
done
PATH=$XBIN:$PATH make -s CC=m68k-atari-mint-gcc \
    OPTIMIZE="-std=gnu89 -include time.h -O2 -m68020-60 -DHAVE_NO_LCHOWN -DT425 -I$RT" \
    LDFLAGS="$T4O" > "$W/make.log" 2>&1 || { tail -20 "$W/make.log"; exit 1; }
"$XBIN/m68k-atari-mint-strip" src/lha
sh "$REPO/sam-yum-tt/t4lha.sh" /tmp/lh > /dev/null
cd "$REPO"
python3 - "$W/lha-114i/src/lha" /tmp/lh/lhaserv.b4h <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
lha, btl = sys.argv[1], sys.argv[2]
open("build/lha-1.14i-5.m68kmint.rpm", "wb").write(R.write_rpm("lha", "1.14i", "5", [
    ("/usr/bin/lha", 0o100755, open(lha, "rb").read()),
    ("/usr/lib/t425/lha.btl", 0o100644, open(btl, "rb").read()),
], summary="LHa 1.14i; -lh5-/-lh6- encoding and decoding offload to the ATW800/2 T425 when lha-t425-on is installed (sam)",
    provides=["lha"]))
open("build/lha-t425-on-1.14i-5.m68kmint.rpm", "wb").write(R.write_rpm("lha-t425-on", "1.14i", "5", [
    ("/etc/t425/enabled/lha", 0o100644, b""),
], summary="turn on T425 offload for lha (after its FR-5 gate)", requires_=["lha"]))
print("build/lha-1.14i-5.m68kmint.rpm build/lha-t425-on-1.14i-5.m68kmint.rpm")
PY
