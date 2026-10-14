import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tt_versions import evaluate  # noqa: E402

INDEX = {"ttjob": ["ttjob", "1.0", "17"], "t425-compress": ["t425-compress", "1.6", "1"], "bash": ["bash", "2.05a", "3.sam1"]}


def test_states():
    frame = {"ttjobd_started": 100, "packages": {
        "ttjob": ["1.0-17", 200],            # current, but installed after ttjobd started
        "t425-compress": ["1.4-1", 50],      # mirror has 1.6
        "bash": ["2.05a-3.sam1", 10],        # current
        "arc": ["5.21e-2", 10]}}             # not on the mirror
    rows, counts = evaluate(frame, INDEX)
    assert {(s, n) for s, n, _ in rows} == {("STALE", "t425-compress"), ("RUNNING_STALE", "ttjobd")}
    assert counts == {"CURRENT": 2, "STALE": 1, "RUNNING_STALE": 1, "local": 1}


def test_no_frame_is_unreported_not_current():
    rows, counts = evaluate(None, INDEX)
    assert rows[0][0] == "UNREPORTED" and counts == {"UNREPORTED": 1}


def test_ttjobd_restarted_after_install_is_current():
    frame = {"ttjobd_started": 300, "packages": {"ttjob": ["1.0-17", 200]}}
    assert evaluate(frame, INDEX)[0] == []
