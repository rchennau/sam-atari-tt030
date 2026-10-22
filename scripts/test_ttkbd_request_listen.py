"""FR-13 listener: only a fresh, unseen, correctly signed ttkbd-start request is accepted.  pytest scripts/test_ttkbd_request_listen.py"""
import json
import os
import sys

import pytest
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat

sys.path.insert(0, os.path.dirname(__file__))
import ttkbd_request_listen as L  # noqa: E402

KEY = Ed25519PrivateKey.from_private_bytes(bytes(range(32)))
PUB = KEY.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw).hex()


def req(op="ttkbd-start", rid="kbd-100-7", ts=1000, key=KEY):
    body = json.dumps({"op": op, "request_id": rid, "ts": ts})
    return body + "|" + key.sign(body.encode()).hex()


def test_valid_once_then_replay_refused():
    seen = set()
    assert L.authorize(req(), seen, PUB, now=1010) == "kbd-100-7"
    with pytest.raises(ValueError, match="replayed"):
        L.authorize(req(), seen, PUB, now=1010)


@pytest.mark.parametrize("line,why", [
    (req(key=Ed25519PrivateKey.generate()), "bad signature"),
    (req()[:-2] + "00", "bad signature"),
    (req().rpartition("|")[0], "unsigned"),
    (req(op="build"), "not a keyboard request"),
    (req(ts=500), "stale"),
    (req(rid="../x"), "invalid id"),
])
def test_refusals(line, why):
    with pytest.raises(ValueError, match=why):
        L.authorize(line, set(), PUB, now=1010)


def test_shipped_key_is_the_cards():
    assert len(open(L.PUB).read().strip()) == 64
