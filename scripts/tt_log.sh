# tt_log.sh — source at the top of a TT030 script (operator rule 2026-10-13: every script logs where SAM can review).
#   . "$(dirname "$0")/tt_log.sh"     (or the right relative path)
# Appends the script's whole stdout+stderr to ~/.local/state/tt030-ops/<script>.log with a start line (args, pid)
# and an exit line, and emits the central sam_logger event tt030.script.run (script, rc, log) at exit — the same
# channel as ci.sh's tt030.ci.run. Output still reaches the terminal. A failed emit never changes the exit code.
TT_NAME=$(basename "$0" .sh)
TT_LOG_DIR=${TT_LOG_DIR:-$HOME/.local/state/tt030-ops}
mkdir -p "$TT_LOG_DIR"
TT_LOG=$TT_LOG_DIR/$TT_NAME.log
echo "== $(date -u +%FT%TZ) start $TT_NAME $* (pid $$)" >> "$TT_LOG"
exec > >(tee -a "$TT_LOG") 2> >(tee -a "$TT_LOG" >&2)
_tt_log_exit() {
    local rc=$1
    echo "== $(date -u +%FT%TZ) exit $rc" >> "$TT_LOG"
    PYTHONPATH=/opt/sam/runtime/src /opt/sam/venv/bin/python3 -c '
import sys
from sam.utils.sam_logger import emit
emit("tt030.script.run", outcome="pass" if sys.argv[2] == "0" else "fail", script=sys.argv[1], rc=int(sys.argv[2]), log=sys.argv[3])
' "$TT_NAME" "$rc" "$TT_LOG" > /dev/null 2>&1 || true
}
trap '_tt_log_exit $?' EXIT
