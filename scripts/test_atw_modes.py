"""FR-1 decoder (tt030-atw800-video-modes): layout, RAM and the loud failures.  pytest scripts/test_atw_modes.py"""
import os
import struct
import sys

import pytest

sys.path.insert(0, os.path.dirname(__file__))
import atw_modes as A  # noqa: E402


def rec(name, bpp, vis, virt):
    bpl = virt[0] * bpp // 8
    body = struct.pack(">9H", 1, bpl, bpp, 0, 1, vis[0] - 1, vis[1] - 1, virt[0] - 1, virt[1] - 1)
    return name.ljust(34).encode() + body + bytes(A.REC - 34 - len(body) - 6) + A.MARK


def test_split_row_ram_uses_the_virtual_width():
    rows = A.decode(rec("1024 x 768 256 60Hz", 8, (1024, 768), (1024, 768))
                    + rec("1280 x 720 256 60Hz (Split)", 8, (640, 720), (1280, 720)))
    assert [r["mode"] for r in rows] == [1, 2]
    assert rows[0]["vram_kb"] == 768 and rows[0]["fits_2mb"]
    assert rows[1]["split"] and rows[1]["w"] == 640 and rows[1]["vram_kb"] == 900


def test_bad_files_fail_loudly():
    good = rec("640 x 480 2 60Hz", 1, (640, 480), (640, 480))
    with pytest.raises(ValueError):
        A.decode(good[:-1])                          # not a multiple of 86
    with pytest.raises(ValueError):
        A.decode(good[:-6] + bytes(6))               # marker missing
    with pytest.raises(ValueError):
        A.decode(good[:36] + struct.pack(">H", 999) + good[38:])   # bytes/line vs width


def test_card_timings_match_the_measured_table():
    """docs/resolution-management.md computed these from the dividers; the card's table must still decode to them."""
    here = os.path.join(os.path.dirname(__file__), "..", "staging", "XVDI_CARD", "XVDI.BIB")
    rows = {r["mode"]: r for r in A.decode(open(here, "rb").read())}
    assert (rows[32]["pclk_mhz"], rows[32]["h_khz"], rows[32]["v_hz"]) == (137.7, 66.2, 59.6)
    assert (rows[39]["pclk_mhz"], rows[39]["htotal"], rows[39]["vtotal"]) == (102.6, 1824, 967)
    assert rows[37]["bpp"] == 1 and rows[37]["pclk_mhz"] == 137.7
