import subprocess
import time
import os

artifact_dir = "/home/sam/.gemini/antigravity-cli/brain/75abdecc-3745-4fae-9a7c-d3c1754d5499"
os.makedirs(artifact_dir, exist_ok=True)
shot_path = os.path.join(artifact_dir, "xboot_menu.png")

# Use xvfb-run wrapping xwd / xwdtopnm / pnmtopng or python PIL imagegrab / xwd
script = f"""
export DISPLAY=:99
Xvfb :99 -screen 0 1280x1024x24 &
XVFB_PID=$!
sleep 1

flatpak run --user org.tuxfamily.hatari --configfile /home/sam/Projects/atari-tt030-enhancement/emulator/hatari.cfg --fast-forward on &
HATARI_PID=$!

sleep 4

xwd -root -display :99 | convert xwd:- {shot_path} 2>/dev/null || xwd -root -display :99 > /tmp/shot.xwd

kill $HATARI_PID 2>/dev/null || true
kill $XVFB_PID 2>/dev/null || true
"""

with open("/tmp/run_shot.sh", "w") as f:
    f.write(script)

os.system("bash /tmp/run_shot.sh")
if os.path.exists(shot_path):
    print("SUCCESS: Captured XBOOT III screenshot size:", os.path.getsize(shot_path))
else:
    print("Note: shot.xwd size:", os.path.getsize("/tmp/shot.xwd") if os.path.exists("/tmp/shot.xwd") else 0)
