"""FR-4 ttbuildd.handle: every request ends in exactly one terminal verb.  pytest scripts/test_ttbuildd.py"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import ttbuildd  # noqa: E402


def run(tmp_path, monkeypatch, pkg, build=None, add=None, recipes=("pv",), card_error=None):
    (tmp_path / "built").mkdir(exist_ok=True)
    (tmp_path / "index.tsv").write_text("#serial\t1\nless\t458\t1\n")
    monkeypatch.setattr(ttbuildd, "MIRROR", str(tmp_path))
    monkeypatch.setattr(ttbuildd.build_recipe, "recipes", lambda: {r: {"version": "1"} for r in recipes})
    said, cards = [], []
    ttbuildd.handle(pkg, "rid1", lambda v, t: said.append((v, t)), build=build, add=add,
                    file_card=lambda p, r: cards.append(p) or card_error)
    return said, cards


def test_on_mirror_is_done(tmp_path, monkeypatch):
    assert run(tmp_path, monkeypatch, "less")[0] == [("done", "on-mirror")]


def test_no_recipe_queues_once_and_files_one_card(tmp_path, monkeypatch):
    said, cards = run(tmp_path, monkeypatch, "vim")
    assert said == [("queued", "no-recipe")] and cards == ["vim"]
    said, cards = run(tmp_path, monkeypatch, "vim")                  # second request: deduplicated
    assert said == [("queued", "no-recipe")] and cards == []
    assert len((tmp_path / "queue.jsonl").read_text().splitlines()) == 1


def test_card_failure_reaches_the_tt(tmp_path, monkeypatch):
    said, _ = run(tmp_path, monkeypatch, "top", card_error="board has no home on this node")
    assert said == [("queued", "no-recipe; board card NOT filed: board has no home on this node")]


def test_build_failure_is_reported_not_hung(tmp_path, monkeypatch):
    def boom(*a, **k):
        raise RuntimeError("`make` failed (rc 2):\nfoo.c: error")
    said, _ = run(tmp_path, monkeypatch, "pv", build=boom)
    assert said == [("failed", "`make` failed (rc 2):")]


def test_build_success_publishes_and_records_provenance(tmp_path, monkeypatch):
    def fake_build(pkg, out, timeout, progress):
        assert timeout < 900                                          # under the client's 15-min wait
        progress("make")
        os.makedirs(out, exist_ok=True)
        p = os.path.join(out, "pv-1-1.m68kmint.rpm")
        open(p, "wb").write(b"x")
        return p, {"/usr/bin/pv": "abc"}
    added = []
    said, _ = run(tmp_path, monkeypatch, "pv", build=fake_build, add=lambda m, r: added.append(r) or 0)
    assert said == [("progress", "make"), ("progress", "publish"), ("done", "built")]
    assert added and not os.path.exists(added[0])                     # staged RPM cleaned up
    prov = json.loads((tmp_path / "built" / "provenance.jsonl").read_text())
    assert prov["request_id"] == "rid1" and prov["binaries"] == {"/usr/bin/pv": "abc"}


def test_authorize_signed_fresh_unseen_only():
    import pytest
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat
    k = Ed25519PrivateKey.generate()
    pub = k.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw).hex()
    now = 1_000_000

    def req(rid="r1", pkg="pv", ts=now, key=k):
        body = json.dumps({"request_id": rid, "pkg": pkg, "ts": ts})
        return f"{body}|{key.sign(body.encode()).hex()}".encode()
    seen = set()
    assert ttbuildd.authorize(req(), seen, now=now, pub_hex=pub) == ("r1", "pv")
    for bad, why in [(req(), "replayed"), (req("r2", ts=now - 601), "stale"),
                     (req("r3", key=Ed25519PrivateKey.generate()), "bad signature"),
                     (b'{"request_id":"r4","pkg":"pv","ts":1000000}', "unsigned"),
                     (req("r5").replace(b'"pv"', b'"vi"'), "bad signature")]:
        with pytest.raises(ValueError, match=why):
            ttbuildd.authorize(bad, seen, now=now, pub_hex=pub)
    with pytest.raises(ValueError, match="no TT public key"):
        ttbuildd.REQUEST_PUB = "/nonexistent"
        ttbuildd.authorize(req("r6"), seen, now=now)


def test_seen_ids_survive_a_restart_and_age_out(tmp_path):
    p = str(tmp_path / "seen")
    s = ttbuildd.SeenIds(p)
    s.add("a")
    assert "a" in ttbuildd.SeenIds(p)                                         # restart remembers
    assert "a" not in ttbuildd.SeenIds(p, now=__import__("time").time() + 601)  # aged out
