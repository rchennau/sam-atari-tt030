"""NFR-2 + Phase-1 host checks for rpm_header.py / tt_rpm.py.  Run: pytest scripts/test_tt_rpm.py"""
import gzip
import os
import shutil
import sys

import pytest

sys.path.insert(0, os.path.dirname(__file__))
import rpm_header as R  # noqa: E402
import tt_rpm  # noqa: E402

POOL = os.path.join(os.path.dirname(__file__), "..", "staging", "SPAREMINT")

# Independent reference: captured 2026-09-21 with the `rpmfile` PyPI parser, not this code.
GOLDEN = {
    'bash-2.05a-3.m68kmint.rpm': ('bash', '2.05a', ['/bin/sh']),
    'fileutils-4.1-2.m68kmint.rpm': ('fileutils', '4.1', ['/sbin/install-info', '/bin/sh']),
    'findutils-4.1.7-1.m68kmint.rpm': ('findutils', '4.1.7', ['/sbin/install-info', '/bin/sh']),
    'ftp-server-0.4-1.m68kmint.rpm': ('ftp-server', '0.4', ['freemint-net', 'netbase', 'inetd']),
    'grep-2.4.2-1.m68kmint.rpm': ('grep', '2.4.2', ['/sbin/install-info', '/bin/sh']),
    'gzip-1.3-1.m68kmint.rpm': ('gzip', '1.3', ['/sbin/install-info', 'mktemp', '/bin/sh']),
    'inetd-0.17-5.m68kmint.rpm': ('inetd', '0.17', ['freemint-net', 'portmap', 'tcp_wrappers', '/bin/sh']),
    'libtermcap-2.0.8-4.m68kmint.rpm': ('libtermcap', '2.0.8', ['/etc/termcap', '/bin/sh']),
    'mintbin-0.3-4.m68kmint.rpm': ('mintbin', '0.3', ['/bin/sh']),
    'mintlib-0.59.1-1.m68kmint.rpm': ('mintlib', '0.59.1', ['/bin/sh']),
    'ncurses-5.1-1.m68kmint.rpm': ('ncurses', '5.1', []),
    'nlogin-0.1.2-1.m68kmint.rpm': ('nlogin', '0.1.2', []),
}


@pytest.mark.parametrize("fname", sorted(GOLDEN))
def test_header_matches_independent_parser(fname):
    _, m, _ = R.rpm_header(open(os.path.join(POOL, fname), "rb").read())
    assert (m[R.NAME], m[R.VERSION], R.requires(m)) == GOLDEN[fname]


@pytest.mark.parametrize("damage", [
    lambda d: b"XXXX" + d[4:],                 # lead magic
    lambda d: d[:96] + b"\0" * 4 + d[100:],    # signature header magic
    lambda d: d[:200],                         # truncated inside the headers
])
def test_malformed_rejected(damage, tmp_path):
    good = open(os.path.join(POOL, "gzip-1.3-1.m68kmint.rpm"), "rb").read()
    with pytest.raises(R.BadRpm):
        R.rpm_header(damage(good))
    # and the index refuses to write rather than shrinking silently
    (tmp_path / "m68kmint").mkdir()
    (tmp_path / "m68kmint" / "bad.rpm").write_bytes(damage(good))
    assert tt_rpm.cmd_index(str(tmp_path)) == 1
    assert not (tmp_path / "index.tsv").exists()


def test_index_fields_and_file_provides(tmp_path, monkeypatch):
    import subprocess
    shutil.copytree(POOL, tmp_path / "m68kmint", ignore=shutil.ignore_patterns("bootstrap"))
    monkeypatch.setattr(tt_rpm, "SIGN_KEY", str(tmp_path / "none.key"))
    assert tt_rpm.cmd_index(str(tmp_path)) == 1 and not (tmp_path / "index.tsv").exists()  # unsigned: refuse
    key = tmp_path / "k.pem"
    subprocess.run(["openssl", "genrsa", "-out", str(key), "2048"], check=True, capture_output=True)
    monkeypatch.setattr(tt_rpm, "SIGN_KEY", str(key))
    assert tt_rpm.cmd_index(str(tmp_path)) == 0
    pub = subprocess.run(["openssl", "rsa", "-in", str(key), "-pubout"], capture_output=True, check=True).stdout
    (tmp_path / "k.pub").write_bytes(pub)
    v = subprocess.run(["openssl", "dgst", "-sha256", "-verify", str(tmp_path / "k.pub"), "-signature",
                        str(tmp_path / "index.tsv.sig"), str(tmp_path / "index.tsv")], capture_output=True)
    assert v.returncode == 0                                   # the command the TT runs accepts it
    rows = {line.split("\t")[0]: line.split("\t")
            for line in (tmp_path / "index.tsv").read_text().splitlines()}
    assert all(len(r) == 9 for r in rows.values())
    assert rows["gzip"][1:4] == ["1.3", "1", "m68kmint"]
    assert "/bin/sh" in rows["bash"][8].split(",")      # file-provide, required by many
    assert "/usr/bin/gzip" not in rows["gzip"][8]       # nobody requires it: bounded out


def test_rpmvercmp_and_tiebreak():
    assert tt_rpm.rpmvercmp("2.05a", "2.05") == 1
    assert tt_rpm.rpmvercmp("1.10", "1.9") == 1
    assert tt_rpm.rpmvercmp("1.0", "1.0a") == -1
    assert tt_rpm.rpmvercmp("5.6p1", "5.6p1") == 0
    up = dict(version="1.0", release="1", pool="m68kmint")
    assert tt_rpm.newer(dict(up, pool="built"), up)
    assert not tt_rpm.newer(dict(up, pool="built"), dict(up, release="2"))


def test_writer_roundtrip():
    data = tt_rpm.synth_gate_rpm()
    _, m, off = R.rpm_header(data)
    assert (m[R.NAME], m[R.ARCH]) == ("samgate", "m68kmint")
    assert m[R.RPMVERSION] == "3.0.6"                          # else rpm -V uses broken MD5
    assert data[4:10] == b"\x03\x00\x00\x00\x00\x0d"          # SpareMiNT lead: v3, arch 13
    modes = dict(zip(R.file_paths(m), m[R.FILEMODES]))
    assert modes["/usr/share/samgate/run.sh"] == 0o100750
    assert modes["/usr/share/samgate/link"] == 0o160777
    cpio = gzip.decompress(data[off:])
    assert b"usr/share/samgate/link\0" in cpio and b"./usr" not in cpio


def test_sync_verifies_prunes_and_spares(tmp_path):
    good, other = b"rpm-a", b"rpm-b"
    entries = [
        {"path": "sparemint/RPMS/m68kmint/a-1-1.m68kmint.rpm", "sha": tt_rpm.git_blob_sha1(good)},
        {"path": "sparemint/RPMS/noarch/b-1-1.noarch.rpm", "sha": "0" * 40},   # corrupt download
        {"path": "sparemint/SRPMS/a-1-1.src.rpm", "sha": "x"},                   # not a binary pool
    ]
    for pool, name in [("m68kmint", "gone-1-1.m68kmint.rpm"), ("m68kmint", "bash-2.05a-3.m68kmint.rpm"),
                       ("built", "mine-1-1.m68kmint.rpm")]:
        (tmp_path / pool).mkdir(exist_ok=True)
        (tmp_path / pool / name).write_bytes(b"old")
    fetched = []
    fetch = lambda url: fetched.append(url) or (good if "a-1-1" in url else other)  # noqa: E731
    added, pruned, bad = tt_rpm.sync(str(tmp_path), entries, fetch, {"bash-2.05a-3.m68kmint.rpm"})
    assert (added, pruned) == (1, 1) and bad == ["noarch/b-1-1.noarch.rpm: git blob sha mismatch"]
    assert (tmp_path / "m68kmint" / "a-1-1.m68kmint.rpm").read_bytes() == good
    assert not (tmp_path / "m68kmint" / "gone-1-1.m68kmint.rpm").exists()      # upstream dropped it
    assert (tmp_path / "m68kmint" / "bash-2.05a-3.m68kmint.rpm").exists()      # pinned base-29
    assert (tmp_path / "built" / "mine-1-1.m68kmint.rpm").exists()             # built/ untouched
    assert not (tmp_path / "noarch" / "b-1-1.noarch.rpm").exists()
    assert tt_rpm.sync(str(tmp_path), entries[:1], fetch, set())[0] == 0        # present + sha ok: no refetch


def test_resolve_awk_closure(tmp_path):
    """sam-yum-tt/src/resolve.awk, run by the host awk exactly as the TT runs it."""
    import subprocess
    row = lambda n, req, prov="": "\t".join([n, "1", "1", "m68kmint", f"m68kmint/{n}.rpm", "10",  # noqa: E731
                                             "s" + n, req, prov or n]) + "\n"
    (tmp_path / "index.tsv").write_text(row("app", "libx,/bin/sh,base") + row("libx", "liby")
                                         + row("liby", "") + row("base", "") + row("orphan", "ghost"))
    (tmp_path / "inst").write_text("base\n")
    awk = os.path.join(os.path.dirname(__file__), "..", "sam-yum-tt", "src", "resolve.awk")

    def plan(want):
        r = subprocess.run(["awk", "-F\t", "-v", f"WANT={want}", "-f", awk, str(tmp_path / "inst"),
                            str(tmp_path / "index.tsv")], capture_output=True, text=True, check=True)
        return sorted(r.stdout.split("\n")[:-1])
    assert plan("app") == ["GET app m68kmint/app.rpm sapp 10", "GET libx m68kmint/libx.rpm slibx 10",
                           "GET liby m68kmint/liby.rpm sliby 10"]   # transitive; base installed; /bin/sh on disk
    assert plan("base") == []                                        # already installed: nothing to do
    assert plan("nope") == ["NOTFOUND nope"]
    assert "UNRESOLVED ghost orphan" in plan("orphan")
