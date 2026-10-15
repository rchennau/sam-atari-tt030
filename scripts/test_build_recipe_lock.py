"""F104: two builds of one package (ttbuildd + ci.sh) never share the fixed work dir.  pytest scripts/test_build_recipe_lock.py"""
import os
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(__file__))
import build_recipe as B  # noqa: E402


def test_same_package_builds_serialise(monkeypatch, tmp_path):
    monkeypatch.setattr(B.tempfile, "gettempdir", lambda: str(tmp_path))
    inside, overlap = [], []

    def fake(name, out_dir, timeout, progress, work):
        overlap.append(len(inside))
        inside.append(1)
        time.sleep(0.2)
        inside.pop()

    monkeypatch.setattr(B, "_build", fake)
    ts = [threading.Thread(target=B.build, args=("pkg", "out")) for _ in range(2)]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    assert overlap == [0, 0]
