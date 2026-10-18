#!/bin/bash
# Package the TT's Dropbear (scripts/build_dropbear.sh: Monocypher curve25519/ed25519, X25519/Ed25519 via ttjobd or
# the T425, t4lock) as build/dropbear-<ver>-<rel>.m68kmint.rpm: /usr/sbin/dropbear, /usr/bin/dropbearkey,
# /usr/bin/dbclient, /etc/rc.dropbear (started from mint.cnf). Host keys stay outside the package. Binaries are
# stripped: unstripped ones carry temp object names and the build path. A new sshd takes effect at the next reboot.
# Prints the RPM path.
. "$(dirname "$0")/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
cd "$(dirname "$0")/.."
V=2026.94
# release = commits on the inputs + .sam1 (2026-10-18 audit: the fixed 2.sam1 met changed bytes after t4lock.c and
# build_dropbear.sh changed, and CI's publish gate went red)
R=${1:-$(git rev-list --count HEAD -- staging/DROPBEAR scripts/build_dropbear.sh scripts/build_dropbear_rpm.sh sam-yum-tt/src/t4lock.c sam-yum-tt/src/t4lock.h sam-yum-tt/src/t4adler.c sam-yum-tt/src/t4adler.h sam-job-tt/libttjob.c sam-job-tt/libttjob.h sam-job-tt/jobwire.h scripts/rpm_header.py).sam1}   # 1.sam1: first packaged build (t4lock self-pid fix, ttjobd client); 2.sam1: /etc/dropbear/dropbear.conf
scripts/build_dropbear.sh > /dev/null
S=tools/cross-mint/usr/bin/m68k-atari-mint-strip
for b in dropbear dropbearkey dbclient; do $S -o build/$b tools/dropbear-$V/$b; done
python3 - "$V" "$R" <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
v, r = sys.argv[1:]
rd = lambda p: open(p, "rb").read()
out = f"build/dropbear-{v}-{r}.m68kmint.rpm"
open(out, "wb").write(R.write_rpm("dropbear", v, r, [
    ("/usr/sbin/dropbear", 0o100755, rd("build/dropbear")),
    ("/usr/bin/dropbearkey", 0o100755, rd("build/dropbearkey")),
    ("/usr/bin/dbclient", 0o100755, rd("build/dbclient")),
    ("/etc/rc.dropbear", 0o100755, rd("staging/HD10_OVERLAY/etc/rc.dropbear")),
    ("/etc/dropbear/dropbear.conf", 0o100644, rd("staging/HD10_OVERLAY/etc/dropbear/dropbear.conf")),
], summary="Dropbear SSH server for the TT: curve25519/ed25519, crypto on the transputers via ttjobd (sam)",
   description="Dropbear 2026.94 (MIT licence) with Monocypher (BSD-2/CC0) for curve25519/ed25519; X25519 and "
               "Ed25519 verify go to ttjobd, else the T425, else the 68030. Host keys are not in the package.",
   fileflags={"/etc/dropbear/dropbear.conf": 1 | 16}))   # %config(noreplace)
print(out)
PY
