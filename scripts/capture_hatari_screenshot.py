import subprocess
import time
import os

os.makedirs("/tmp/hatari_shots", exist_ok=True)
cmd = [
    "xvfb-run", "-a", "-s", "-screen 0 1280x1024x24",
    "flatpak", "run", "--user", "org.tuxfamily.hatari",
    "--configfile", "/home/sam/Projects/atari-tt030-enhancement/emulator/hatari.cfg",
    "--fast-forward", "on",
    "--run-vbls", "3000"
]
print("Running 3000 VBL Hatari simulation...")
res = subprocess.run(cmd, capture_output=True, text=True)
print("Hatari stdout:", res.stdout)
