#!/usr/bin/env python3
"""ttbuildd — build-on-miss for the TT (tt030-rpm-pipeline FR-4). Runs on fractal as sam.

Subscribes sam/tt030/build/request and answers on
sam/tt030/build/{progress,done,failed,queued}/<request_id> with a short plain-text payload, so the
TT never parses JSON:
  progress  accepted | waiting | configure | make | package | publish
  done      on-mirror | built          -> the client re-reads the index and installs
  failed    <one line why>             -> build broke, or exceeded BUILD_TIMEOUT
  queued    no-recipe                  -> appended to tt030/queue.jsonl + a board card (deduplicated)
One build at a time (fractal is the operator's desktop); BUILD_TIMEOUT is under the client's 15-min
wait, so a slow build answers `failed`, never a silent client timeout. Every RPM it writes is
recorded in tt030/built/provenance.jsonl.

A request is `<json>|<ed25519 signature, hex>`, json = {"request_id","pkg","ts"}, signed on the TT by
`ttsign` with a key only the card holds (NFR-4: the estate broker is anonymous, and classic
mosquitto ACLs cannot deny one topic, so authorization lives here). Unsigned, bad-signature,
older than MAX_AGE, or replayed ids are refused with `failed unauthorized`. No public key on disk
means every request is refused (fail closed).
"""
import json
import os
import queue
import re
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_recipe  # noqa: E402
import tt_rpm  # noqa: E402

MIRROR = os.environ.get("TT030_MIRROR", "/mnt/vault/tt030")
REQUEST_PUB = os.path.expanduser("~/.config/tt030-mirror/tt-request.pub")   # 64 hex digits
MAX_AGE = 600
BUILD_TIMEOUT = 600
ID_RE = re.compile(r"^[A-Za-z0-9-]{1,64}$")
PKG_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9+._-]{0,63}$")
REQUEST = "sam/tt030/build/request"


def authorize(payload, seen, now=None, pub_hex=None):
    """Return (request_id, pkg) for a validly signed, fresh, unseen request; else raise ValueError."""
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
    text = payload.decode("ascii", errors="strict") if isinstance(payload, bytes) else payload
    body, sep, sig = text.rpartition("|")
    if not sep or len(sig) != 128:
        raise ValueError("unsigned")
    if pub_hex is None:
        try:
            pub_hex = open(REQUEST_PUB).read().strip()
        except OSError:
            raise ValueError("no TT public key on this host") from None
    try:
        Ed25519PublicKey.from_public_bytes(bytes.fromhex(pub_hex)).verify(bytes.fromhex(sig), body.encode())
    except (InvalidSignature, ValueError):
        raise ValueError("bad signature") from None
    req = json.loads(body)
    rid, pkg, ts = str(req["request_id"]), str(req["pkg"]), int(req["ts"])
    if abs((now or time.time()) - ts) > MAX_AGE:
        raise ValueError("stale")
    if rid in seen:
        raise ValueError("replayed")
    if not ID_RE.match(rid) or not PKG_RE.match(pkg):
        raise ValueError("invalid id/pkg")
    seen.add(rid)
    return rid, pkg


class SeenIds(set):
    """request_ids already accepted, persisted so a restart cannot re-open a replay window.
    Entries older than MAX_AGE are dropped on load: past that, `authorize` rejects on age anyway."""

    def __init__(self, path, now=None):
        super().__init__()
        self.path, now = path, now or time.time()
        try:
            for line in open(path):
                rid, ts = line.split()
                if now - float(ts) <= MAX_AGE:
                    super().add(rid)
        except (OSError, ValueError):
            pass
        with open(path, "w") as fh:                    # rewrite pruned
            fh.writelines(f"{r} {now:.0f}\n" for r in self)

    def add(self, rid):
        super().add(rid)
        with open(self.path, "a") as fh:
            fh.write(f"{rid} {time.time():.0f}\n")


def on_mirror(pkg):
    try:
        with open(os.path.join(MIRROR, "index.tsv")) as fh:
            return any(line.split("\t", 1)[0] == pkg for line in fh if not line.startswith("#"))
    except OSError:
        return False


def enqueue_for_human(pkg, rid, file_card):
    """Append to queue.jsonl unless an open entry exists; file one board card per package.
    Returns the card error, or None: a card that failed must reach the TT, not only stderr."""
    path = os.path.join(MIRROR, "queue.jsonl")
    open_entries = []
    if os.path.exists(path):
        open_entries = [json.loads(line) for line in open(path) if line.strip()]
    if any(e["pkg"] == pkg and e.get("state") == "open" for e in open_entries):
        return None
    with open(path, "a") as fh:
        fh.write(json.dumps({"pkg": pkg, "state": "open", "first_request_id": rid,
                             "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}) + "\n")
    return file_card(pkg, rid)


def board_card(pkg, rid):
    try:
        from sam.board import api
        api.create(type="todo", title=f"TT030: write a build recipe for '{pkg}' (build-on-miss queued it)",
                   body=f"A TT `yum install {pkg}` found no mirror package and no entry in "
                        f"recipes/source-map.json (request {rid}). Add a pinned source-map entry in "
                        f"~/Projects/atari-tt030-enhancement, then mark the queue.jsonl entry done.",
                   lane="backlog", origin="ttbuildd")
    except Exception as e:  # noqa: BLE001 — the queue entry is the record; a card failure is reported
        print(f"ttbuildd: board card for {pkg} not filed: {e}", file=sys.stderr, flush=True)
        return str(e).strip().splitlines()[0][:80] if str(e).strip() else type(e).__name__
    return None


def handle(pkg, rid, say, build=build_recipe.build, add=tt_rpm.cmd_add, file_card=board_card):
    """One request, end to end. `say(verb, text)` publishes to sam/tt030/build/<verb>/<rid>."""
    if on_mirror(pkg):
        return say("done", "on-mirror")
    if pkg not in build_recipe.recipes():
        err = enqueue_for_human(pkg, rid, file_card)
        # 2026-09-23: the card failed ("board has no home on this node") while the TT was told
        # a plain "queued" — the request looked routed to a human and reached only queue.jsonl.
        return say("queued", f"no-recipe; board card NOT filed: {err}" if err else "no-recipe")
    try:
        rpm, bins = build(pkg, os.path.join(MIRROR, ".ttbuildd-out"), timeout=BUILD_TIMEOUT,
                          progress=lambda step: say("progress", step))
    except Exception as e:  # noqa: BLE001 — every failure must reach the waiting TT, never hang it
        first = str(e).strip().splitlines()[0] if str(e).strip() else type(e).__name__
        return say("failed", first[:200])
    say("progress", "publish")
    if add(MIRROR, rpm):
        return say("failed", "tt_rpm add/index failed")
    with open(os.path.join(MIRROR, "built", "provenance.jsonl"), "a") as fh:
        fh.write(json.dumps({"rpm": os.path.basename(rpm), "request_id": rid, "binaries": bins,
                             "recipe": build_recipe.recipes()[pkg],
                             "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}) + "\n")
    os.remove(rpm)
    say("done", "built")


def main():
    from sam.utils import mqtt as sm
    work = queue.Queue()
    c = sm.client(client_id=f"ttbuildd-{os.getpid()}")

    def say(verb, rid, text):
        c.publish(f"sam/tt030/build/{verb}/{rid}", text, qos=1)
        print(f"ttbuildd: {rid} {verb} {text}", flush=True)

    def on_connect(client, *_):
        client.subscribe(REQUEST, qos=1)

    seen = SeenIds(os.path.join(MIRROR, ".ttbuildd-seen"))

    def on_message(_c, _u, msg):
        try:
            rid, pkg = authorize(msg.payload, seen)
        except (ValueError, KeyError, TypeError) as e:
            print(f"ttbuildd: refused request ({e}): {msg.payload[:120]!r}", file=sys.stderr, flush=True)
            # answer the id if one can be read, so a legitimate but misconfigured TT is told why
            m = re.search(r'"request_id"\s*:\s*"([A-Za-z0-9-]{1,64})"', msg.payload.decode("ascii", "replace"))
            if m:
                say("failed", m.group(1), "unauthorized")
            return
        say("progress", rid, "waiting" if work.qsize() or busy.is_set() else "accepted")
        work.put((pkg, rid))

    busy = threading.Event()
    c.on_connect = on_connect
    c.on_message = on_message
    c.connect(sm.default_broker(), 1883, keepalive=60)
    c.loop_start()
    while True:
        pkg, rid = work.get()
        busy.set()
        try:
            handle(pkg, rid, lambda verb, text: say(verb, rid, text))
        finally:
            busy.clear()


if __name__ == "__main__":
    main()
