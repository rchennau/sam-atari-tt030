"""FR-5 check: the evdev -> Atari map types the right characters through the TT's own en_uk.tbl,
plus the KeyPump rules (autorepeat dropped, release chord ends the session). Run: python3 -m pytest"""
import sys
from pathlib import Path

from evdev import ecodes as e

sys.path.insert(0, str(Path(__file__).parent))
import ttkbd_send as t  # noqa: E402

TBL = (Path(__file__).parent.parent / "staging/FREEMINT/sysdir-1-19-4eb/keyboard/en_uk.tbl").read_bytes()
UNSHIFTED = TBL[4:4 + 128]


def test_printable_keys_match_en_uk_table():
    want = {**{getattr(e, f"KEY_{c.upper()}"): c for c in "abcdefghijklmnopqrstuvwxyz0123456789"},
            e.KEY_SEMICOLON: ";", e.KEY_APOSTROPHE: "'", e.KEY_LEFTBRACE: "[", e.KEY_RIGHTBRACE: "]",
            e.KEY_COMMA: ",", e.KEY_DOT: ".", e.KEY_SLASH: "/", e.KEY_MINUS: "-", e.KEY_EQUAL: "=",
            e.KEY_BACKSLASH: "#", e.KEY_102ND: "\\", e.KEY_SPACE: " "}
    bad = {e.KEY[k]: (chr(UNSHIFTED[t.EV2ST[k]]), c) for k, c in want.items() if chr(UNSHIFTED[t.EV2ST[k]]) != c}
    assert not bad, bad


def test_pump_forwards_autorepeat_and_stops_on_chord():
    sent = []
    p = t.KeyPump(lambda sc, fl: sent.append((sc, fl)))
    assert p.event(e.KEY_A, 1) and p.event(e.KEY_A, 2) and p.event(e.KEY_A, 0)
    assert sent == [(0x1E, 0), (0x1E, 0), (0x1E, t.F_BREAK)]
    assert p.event(e.KEY_RIGHTCTRL, 1) and p.event(e.KEY_RIGHTALT, 1)
    assert p.event(e.KEY_ESC, 1) is False
