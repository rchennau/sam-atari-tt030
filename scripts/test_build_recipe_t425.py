"""build_recipe T425 path (tt030-t425-kernel-ports FR-3), on the t4demo fixture: the package links
libt4call, ships its .btl, and a separate -t425-on RPM holds the enable file. Needs the m68k cross
toolchain and the INMOS toolset (fractal); skipped elsewhere."""
import hashlib
import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(__file__))
import build_recipe as B  # noqa: E402
import rpm_header as R  # noqa: E402

FX = os.path.join(B.REPO, "recipes", "fixtures", "t4demo-1.0.tar.gz")


@pytest.mark.skipif(not os.path.exists(os.path.join(B.XBIN, f"{B.T}-gcc"))
                    or not os.path.isdir(os.path.join(B.REPO, "tools", "d72uni")), reason="needs cross + INMOS tools")
def test_t425_recipe_builds_package_btl_and_held_switch(tmp_path, monkeypatch):
    sha = open(FX + ".sha256").read().strip()
    assert hashlib.sha256(open(FX, "rb").read()).hexdigest() == sha        # fixture pinned like a recipe
    rec = {"version": "1.0", "url": "file://" + FX, "sha256": sha, "summary": "fixture",
           "t425": {"server_build": "sam-yum-tt/t4compress.sh", "server_out": "compserv.b4h",
                    "btl": "t4demo.btl"}}
    monkeypatch.setattr(B, "recipes", lambda: {"t4demo": rec})
    cached = os.path.join(B.REPO, "tools", "src", "t4demo-1.0.tar.gz")
    if os.path.exists(cached):               # fetch() caches by file name; a stale fixture fails the pin
        os.remove(cached)
    rpm, bins = B.build("t4demo", str(tmp_path), timeout=900, progress=lambda m: None)
    _, m, _ = R.rpm_header(open(rpm, "rb").read())
    assert R.file_paths(m) == ["/usr/bin/t4demo", "/usr/lib/t425/t4demo.btl"]
    assert "/usr/bin/t4demo" in bins                                        # an m68k binary, stripped
    on = tmp_path / "t4demo-t425-on-1.0-1.m68kmint.rpm"
    _, m, _ = R.rpm_header(on.read_bytes())
    assert R.file_paths(m) == ["/etc/t425/enabled/t4demo"] and R.requires(m) == ["t4demo"]
