#!/bin/bash
# Package TosWin2 (GEM terminal / XaAES console catcher) with the sam idle back-off (staging/TOSWIN2/idle-backoff.patch:
# poll every 50 ms while busy, 250 ms after ~2 s idle; idle CPU 6.5 % -> 1.9 % of the TT030, measured 2026-10-14) as
# build/toswin2-2.7-<rel>.m68kmint.rpm: /opt/GEM/toswin2/toswin2.app only (the resource and config files stay as
# installed). Sources pinned: freemint/toswin2 af2e9e2 (2.7 + upstream fixes to 2026-09-20), freemint/cflib ce57817c3f68.
# GPL (toswin2) / LGPL (cflib). A new toswin2 takes effect at the next reboot. Prints the RPM path.
. "$(dirname "$0")/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
cd "$(dirname "$0")/.."
R=${1:-1.sam1}
T=$PWD/tools; export PATH=$T/cross-mint/usr/bin:$PATH
fetch() { [ -d "$T/$1" ] || git clone -q "https://github.com/freemint/$2.git" "$T/$1"; git -C "$T/$1" fetch -q --depth 50 origin; git -C "$T/$1" checkout -q -f "$3"; git -C "$T/$1" clean -qfdx; }
fetch cflib-src cflib ce57817c3f68c7d346d1081768505e81acaee807
fetch toswin2-src toswin2 af2e9e2
make -C "$T/cflib-src" -j8 > /dev/null 2>&1
git -C "$T/toswin2-src" apply "$PWD/staging/TOSWIN2/idle-backoff.patch"
make -C "$T/toswin2-src" 030 M68K_ATARI_MINT_CFLAGS="-I$T/cflib-src" M68K_ATARI_MINT_LDFLAGS="-L$T/cflib-src/.build/m68020" > /dev/null 2>&1
m68k-atari-mint-strip -o build/toswin2.app "$T/toswin2-src/.compile_030/toswin2.app"
python3 - "$R" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
r = sys.argv[1]
out = f"build/toswin2-2.7-{r}.m68kmint.rpm"
open(out, "wb").write(R.write_rpm("toswin2", "2.7", r, [
    ("/opt/GEM/toswin2/toswin2.app", 0o100755, open("build/toswin2.app", "rb").read()),
], summary="TosWin2 GEM terminal with an idle poll back-off (sam)",
   description="freemint/toswin2 af2e9e2 + staging/TOSWIN2/idle-backoff.patch (GPL); cflib (LGPL)."))
print(out)
PY
