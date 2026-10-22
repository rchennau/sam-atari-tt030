"""defect b5fae241: the WiFi handshake reads an ack before 'session up'; a refusal (close) exits non-zero.
No evdev needed.  python3 -m pytest scripts/test_ttkbd_session.py"""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).parent))
import ttkbd_send as S  # noqa: E402


def _fake_tt(reply):
    """One-connection TCP server: sends a v3 hello, reads the 96-byte auth, then `reply` (b'' = just close)."""
    import socket
    import threading
    srv = socket.socket()
    srv.bind(("127.0.0.1", 0))
    srv.listen(1)

    def run():
        c, _ = srv.accept()
        c.sendall(bytes([3]) + bytes(16) + bytes(range(32)))
        got = b""
        while len(got) < 96:
            got += c.recv(96 - len(got))
        if reply:
            c.sendall(reply)
        c.close()
        srv.close()
    threading.Thread(target=run, daemon=True).start()
    return srv.getsockname()[1]


def test_refused_key_is_not_a_session(tmp_path, monkeypatch):
    """defect b5fae241: a TT that closes after the auth (bad signature) must not read as 'session up'."""
    key = tmp_path / "k"
    key.write_bytes(bytes(32))
    monkeypatch.setattr(S, "KEYFILE", key)
    with pytest.raises(SystemExit, match="refused"):
        S.Session("127.0.0.1", _fake_tt(b""))
    assert S.Session("127.0.0.1", _fake_tt(S.MSG_ACK)).ctr == 0     # the ack = accepted
