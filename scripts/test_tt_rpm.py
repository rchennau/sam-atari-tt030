"""NFR-2 + Phase-1 host checks for rpm_header.py / tt_rpm.py.  Run: pytest scripts/test_tt_rpm.py"""
import gzip
import hashlib
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
    signed = (tmp_path / "index.signed").read_bytes()          # split exactly as the TT client does
    (tmp_path / "s.sig").write_bytes(signed[:256])
    (tmp_path / "s.tsv").write_bytes(signed[256:])
    verify = ["openssl", "dgst", "-sha256", "-verify", str(tmp_path / "k.pub"), "-signature",
              str(tmp_path / "s.sig"), str(tmp_path / "s.tsv")]
    assert subprocess.run(verify, capture_output=True).returncode == 0
    assert signed[256:] == (tmp_path / "index.tsv").read_bytes()
    serial = int(signed[256:].split(b"\n")[0].split(b"\t")[1])
    assert tt_rpm.cmd_index(str(tmp_path)) == 0                # re-index: serial strictly increases
    assert int((tmp_path / "index.tsv").read_text().split("\n")[0].split("\t")[1]) > serial
    body = bytearray(signed[256:])
    body[-2] ^= 1                                              # tampered body: one bit flipped
    (tmp_path / "s.tsv").write_bytes(bytes(body))
    assert subprocess.run(verify, capture_output=True).returncode != 0
    rows = {line.split("\t")[0]: line.split("\t")
            for line in (tmp_path / "index.tsv").read_text().splitlines() if not line.startswith("#")}
    assert all(len(r) == 10 for r in rows.values())
    assert "gzip" in rows["gzip"][9].split(",")          # commands column: /bin, /usr/bin, /sbin basenames
    assert rows["gzip"][1:4] == ["1.3", "1", "m68kmint"]
    assert "/bin/sh" in rows["bash"][8].split(",")      # file-provide, required by many
    assert "/usr/bin/gzip" not in rows["gzip"][8]       # nobody requires it: bounded out


def test_add_rejects_malformed_and_publishes_good(tmp_path, monkeypatch):
    import subprocess
    key = tmp_path / "k.pem"
    subprocess.run(["openssl", "genrsa", "-out", str(key), "2048"], check=True, capture_output=True)
    monkeypatch.setattr(tt_rpm, "SIGN_KEY", str(key))
    bad = tmp_path / "bad.rpm"
    bad.write_bytes(b"not an rpm")
    assert tt_rpm.cmd_add(str(tmp_path / "m"), str(bad)) == 1
    good = tmp_path / "samgate-1.0-1.m68kmint.rpm"
    good.write_bytes(tt_rpm.synth_gate_rpm())
    assert tt_rpm.cmd_add(str(tmp_path / "m"), str(good)) == 0
    assert "\tbuilt/samgate-1.0-1.m68kmint.rpm\t" in (tmp_path / "m" / "index.tsv").read_text()


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


def test_scan_cache_reuses_unchanged_and_rereads_changed(tmp_path, monkeypatch):
    shutil.copytree(POOL, tmp_path / "m68kmint", ignore=shutil.ignore_patterns("bootstrap"))
    first, _ = tt_rpm.scan(str(tmp_path))
    reads = []
    real_open = open
    monkeypatch.setattr("builtins.open", lambda p, *a, **k: (reads.append(str(p)), real_open(p, *a, **k))[1])
    again, _ = tt_rpm.scan(str(tmp_path))
    assert again == first and not [r for r in reads if r.endswith(".rpm")]    # all from cache
    victim = tmp_path / "m68kmint" / "gzip-1.3-1.m68kmint.rpm"
    victim.write_bytes(victim.read_bytes() + b"\0")                         # size changes
    reads.clear()
    third, _ = tt_rpm.scan(str(tmp_path))
    assert [r for r in reads if r.endswith(".rpm")] == [str(victim)]
    assert [p["sha256"] for p in third if p["name"] == "gzip"] != [p["sha256"] for p in first if p["name"] == "gzip"]
    assert oct(os.stat(tmp_path / ".scan-cache.json").st_mode & 0o777) == "0o644"


def test_rpm_va_classify_expected_real_and_depgaps():
    import rpm_va_classify as C
    rows = C.classify("\n".join([
        "1790054354",                                               # start stamp is ignored
        "......G.   /usr/bin/gzip",
        "S.5....T c /etc/inetd.conf",                               # present in HD10_OVERLAY
        ".......T   /usr/sbin/syslogd",
        "missing    /usr/bin/rdate",
        ".M.....T c /etc/syslog.conf",                              # not in the overlay: real
        "Unsatisfied dependencies for gzip-1.3-1: /sbin/install-info  , mktemp",
        "garbage line",
    ]))
    verdicts = [(v, r) for v, r, _, _ in rows]
    assert verdicts == [("EXPECTED", "group-wheel"), ("EXPECTED", "overlay-config"), ("EXPECTED", "mtime-only"),
                        ("REAL", "missing"), ("REAL", ".M.....T"), ("DEPGAP", "gzip-1.3-1"), ("REAL", "unparsed")]


def test_push_verifies_sha_on_the_tt_before_rpm_i(tmp_path):
    (tmp_path / "built").mkdir()
    rpm = tmp_path / "built" / "x-1-1.m68kmint.rpm"
    rpm.write_bytes(b"payload")
    good = hashlib.sha256(b"payload").hexdigest()
    (tmp_path / "index.tsv").write_text(f"#serial\t1\nx\t1\t1\tm68kmint\tbuilt/x-1-1.m68kmint.rpm\t7\t{good}\t\tx\n")
    calls = []

    def tt(corrupt=False):
        def run(host, cmd, stdin=None):
            calls.append(cmd)
            if cmd.startswith("cat >"):
                d = hashlib.sha256(stdin + (b"!" if corrupt else b"")).hexdigest()
                return 0, f"SHA256(/tmp/push-x-1-1.m68kmint.rpm)= {d}\n"
            return 0, ""
        return run
    assert tt_rpm.cmd_push(str(tmp_path), ["x"], "h", run=tt()) == 0
    assert "rpm -i /tmp/push-x-1-1.m68kmint.rpm" in calls[-1]
    calls.clear()
    assert tt_rpm.cmd_push(str(tmp_path), ["x"], "h", run=tt(corrupt=True)) == 1       # bad transfer
    assert not any("rpm -i" in c for c in calls)                                      # never installed
    assert tt_rpm.cmd_push(str(tmp_path), ["nope"], "h", run=tt()) == 1                # not on mirror


def test_update_awk_offers_only_newer(tmp_path):
    import subprocess
    (tmp_path / "iv").write_text("less\t457\t1\npv\t1.7.24\t1\nmawk\t1.3.4\t20260302\n")
    (tmp_path / "ix").write_text("#serial\t1\n" + "".join(
        f"{n}\t{v}\t{r}\tm\t{p}\t9\t{s}\t\t{n}\n" for n, v, r, p, s in [
            ("less", "458", "1", "m/less.rpm", "s1"), ("pv", "1.7.24", "1", "b/pv.rpm", "s2"),
            ("mawk", "1.3.4", "20260301", "b/m.rpm", "s3"), ("vim", "6", "1", "m/v.rpm", "s4")]))
    awk = os.path.join(os.path.dirname(__file__), "..", "sam-yum-tt", "src", "update.awk")
    run = lambda only="": subprocess.run(["awk", "-F\t", "-v", f"ONLY={only}", "-f", awk, str(tmp_path / "iv"),  # noqa: E731
                                          str(tmp_path / "ix")], capture_output=True, text=True, check=True).stdout
    assert run() == "GET less m/less.rpm s1 9\n"     # newer only; equal, older and not-installed skipped
    assert run("pv") == ""


def test_near_awk_and_yum_install_lists_near_matches(tmp_path):
    """`yum install top` found nothing and asked for a build (2026-09-23); top ships in pstop."""
    import subprocess
    src = os.path.join(os.path.dirname(__file__), "..", "sam-yum-tt", "src")
    row = lambda n, cmds="", prov="": "\t".join([n, "1", "1", "m68kmint", f"m68kmint/{n}.rpm", "10",  # noqa: E731
                                                 "s" + n, "", prov or n, cmds]) + "\n"
    ix = tmp_path / "index.tsv"
    ix.write_text("#serial\t1\n" + row("less", "less,lessecho") + row("pstop", "ps,top") + row("sed", "sed")
                  + row("vim", "vi,vim") + row("vim-minimal", "vi") + row("fileutils", "ls,cp"))

    def near(want):
        return subprocess.run(["awk", "-F\t", "-v", f"WANT={want}", "-f", os.path.join(src, "near.awk"), str(ix)],
                              capture_output=True, text=True, check=True).stdout.splitlines()
    assert near("top") == ["NEAR top pstop 1-1 cmd"]
    assert near("vi")[:2] == ["NEAR vi vim 1-1 cmd", "NEAR vi vim-minimal 1-1 cmd"]
    assert near("lses")[0] == "NEAR lses less 1-1 spelling"
    assert near("ls") == ["NEAR ls fileutils 1-1 cmd"]       # 2 chars: no substring noise
    assert near("xyzzy") == []                                # nothing near: caller builds
    # the client, on the host: rpm stubbed, no network (the index is fresh, so no makecache)
    bin_ = tmp_path / "bin"
    bin_.mkdir()
    (bin_ / "rpm").write_text("#!/bin/sh\nexit 0\n")
    (bin_ / "rpm").chmod(0o755)
    (bin_ / "logger").write_text(f'#!/bin/sh\necho "$@" >> {tmp_path}/syslog\n')   # records what yum logs
    (bin_ / "logger").chmod(0o755)
    env = dict(os.environ, CACHE=str(tmp_path), LIB=src, PATH=f"{bin_}:{os.environ['PATH']}")
    r = subprocess.run(["bash", os.path.join(src, "yum"), "install", "top"], env=env, capture_output=True, text=True)
    assert r.returncode == 1
    assert 'no package "top"' in r.stdout and "pstop" in r.stdout and "provides top" in r.stdout
    assert "yum install --build top'" in r.stdout
    assert "requesting a build" not in r.stdout + r.stderr    # no build asked for
    logged = (tmp_path / "syslog").read_text()
    assert "-p user.notice" in logged and "not on the mirror: top; near matches: pstop" in logged
    (bin_ / "logger").unlink()                                # no logger: yum still works, says so
    for tool in ("awk", "sed", "grep", "find", "tr", "cat", "rm", "mkdir", "wc", "head", "tail"):
        (bin_ / tool).symlink_to(shutil.which(tool))          # PATH without the host's real logger
    r = subprocess.run([shutil.which("bash"), os.path.join(src, "yum"), "install", "top"], env=dict(env, PATH=str(bin_)),
                       capture_output=True, text=True)
    assert r.returncode == 1 and "not logged: no logger" in r.stderr
