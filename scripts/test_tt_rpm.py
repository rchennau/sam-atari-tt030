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


def test_index_fields_and_file_provides(tmp_path):
    shutil.copytree(POOL, tmp_path / "m68kmint", ignore=shutil.ignore_patterns("bootstrap"))
    assert tt_rpm.cmd_index(str(tmp_path)) == 0
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
