#!/bin/bash
# tt_power.sh — switch the TT030's mains via the SP20 plug "TT030" on the LAN (tt030-rom-set FR-2). No cloud at run time:
# tinytuya local protocol 3.3 to the plug, key from a sam-only file fetched once from the Tuya cloud (2026-10-19).
#   scripts/tt_power.sh status | on | off | cycle [off_seconds=10]
# Plug: 192.168.0.102 (UDR7 fixed IP). Only one local client at a time: an open SmartLife app makes this fail (rc 3).
. "$(dirname "$0")/tt_log.sh"
D=${TT_PLUG_DIR:-/home/sam/.local/share/tt030-plug}
[ -r "$D/devices.json" ] || { echo "tt_power: $D/devices.json not readable (run as sam)" >&2; exit 2; }
"$D/.venv/bin/python" - "$D/devices.json" "${1:-status}" "${2:-10}" <<'PY'
import json, sys, time, tinytuya
path, cmd, secs = sys.argv[1], sys.argv[2], float(sys.argv[3])
x = json.load(open(path))[0]
def dev():
    return tinytuya.OutletDevice(x["id"], x.get("ip", "192.168.0.102"), x["key"], version=float(x.get("version", 3.3)),
                                 connection_timeout=8, connection_retry_limit=3)
def relay():
    st = dev().status()
    if not st or "dps" not in st:
        print(f"tt_power: plug unreachable ({(st or {}).get('Error')}); is a SmartLife app open on the LAN?", file=sys.stderr)
        sys.exit(3)
    return st["dps"]
def want(on):
    (dev().turn_on if on else dev().turn_off)(); time.sleep(2)
    if relay().get("1") is not on:
        print(f"tt_power: relay did not go {'on' if on else 'off'}", file=sys.stderr); sys.exit(4)
if cmd == "status":
    d = relay(); print(f"relay={'on' if d.get('1') else 'off'} power_w={d.get('5', 0) / 10} current_ma={d.get('4')} volts={d.get('6', 0) / 10}")
elif cmd in ("on", "off"):
    want(cmd == "on"); print(f"relay={cmd}")
elif cmd == "cycle":
    want(False); print(f"relay=off ({secs:g} s)", flush=True); time.sleep(secs); want(True); print("relay=on")
else:
    print("usage: tt_power.sh status|on|off|cycle [off_seconds]", file=sys.stderr); sys.exit(2)
PY
