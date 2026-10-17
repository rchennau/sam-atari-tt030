#!/bin/bash
# Package the two tool sets the HD10 overlay installs unowned (FHS audit C3, 2026-10-17) from the exact staged
# binaries (byte-identical to the live TT's), so ownership changes and behaviour does not:
#   sntp-tt-1.0-1    /usr/sbin/sntp (src/sntp.c; mint.cnf runs it at boot)
#   lrzsz-tt-0.12.20-1  /usr/bin/lrz, lsz, rz -> lrz, sz -> lsz, szs (ZMODEM; scripts/tt_zput.sh depends on rz)
# Prints the RPM paths.
. "$(dirname "$0")/tt_log.sh"   # log every run for SAM (operator rule 2026-10-13)
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
import sys
sys.path.insert(0, "scripts")
import rpm_header as R
O = "staging/HD10_OVERLAY"
x = lambda p: (p, 0o100755, open(O + p, "rb").read())
link = lambda p, t: (p, 0o160000 | 0o777, t.encode())     # MiNT S_IFLNK, body = target (rpm_header.write_rpm)
for name, ver, files, summ in (
    ("sntp-tt", "1.0", [x("/usr/sbin/sntp")], "Set the TT clock from an NTP server at boot (sam)"),
    ("lrzsz-tt", "0.12.20", [x("/usr/bin/lrz"), x("/usr/bin/lsz"), x("/usr/bin/szs"),
                             link("/usr/bin/rz", "lrz"), link("/usr/bin/sz", "lsz")],
     "lrzsz ZMODEM rz/sz for MiNT, plus szs (echo-safe sz over the serial console) (sam)"),
):
    out = f"build/{name}-{ver}-1.m68kmint.rpm"
    open(out, "wb").write(R.write_rpm(name, ver, "1", files, summary=summ))
    print(out)
PY
