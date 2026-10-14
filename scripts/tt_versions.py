#!/usr/bin/env python3
"""tt_versions.py — what the TT runs vs what the mirror offers (adopted from SAM PMS node-runtime-version, 2026-10-13).

    scripts/tt_versions.py [MIRROR]        default /mnt/vault/tt030 (TT_MIRROR)

Reads ttmon's retained frame sam/node/atari-tt030/runtime (installed packages + ttjobd start time) and the mirror's
index.tsv, and prints one line per package that is not CURRENT, then a summary:
  STALE           the mirror has a newer version-release of an installed package (yum update would change it)
  RUNNING_STALE   ttjobd started before its installed ttjob package (the new binary waits for a reboot)
  UNREPORTED      no frame on the broker (ttmon not running, or a ttmon without the runtime frame)
Exit 0 when everything is CURRENT, 1 otherwise, 2 when the mirror index cannot be read. Packages the mirror does not
carry at all are counted as "local", never as current or stale.
"""
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tt_rpm import read_index, rpmvercmp  # noqa: E402

TOPIC = "sam/node/atari-tt030/runtime"


def evaluate(frame, index):
    """frame: ttmon runtime dict or None; index: tt_rpm.read_index rows. Returns (rows, counts)."""
    if frame is None:
        return [("UNREPORTED", "-", f"no retained frame on {TOPIC}")], {"UNREPORTED": 1}
    rows, counts = [], {"CURRENT": 0, "STALE": 0, "RUNNING_STALE": 0, "local": 0}
    for name, (verrel, _t) in sorted(frame.get("packages", {}).items()):
        r = index.get(name)
        if r is None:
            counts["local"] += 1
            continue
        ver, _, rel = verrel.rpartition("-")
        if (rpmvercmp(r[1], ver) or rpmvercmp(r[2], rel)) > 0:
            rows.append(("STALE", name, f"{verrel} installed, {r[1]}-{r[2]} on the mirror"))
            counts["STALE"] += 1
        else:
            counts["CURRENT"] += 1
    tj = frame.get("packages", {}).get("ttjob")
    started = frame.get("ttjobd_started", 0)
    if tj and started and tj[1] > started:
        rows.append(("RUNNING_STALE", "ttjobd", f"started {started}, ttjob {tj[0]} installed {tj[1]} (reboot to load it)"))
        counts["RUNNING_STALE"] += 1
    return rows, counts


def fetch_frame(timeout=3):
    """The retained frame is the snapshot we want, so -C 1 exiting on it is correct here."""
    p = subprocess.run(["mosquitto_sub", "-h", "mqtt.sam.int", "-t", TOPIC, "-C", "1", "-W", str(timeout)],
                       capture_output=True, text=True)
    return json.loads(p.stdout) if p.returncode == 0 and p.stdout.strip() else None


def main(argv):
    mirror = argv[1] if len(argv) > 1 else os.environ.get("TT_MIRROR", "/mnt/vault/tt030")
    try:
        index = read_index(mirror)
    except OSError as e:
        print(f"tt_versions: mirror index unreadable: {e}", file=sys.stderr)
        return 2
    rows, counts = evaluate(fetch_frame(), index)
    for state, name, why in rows:
        print(f"{state:13} {name:20} {why}")
    print("tt_versions: " + " ".join(f"{k}={v}" for k, v in counts.items()))
    return 0 if not rows else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
