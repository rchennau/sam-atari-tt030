#!/bin/bash
# Package HighWire, SAM TT030 fork (github.com/rchennau/highwire-sam-tt030, branch sam-tt030) as
# build/highwire-<ver>-<rel>.m68kmint.rpm:
#   /opt/highwire/highwire.prg, highwire.rsc, deskicon.rsc, html/ (help + start pages), modules/network.ovl (0755:
#   ext2 enforces the execute bit for Pexec), highwire.cfg (%config: HTTP_PROXY = ttproxy.sam.int:3128),
#   /usr/bin/highwire (cd into /opt/highwire, run it), /var/cache/highwire (CACHEDIR).
# GPL-3.0, built from source here, so unlike highwire-tos (vendor 0.5.0 binary, cannot start on xVDI) it may be
# published. Replaces highwire-tos: both own /opt/highwire, so remove highwire-tos first.
#   scripts/build_highwire_rpm.sh [FORK_DIR]     default ../highwire-sam-tt030 (cloned if missing)
. "$(dirname "$0")/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
cd "$(dirname "$0")/.."
R=$PWD; F=${1:-$R/../highwire-sam-tt030}
[ -d "$F" ] || git clone -q -b sam-tt030 git@github.com:rchennau/highwire-sam-tt030.git "$F"
[ -z "$(git -C "$F" status --porcelain --untracked-files=no)" ] || { echo "build_highwire_rpm: $F has local changes" >&2; exit 1; }
bash "$F/packaging/build-tt030.sh" "$R" >/dev/null
V=$(sed -n 's/^#define _HIGHWIRE_VERSION_ "\([0-9.]*\)-sam\([0-9]*\)"$/\1 \2/p' "$F/src/version.h")
[ -n "$V" ] || { echo "build_highwire_rpm: version.h is not <upstream>-samN" >&2; exit 1; }
python3 - "$F" $V "$(git -C "$F" rev-parse --short HEAD)" <<'PY'
import os, sys
sys.path.insert(0, "scripts")
import rpm_header as R
f, ver, sam, sha = sys.argv[1:5]
rd = lambda p: open(os.path.join(f, p), "rb").read()
D = "/opt/highwire"
files = [(d, 0o40755, b"") for d in (D, f"{D}/modules", "/var/cache/highwire")]
files += [(f"{D}/highwire.prg", 0o100755, rd("dist/highwire.prg")),
          (f"{D}/modules/network.ovl", 0o100755, rd("modules/mintnet.ovl")),
          (f"{D}/highwire.rsc", 0o100644, rd("rsc/highwire.rsc")),
          (f"{D}/deskicon.rsc", 0o100644, rd("rsc/deskicon.rsc")),
          (f"{D}/highwire.cfg", 0o100644, rd("packaging/highwire.cfg")),
          ("/usr/bin/highwire", 0o100755, b'#!/bin/sh\ncd /opt/highwire && exec ./highwire.prg "$@"\n')]
for top, dirs, names in os.walk(os.path.join(f, "html")):
    rel = os.path.relpath(top, f)
    files.append((f"{D}/{rel}", 0o40755, b""))
    files += [(f"{D}/{rel}/{n}", 0o100644, rd(os.path.join(rel, n))) for n in sorted(names)]
out = f"build/highwire-{ver}-{sam}.sam.m68kmint.rpm"
open(out, "wb").write(R.write_rpm("highwire", ver, f"{sam}.sam", files,
    summary=f"HighWire web browser {ver}, SAM TT030 fork (fork {sha}); HTTPS via ttproxy.sam.int (sam)",
    description="GPL-3.0. github.com/rchennau/highwire-sam-tt030 (branch sam-tt030), upstream "
                "github.com/neilrackett/atarist-highwire. No TLS on the TT: HTTP_PROXY terminates it on CT114.",
    fileflags={f"{D}/highwire.cfg": 1}))   # RPMFILE_CONFIG: a local edit survives updates
print(out, len(files), "entries")
PY
