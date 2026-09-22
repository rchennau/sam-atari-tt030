"""FR-4 ttbuildd.handle: every request ends in exactly one terminal verb.  pytest scripts/test_ttbuildd.py"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import ttbuildd  # noqa: E402


def run(tmp_path, monkeypatch, pkg, build=None, add=None, recipes=("pv",)):
    (tmp_path / "built").mkdir(exist_ok=True)
    (tmp_path / "index.tsv").write_text("#serial\t1\nless\t458\t1\n")
    monkeypatch.setattr(ttbuildd, "MIRROR", str(tmp_path))
    monkeypatch.setattr(ttbuildd.build_recipe, "recipes", lambda: {r: {"version": "1"} for r in recipes})
    said, cards = [], []
    ttbuildd.handle(pkg, "rid1", lambda v, t: said.append((v, t)), build=build, add=add,
                    file_card=lambda p, r: cards.append(p))
    return said, cards


def test_on_mirror_is_done(tmp_path, monkeypatch):
    assert run(tmp_path, monkeypatch, "less")[0] == [("done", "on-mirror")]


def test_no_recipe_queues_once_and_files_one_card(tmp_path, monkeypatch):
    said, cards = run(tmp_path, monkeypatch, "vim")
    assert said == [("queued", "no-recipe")] and cards == ["vim"]
    said, cards = run(tmp_path, monkeypatch, "vim")                  # second request: deduplicated
    assert said == [("queued", "no-recipe")] and cards == []
    assert len((tmp_path / "queue.jsonl").read_text().splitlines()) == 1


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
