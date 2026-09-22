#!/usr/bin/env python3
"""tt_reach_probe.py — can fractal REACH the TT? (RCA 2026-09-21-tt030-inbound-outage D1/A2)

ttmon's telemetry only proves the TT can reach the broker; on 2026-09-21 it said READY for minutes
while nothing could reach the TT. This probes the other direction and publishes, retained:

  sam/node/reachability/atari-tt030
  {"node_id":"atari-tt030","ts":...,"tcp22":"ok"|"fail","connect_ms":N|null,"neigh":"REACHABLE|STALE|FAILED|…",
   "from":"fractal","status":"REACHABLE"|"UNREACHABLE"}

`UNREACHABLE` while ttmon says READY is the inbound-only failure, named. One shot; a systemd timer
runs it. Exit 0 reachable · 1 unreachable · 2 could not publish (so the timer's failure is visible).
"""
import json
import socket
import subprocess
import sys
import time

TT = "192.168.0.30"  # sam.int-exception: the TT has no DNS record; static LAN address (inf-card)
TOPIC = "sam/node/reachability/atari-tt030"


def neigh(ip):
    out = subprocess.run(["ip", "neigh", "show", ip], capture_output=True, text=True).stdout.split()
    states = [w for w in out if w.isupper() and w not in ("PERMANENT",)] or ["NONE"]
    # worst state across interfaces (fractal reaches the LAN on two)
    order = ["FAILED", "INCOMPLETE", "NONE", "STALE", "DELAY", "PROBE", "REACHABLE"]
    return min(states, key=lambda s: order.index(s) if s in order else 2)


def probe(ip=TT, port=22, timeout=10):
    t = time.monotonic()
    try:
        with socket.create_connection((ip, port), timeout=timeout):
            return "ok", round((time.monotonic() - t) * 1000)
    except OSError:
        return "fail", None


def main():
    from sam.utils import mqtt as sm
    tcp, ms = probe()
    rec = {"node_id": "atari-tt030", "ts": int(time.time()), "tcp22": tcp, "connect_ms": ms,
           "neigh": neigh(TT), "from": socket.gethostname(),
           "status": "REACHABLE" if tcp == "ok" else "UNREACHABLE"}
    print(json.dumps(rec))
    if not sm.publish(TOPIC, json.dumps(rec), retain=True, qos=1):
        print("tt_reach_probe: publish FAILED", file=sys.stderr)
        return 2
    return 0 if tcp == "ok" else 1


if __name__ == "__main__":
    sys.exit(main())
