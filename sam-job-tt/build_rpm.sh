#!/bin/bash
# build_rpm.sh — package the job queue (tt030-transputer-job-queue, Deployment §5) as build/ttjob-<V>-<R>.m68kmint.rpm:
#   /usr/sbin/ttjobd, /usr/bin/ttjobcli, /usr/bin/ttlease, /usr/lib/t425/job{full,t1,t0}.btl (from build_t4.sh), /etc/rc.ttjobd.
# Release = number of commits touching sam-job-tt or sam-yum-tt/src (shared sources), so every change gets a newer NVR and an unchanged tree rebuilds the
# same name (ci.sh publishes only names the mirror lacks). Boot wiring stays outside the package: mint.cnf runs
# "exec u:/bin/bash /etc/rc.ttjobd" (set once by hand, 2026-10-13). A new ttjobd takes effect at the next reboot.
# Usage: build_rpm.sh [VERSION]   (run build_t4.sh first; prints the RPM path)
. "$(cd "$(dirname "$0")" && git rev-parse --show-toplevel)/scripts/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/.." && pwd); y=$root/sam-yum-tt/src
V=${1:-1.0}
R=$(git -C "$root" rev-list --count HEAD -- sam-job-tt sam-yum-tt/src)   # sam-yum-tt/src: ttjobd links atwboot.c, t4lock.c
for b in jobfull jobt1 jobt0; do [ -s "$here/build/$b.btl" ] || { echo "build_rpm: build/$b.btl missing (run build_t4.sh)" >&2; exit 1; }; done
CC="$root/tools/cross-mint/usr/bin/m68k-atari-mint-gcc -m68020-60 -O2 -Wall -s"   # -s: unstripped symbols carry temp object names, so no two builds match
$CC -I"$y" -I"$here" -o "$here/build/ttjobd.ttp" "$here/ttjobd.c" "$y/atwboot.c" "$y/t4lock.c"
$CC -I"$y" -I"$here" -o "$here/build/ttjobcli.ttp" "$here/ttjobcli.c" "$y/sha256.c" "$y/t4adler.c"
$CC -I"$y" -I"$here" -o "$here/build/ttlease.ttp" "$here/ttlease.c" "$here/libttjob.c" "$y/t4adler.c"
python3 - "$root" "$here" "$V" "$R" <<'PY'
import sys
root, here, v, r = sys.argv[1:]
sys.path.insert(0, f"{root}/scripts")
import rpm_header as R
rd = lambda p: open(p, "rb").read()
out = f"{here}/build/ttjob-{v}-{r}.m68kmint.rpm"
files = [("/usr/sbin/ttjobd", 0o100755, rd(f"{here}/build/ttjobd.ttp")),
         ("/usr/bin/ttjobcli", 0o100755, rd(f"{here}/build/ttjobcli.ttp")),
         ("/usr/bin/ttlease", 0o100755, rd(f"{here}/build/ttlease.ttp")),
         ("/etc/rc.ttjobd", 0o100755, rd(f"{here}/tt/rc.ttjobd"))]
files += [(f"/usr/lib/t425/{b}.btl", 0o100644, rd(f"{here}/build/{b}.btl")) for b in ("jobfull", "jobt1", "jobt0")]
open(out, "wb").write(R.write_rpm("ttjob", v, r, files,
    summary="Transputer job queue: ttjobd + resident T425/TRAM images (sam)",
    description="ttjobd owns the ATW800/2 link and runs jobs on the T425 and both TRAMs; clients (Dropbear, "
                "tgzip) fall back to the 68030 when it is absent. The .btl images are built with the INMOS "
                "toolset: LAN mirror only, never redistributed. Boot: mint.cnf runs /etc/rc.ttjobd."))
print(out)
PY
