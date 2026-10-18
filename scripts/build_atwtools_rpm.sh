#!/bin/bash
# Package the ATW800/2 display tools as build/atwtools-<ver>-1.m68kmint.rpm (product rule 2, FHS audit A1-A3,
# maestro/tracks/tt030-fhs-layout/audit-2026-10-17.md):
#   /usr/sbin  ttsaver.prg, atwmono.prg (XaAES autorun keeps .PRG: a .TTP would open a toswin2 window), ttreboot.prg
#   /usr/bin   atwpal atwres atwshot blitchk fillchk mxprobe vdiprof vtgprobe ttsound vdibench
#   /usr/share/atw/palettes  normal.pal, amber.pal (scripts/atw_palette.py)
# C:\AUTO\DEFAULT.PAL stays on C: (xVDI reads it before MiNT). TTRUN.PRG/EMUTOS.PRG stay too (plain-TOS runs,
# sam-rom-tt). Stripped builds, so reproducible. Prints the RPM path.
. "$(dirname "$0")/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
cd "$(dirname "$0")/.."
V=${1:-1.0}
CC="tools/cross-mint/usr/bin/m68k-atari-mint-gcc -m68020-60 -O2 -s -Wall"
O=build/atwtools; mkdir -p $O
$CC -o $O/ttsaver.prg src/atwbg/ttsaver.c src/atwbg/matrix.c -lgem
$CC -o $O/atwmono.prg src/atwbg/atwmono.c
$CC -o $O/ttreboot.prg sam-rom-tt/src/ttreboot.c
for t in atwres atwshot mxprobe vdiprof vtgprobe; do $CC -o $O/$t src/atwbg/$t.c; done
for t in atwpal blitchk fillchk; do $CC -o $O/$t src/atwbg/$t.c -lgem; done
$CC -o $O/ttsound src/ttsound.c -lm
$CC -o $O/vdibench src/vdibench.c -lgem
python3 scripts/atw_palette.py normal $O/normal.pal
python3 scripts/atw_palette.py amber $O/amber.pal
python3 - "$V" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
v, o = sys.argv[1], "build/atwtools"
f = lambda p, n: (p, 0o100755, open(f"{o}/{n}", "rb").read())
files = [f(f"/usr/sbin/{n}", n) for n in ("ttsaver.prg", "atwmono.prg", "ttreboot.prg")]
files += [f(f"/usr/bin/{n}", n) for n in ("atwpal", "atwres", "atwshot", "blitchk", "fillchk", "mxprobe", "vdiprof",
                                           "vtgprobe", "ttsound", "vdibench")]
files += [(f"/usr/share/atw/palettes/{n}", 0o100644, open(f"{o}/{n}", "rb").read()) for n in ("normal.pal", "amber.pal")]
out = f"build/atwtools-{v}-1.m68kmint.rpm"
open(out, "wb").write(R.write_rpm("atwtools", v, "1", files,
    summary="ATW800/2 display tools: screensaver (matrix), mono copy-engine hook, probes, screenshots (sam)",
    description="ttsaver, atwmono, ttreboot in /usr/sbin; atwpal, atwres, atwshot, blitchk, fillchk, mxprobe, vdiprof, "
                "vtgprobe, ttsound, vdibench in /usr/bin; palettes in /usr/share/atw/palettes."))
print(out)
PY
