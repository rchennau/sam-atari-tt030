import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tt_versions import evaluate  # noqa: E402

INDEX = {"ttjob": ["ttjob", "1.0", "17"], "t425-compress": ["t425-compress", "1.6", "1"], "bash": ["bash", "2.05a", "3.sam1"]}


def v1(started, packages):
    return {"schema": "sam.node.runtime/v1", "node": "atari-tt030", "deploy_commit": "unknown",
            "deployed_at": "1970-01-01T00:03:20Z", "deploy_path": "x-tt030-rpm",
            "services": {"ttjobd": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(started)) if started else None},
            "extra": {"packages": packages}}


def test_states():
    frame = v1(100, {
        "ttjob": ["1.0-17", 200],            # current, but installed after ttjobd started
        "t425-compress": ["1.4-1", 50],      # mirror has 1.6
        "bash": ["2.05a-3.sam1", 10],        # current
        "arc": ["5.21e-2", 10]})             # not on the mirror
    rows, counts = evaluate(frame, INDEX)
    assert {(s, n) for s, n, _ in rows} == {("STALE", "t425-compress"), ("RUNNING_STALE", "ttjobd")}
    assert counts == {"CURRENT": 2, "STALE": 1, "RUNNING_STALE": 1, "local": 1}


def test_no_frame_is_unreported_not_current():
    rows, counts = evaluate(None, INDEX)
    assert rows[0][0] == "UNREPORTED" and counts == {"UNREPORTED": 1}


def test_ttjobd_restarted_after_install_is_current():
    assert evaluate(v1(300, {"ttjob": ["1.0-17", 200]}), INDEX)[0] == []


def test_pre_v1_frame_is_nonconforming():
    rows, _ = evaluate({"node": "atari-tt030", "ttjobd_started": 1, "packages": {}}, INDEX)
    assert rows[0][0] == "NONCONFORMING"
