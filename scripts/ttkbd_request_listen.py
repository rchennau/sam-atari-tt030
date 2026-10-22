#!/usr/bin/env python3
"""ttkbd_request_listen.py — start the keyboard + mouse session when the TT asks (tt030-remote-keyboard FR-13).

Runs in the operator's desktop session (systemd user unit tt-kbd-request.service, iac/fractal), so the session it
starts can grab this host's keyboard and mouse exactly like the launcher icon. Listens on sam/tt030/kbd/request
(mosquitto_sub) for `<json>|<ed25519 sig hex>` from the TT's ttkbd-request: json = {"op":"ttkbd-start",
"request_id","ts"}, signed with the card's request key (public half: sam-kbd-tt/tt-request.pub).

The estate broker is anonymous, so a request is acted on only if: the signature verifies, op is ttkbd-start, it is
at most MAX_AGE s old (or ahead), and its request_id was not seen before. Then `tt-input-toggle.sh --start`, which
does nothing if a session is already running and refuses while the TT's serial line is busy.
ponytail: seen ids live in memory; a replay within MAX_AGE of a listener restart would start one session (the
operator can stop it with the chord or the TT's corner). Persist them if that ever matters.
Exits when mosquitto_sub ends; the unit restarts it.
"""
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PUB = os.path.join(HERE, "..", "sam-kbd-tt", "tt-request.pub")
TOPIC = "sam/tt030/kbd/request"
MAX_AGE = 120
ID_RE = re.compile(r"^kbd-[0-9]+-[0-9]+$")


def authorize(line, seen, pub_hex, now=None):
    """Return the request_id of a valid start request; raise ValueError naming why not."""
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
    body, sep, sig = line.strip().rpartition("|")
    if not sep or len(sig) != 128:
        raise ValueError("unsigned")
    try:
        Ed25519PublicKey.from_public_bytes(bytes.fromhex(pub_hex)).verify(bytes.fromhex(sig), body.encode())
    except (InvalidSignature, ValueError):
        raise ValueError("bad signature") from None
    req = json.loads(body)
    rid = str(req.get("request_id", ""))
    if req.get("op") != "ttkbd-start":
        raise ValueError("not a keyboard request")
    if abs((now or time.time()) - int(req["ts"])) > MAX_AGE:
        raise ValueError("stale")
    if not ID_RE.match(rid):
        raise ValueError("invalid id")
    if rid in seen:
        raise ValueError("replayed")
    seen.add(rid)
    return rid


def main():
    pub_hex = open(PUB).read().strip()          # missing key: crash, the unit logs it (fail closed)
    seen = set()
    sub = subprocess.Popen(["mosquitto_sub", "-h", os.environ.get("MQTT_HOST", "mqtt.sam.int"), "-t", TOPIC],
                           stdout=subprocess.PIPE, text=True)
    print(f"ttkbd_request_listen: listening on {TOPIC}", flush=True)
    for line in sub.stdout:
        try:
            rid = authorize(line, seen, pub_hex)
        except (ValueError, KeyError, json.JSONDecodeError) as e:
            print(f"ttkbd_request_listen: refused: {e}", flush=True)
            continue
        print(f"ttkbd_request_listen: {rid} accepted, starting the session", flush=True)
        subprocess.Popen(["bash", os.path.join(HERE, "tt-input-toggle.sh"), "--start"], start_new_session=True)
    return sub.wait() or 1


if __name__ == "__main__":
    sys.exit(main())
