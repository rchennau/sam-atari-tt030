"""tgzip / tbzip2 (sam-yum-tt, Rev. 7) on the host: the real tgzip.c with atwboot_host.c, so every
piece takes the 68030 path. Proves the multi-piece output is what gunzip / bunzip2 read, that the
T425-unavailable fallback is said out loud, and the gzip-style file handling."""
import bz2
import gzip
import os
import subprocess

import pytest

R = os.path.join(os.path.dirname(__file__), "..")
SRC = os.path.join(R, "sam-yum-tt", "src")
Z = os.path.join(R, "tools", "rpm-src", "zlib-1.3.2")
B = os.path.join(R, "tools", "rpm-src", "bzip2-1.0.8")


@pytest.fixture(scope="module")
def tgzip(tmp_path_factory):
    d = tmp_path_factory.mktemp("tgzip")
    if not (os.path.isdir(Z) and os.path.isdir(B)):
        pytest.skip("zlib/bzip2 sources not unpacked under tools/rpm-src")
    srcs = [os.path.join(SRC, f) for f in ("tgzip.c", "t4call.c", "t4lock.c", "compkern.c", "atwboot_host.c")]
    srcs += [os.path.join(Z, f) for f in ("adler32.c", "crc32.c", "deflate.c", "trees.c", "zutil.c")]
    srcs += [os.path.join(B, f) for f in ("blocksort.c", "huffman.c", "crctable.c", "randtable.c",
                                          "decompress.c", "compress.c", "bzlib.c")]
    subprocess.run(["cc", "-O2", "-w", "-DZ_SOLO", "-DBZ_NO_STDIO", f'-DT4LOCK="{d}/t425.lock"', f'-DT4LOADED="{d}/t425.loaded"',
                    f"-I{SRC}", f"-I{Z}", f"-I{B}",
                    "-o", str(d / "tgzip"), *srcs], check=True)
    os.symlink("tgzip", d / "tbzip2")
    return d


def test_pieces_are_one_valid_file_and_fallback_is_said(tgzip, tmp_path):
    data = b"".join(b"line %d of a text-like input\n" % i for i in range(40000))   # ~1.2 MB: 5 pieces
    (tmp_path / "f").write_bytes(data)
    r = subprocess.run([str(tgzip / "tgzip"), "-v", str(tmp_path / "f")], capture_output=True, text=True)
    assert r.returncode == 0
    assert "T425 did not boot" in r.stderr and "0 of 5 pieces on the T425" in r.stderr
    assert not (tmp_path / "f").exists()                                 # gzip semantics: input removed
    assert gzip.decompress((tmp_path / "f.gz").read_bytes()) == data     # 5 members, one stream to gunzip
    (tmp_path / "g").write_bytes(data)
    r = subprocess.run([str(tgzip / "tbzip2"), "-k", str(tmp_path / "g")], capture_output=True)
    assert r.returncode == 0 and (tmp_path / "g").exists()               # -k keeps the input
    assert bz2.decompress((tmp_path / "g.bz2").read_bytes()) == data


def test_stdin_empty_and_refuse_overwrite(tgzip, tmp_path):
    for payload in (b"", b"x" * (256 * 1024)):                           # empty, and exactly one piece
        r = subprocess.run([str(tgzip / "tgzip")], input=payload, capture_output=True)
        assert r.returncode == 0 and gzip.decompress(r.stdout) == payload
    (tmp_path / "h").write_bytes(b"a")
    (tmp_path / "h.gz").write_bytes(b"old")
    r = subprocess.run([str(tgzip / "tgzip"), str(tmp_path / "h")], capture_output=True, text=True)
    assert r.returncode == 1 and "exists" in r.stderr and (tmp_path / "h.gz").read_bytes() == b"old"


def test_lock_live_holder_falls_back_dead_holder_is_reclaimed(tgzip, tmp_path):
    """t4call's lock (FR-2): a live holder -> 68030 with the reason; a dead pid -> reclaimed, and the
    run's own lock is removed at exit."""
    lock = tgzip / "t425.lock"
    (tmp_path / "f").write_bytes(b"abc" * 1000)
    lock.write_text(f"{os.getpid()} -\n")                              # this pytest process: alive
    r = subprocess.run([str(tgzip / "tgzip"), "-k", "-f", str(tmp_path / "f")], capture_output=True, text=True)
    assert r.returncode == 0 and f"busy (pid {os.getpid()})" in r.stderr
    assert lock.read_text().startswith(str(os.getpid()))                # someone else's lock is left alone
    dead = subprocess.Popen(["true"])
    dead.wait()
    lock.write_text(f"{dead.pid} -\n")                                  # owner gone: stale
    r = subprocess.run([str(tgzip / "tgzip"), "-k", "-f", str(tmp_path / "f")], capture_output=True, text=True)
    assert r.returncode == 0 and "did not boot" in r.stderr and "busy" not in r.stderr
    assert not lock.exists()                                            # taken, then released at exit
    assert gzip.decompress((tmp_path / "f.gz").read_bytes()) == b"abc" * 1000


def test_t4_adler32_matches_zlib_and_t4_enabled(tmp_path):
    """t4call's own adler32 must equal zlib's (t4serv.c checks with zlib's), incl. > 5552-byte blocks."""
    import ctypes
    import zlib
    so = tmp_path / "t4call.so"
    subprocess.run(["cc", "-shared", "-fPIC", "-O2", "-w", f"-I{SRC}", "-o", str(so),
                    os.path.join(SRC, "t4call.c"), os.path.join(SRC, "t4lock.c"),
                    os.path.join(SRC, "atwboot_host.c")], check=True)
    lib = ctypes.CDLL(str(so))
    lib.t4_adler32.restype = ctypes.c_ulong
    lib.t4_adler32.argtypes = [ctypes.c_char_p, ctypes.c_long]
    for data in (b"", b"a", os.urandom(5552), os.urandom(5553), bytes(200000), os.urandom(300000)):
        assert lib.t4_adler32(data, len(data)) == zlib.adler32(data)
    assert lib.t4_enabled(b"surely-not-a-package") == 0


def test_loaded_marker(tmp_path):
    """t4_loaded_set/is (FR-6): the booted server is recorded so the next user skips a blind probe."""
    import ctypes
    so = tmp_path / "t4lock.so"
    subprocess.run(["cc", "-shared", "-fPIC", "-O2", "-w", f"-I{SRC}", f'-DT4LOADED="{tmp_path}/loaded"',
                    "-o", str(so), os.path.join(SRC, "t4lock.c")], check=True)
    lib = ctypes.CDLL(str(so))
    assert lib.t4_loaded_is(b"xserv.btl") == -1                          # nothing recorded: unknown
    lib.t4_loaded_set(b"compserv.btl")
    assert lib.t4_loaded_is(b"compserv.btl") == 1 and lib.t4_loaded_is(b"xserv.btl") == 0
