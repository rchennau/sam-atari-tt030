#!/bin/sh
# Package the TT yum client (sam-yum-tt) as build/yum-tt-<ver>-1.m68kmint.rpm (tt030-rpm-pipeline FR-3).
# Contents: /usr/bin/yum, /usr/lib/yum.tt/resolve.awk, /etc/yum.tt/index.pub (the mirror's public
# signing key; the private half is ~/.config/tt030-mirror/index-sign.key on fractal only).
set -eu
cd "$(dirname "$0")/.."
V=${1:-1.0}
python3 - "$V" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
v = sys.argv[1]
out = f"build/yum-tt-{v}-1.m68kmint.rpm"
open(out, "wb").write(R.write_rpm("yum-tt", v, "1", [
    ("/usr/bin/yum", 0o100755, open("sam-yum-tt/src/yum", "rb").read()),
    ("/usr/lib/yum.tt/resolve.awk", 0o100644, open("sam-yum-tt/src/resolve.awk", "rb").read()),
    ("/etc/yum.tt/index.pub", 0o100644, open("sam-yum-tt/etc/index.pub", "rb").read()),
], summary="yum for the Atari TT from the SAM mirror (sam)", requires_=["mawk", "wget", "openssl", "/bin/sh"]))
print(out)
PY
