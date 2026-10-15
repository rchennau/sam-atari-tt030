"""tt030-yum-gui FR-8: yum-tt writes $CACHE/installed.tsv after every rpm operation, success or failure, atomically.
Runs the real yum script against a fake `rpm` on PATH.  pytest scripts/test_yum_installed_tsv.py"""
import os
import subprocess

YUM = os.path.join(os.path.dirname(__file__), "..", "sam-yum-tt", "src", "yum")
FAKE = """#!/bin/sh
case "$1" in
-qa) printf 'bash\\t2.05a-3\\nless\\t358-1\\n' ;;
-e) [ "$2" = bad ] && exit 1; exit 0 ;;
esac
"""


def _run(tmp_path, *args):
    bin_ = tmp_path / "bin"
    bin_.mkdir(exist_ok=True)
    (bin_ / "rpm").write_text(FAKE)
    (bin_ / "rpm").chmod(0o755)
    env = dict(os.environ, PATH=f"{bin_}:{os.environ['PATH']}", CACHE=str(tmp_path / "cache"))
    return subprocess.run(["bash", YUM, *args], env=env, capture_output=True, text=True)


def test_remove_writes_installed_set(tmp_path):
    assert _run(tmp_path, "remove", "less").returncode == 0
    cache = tmp_path / "cache"
    assert (cache / "installed.tsv").read_text() == "bash\t2.05a-3\nless\t358-1\n"
    assert [p.name for p in cache.iterdir()] == ["installed.tsv"]          # no temp file left behind


def test_failed_rpm_still_refreshes_and_fails(tmp_path):
    r = _run(tmp_path, "remove", "bad")
    assert r.returncode == 1
    assert (tmp_path / "cache" / "installed.tsv").exists()
