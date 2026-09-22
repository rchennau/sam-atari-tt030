#!/usr/bin/env python3
"""ttbuildd — build-on-miss for the TT (tt030-rpm-pipeline FR-4). Runs on fractal as sam.

Subscribes sam/tt030/build/request ({"request_id": ..., "pkg": ...}) and answers on
sam/tt030/build/{progress,done,failed,queued}/<request_id> with a short plain-text payload, so the
TT never parses JSON:
  progress  accepted | waiting | configure | make | package | publish
  done      on-mirror | built          -> the client re-reads the index and installs
  failed    <one line why>             -> build broke, or exceeded BUILD_TIMEOUT
  queued    no-recipe                  -> appended to tt030/queue.jsonl + a board card (deduplicated)
One build at a time (fractal is the operator's desktop); BUILD_TIMEOUT is under the client's 15-min
wait, so a slow build answers `failed`, never a silent client timeout. Every RPM it writes is
recorded in tt030/built/provenance.jsonl.
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
BUILD_TIMEOUT = 600
ID_RE = re.compile(r"^[A-Za-z0-9-]{1,64}$")
PKG_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9+._-]{0,63}$")
REQUEST = "sam/tt030/build/request"


def on_mirror(pkg):
    try:
        with open(os.path.join(MIRROR, "index.tsv")) as fh:
            return any(line.split("\t", 1)[0] == pkg for line in fh if not line.startswith("#"))
    except OSError:
        return False


def enqueue_for_human(pkg, rid, file_card):
    """Append to queue.jsonl unless an open entry exists; file one board card per package."""
    path = os.path.join(MIRROR, "queue.jsonl")
    open_entries = []
    if os.path.exists(path):
        open_entries = [json.loads(line) for line in open(path) if line.strip()]
    if any(e["pkg"] == pkg and e.get("state") == "open" for e in open_entries):
        return False
    with open(path, "a") as fh:
        fh.write(json.dumps({"pkg": pkg, "state": "open", "first_request_id": rid,
                             "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}) + "\n")
    file_card(pkg, rid)
    return True


def board_card(pkg, rid):
    try:
        from sam.board import api
        api.create(type="todo", title=f"TT030: write a build recipe for '{pkg}' (build-on-miss queued it)",
                   body=f"A TT `yum install {pkg}` found no mirror package and no entry in "
                        f"recipes/source-map.json (request {rid}). Add a pinned source-map entry in "
                        f"~/Projects/atari-tt030-enhancement, then mark the queue.jsonl entry done.",
                   lane="backlog", origin="ttbuildd")
    except Exception as e:  # noqa: BLE001 — the queue entry is the record; a card failure is logged
        print(f"ttbuildd: board card for {pkg} not filed: {e}", file=sys.stderr, flush=True)


def handle(pkg, rid, say, build=build_recipe.build, add=tt_rpm.cmd_add, file_card=board_card):
    """One request, end to end. `say(verb, text)` publishes to sam/tt030/build/<verb>/<rid>."""
    if on_mirror(pkg):
        return say("done", "on-mirror")
    if pkg not in build_recipe.recipes():
        enqueue_for_human(pkg, rid, file_card)
        return say("queued", "no-recipe")
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

    def on_message(_c, _u, msg):
        try:
            req = json.loads(msg.payload)
            rid, pkg = str(req["request_id"]), str(req["pkg"])
        except (ValueError, KeyError, TypeError):
            print(f"ttbuildd: dropped malformed request {msg.payload[:120]!r}", file=sys.stderr, flush=True)
            return
        if not ID_RE.match(rid) or not PKG_RE.match(pkg):
            print(f"ttbuildd: dropped invalid request id/pkg {rid!r} {pkg!r}", file=sys.stderr, flush=True)
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
