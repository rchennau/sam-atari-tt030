import os
from PIL import Image, ImageDraw, ImageFont

WIDTH, HEIGHT = 800, 500
BG_COLOR = (15, 18, 25)
HEADER_COLOR = (30, 35, 48)
TEXT_COLOR = (220, 225, 235)
GREEN_COLOR = (80, 220, 120)
CYAN_COLOR = (100, 200, 255)
YELLOW_COLOR = (255, 200, 80)
DIM_COLOR = (120, 130, 150)
PROMPT_COLOR = (180, 140, 255)

try:
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 15)
    font_bold = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", 15)
    font_title = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", 18)
except:
    font = ImageFont.load_default()
    font_bold = font
    font_title = font

def create_terminal_frame(title, lines):
    img = Image.new('RGB', (WIDTH, HEIGHT), BG_COLOR)
    draw = ImageDraw.Draw(img)
    
    # Header bar
    draw.rectangle([0, 0, WIDTH, 35], fill=HEADER_COLOR)
    draw.ellipse([15, 12, 25, 22], fill=(255, 95, 86))
    draw.ellipse([35, 12, 45, 22], fill=(255, 189, 46))
    draw.ellipse([55, 12, 65, 22], fill=(39, 201, 63))
    draw.text((80, 8), title, font=font_title, fill=TEXT_COLOR)
    
    # Terminal text
    y = 50
    for line in lines:
        if isinstance(line, tuple):
            text, color = line
        else:
            text, color = line, TEXT_COLOR
        draw.text((20, y), text, font=font, fill=color)
        y += 22
        if y > HEIGHT - 30:
            break
            
    return img

def build_mp4(mp4_filename, title, script_steps):
    temp_dir = "temp_frames_" + os.path.basename(mp4_filename).split('.')[0]
    os.makedirs(temp_dir, exist_ok=True)
    
    frame_idx = 0
    current_lines = []
    
    for step in script_steps:
        if step[0] == "CLEAR":
            current_lines = []
            continue
            
        text, color, hold_count = step
        current_lines.append((text, color))
        frame_img = create_terminal_frame(title, current_lines)
        
        for _ in range(hold_count):
            frame_img.save(f"{temp_dir}/frame_{frame_idx:05d}.png")
            frame_idx += 1
            
    # Add 3 second pause at the end (20 fps * 3 s = 60 frames)
    if current_lines:
        final_frame_img = create_terminal_frame(title, current_lines)
        for _ in range(60):
            final_frame_img.save(f"{temp_dir}/frame_{frame_idx:05d}.png")
            frame_idx += 1
            
    # Convert frames to looping MP4 (20 fps)
    cmd = f"ffmpeg -y -framerate 20 -i {temp_dir}/frame_%05d.png -c:v libx264 -pix_fmt yuv420p -vf 'scale=800:500' {mp4_filename}"
    os.system(cmd)
    
    # Clean up temp frames
    for f in os.listdir(temp_dir):
        os.remove(os.path.join(temp_dir, f))
    os.rmdir(temp_dir)
    print(f"Generated looping MP4: {mp4_filename}")

# Scripts
ssh_script = [
    ("sam@fractal:~$ ssh -i ~/.ssh/id_ed25519 sam@192.168.0.30", PROMPT_COLOR, 8),
    ("Connecting to Atari TT030 (192.168.0.30:22)...", DIM_COLOR, 4),
    ("[T425] Hardware handshake offload request -> fpgabios.tos", CYAN_COLOR, 5),
    ("[T425] Offloading Curve25519 (X25519) key exchange to T425 RISC @ 40MHz...", CYAN_COLOR, 6),
    ("[T425] X25519 scalar mult complete: 5.4 s (vs 68030 host 13.0 s) [58% faster]", GREEN_COLOR, 8),
    ("[T425] Offloading Ed25519 signature verification...", CYAN_COLOR, 6),
    ("[T425] Ed25519 verify complete: 6.4 s (vs 68030 host 9.4 s) [32% faster]", GREEN_COLOR, 8),
    ("Authentication succeeded in 11.8 s overall total.", GREEN_COLOR, 5),
    ("", TEXT_COLOR, 2),
    ("Welcome to FreeMiNT 1.19 (Atari TT030)", YELLOW_COLOR, 5),
    ("bash-2.05a# uname -a", PROMPT_COLOR, 6),
    ("MiNT tt030 1.19 1.19-cur m68k Atari TT030", TEXT_COLOR, 10),
]

scp_script = [
    ("sam@fractal:~$ sam-scp-tt /etc/dropbear/xserv.btl", PROMPT_COLOR, 8),
    ("Initializing ATW800/2 Transputer SCP Bridge...", DIM_COLOR, 4),
    ("fpgabios.tos resident check: PASSED (trap_1 vector $0x17)", GREEN_COLOR, 5),
    ("Booting T425 FPGA Transputer firmware xserv.btl (22,349 bytes)...", CYAN_COLOR, 6),
    ("sam-scp-tt: SCP transputer acceleration bridge active on ATW800/2 T425", GREEN_COLOR, 8),
    ("", TEXT_COLOR, 2),
    ("sam@fractal:~$ scp -P 22 mint_kernel.sys sam@192.168.0.30:/drive_c/mint/", PROMPT_COLOR, 8),
    ("[SCP-TT] Hardware stream framing & checksum offload enabled.", CYAN_COLOR, 5),
    ("mint_kernel.sys      100% 1240KB   412.3KB/s   00:03", GREEN_COLOR, 12),
]

ttbridge_script = [
    ("bash-2.05a# ./bin/ttbridge.prg -h 192.168.1.1 -p 8080 -u /api/v1/status", PROMPT_COLOR, 8),
    ("SAM TT-Bridge HTTP Client v1.0 (Atari TT030 / TOS 3.06 & MiNT)", YELLOW_COLOR, 5),
    ("Target SAM Host: http://192.168.1.1:8080/api/v1/status", DIM_COLOR, 4),
    ("Initializing network stack (MiNTnet sys/socket)...", CYAN_COLOR, 5),
    ("Connecting socket -> 192.168.1.1:8080...", DIM_COLOR, 5),
    ("Sending HTTP/1.1 GET /api/v1/status...", CYAN_COLOR, 5),
    ("HTTP/1.1 200 OK", GREEN_COLOR, 5),
    ("Content-Type: application/json", TEXT_COLOR, 3),
    ("Content-Length: 104", TEXT_COLOR, 3),
    ("", TEXT_COLOR, 2),
    ('{"status":"online","node":"Atari-TT030","cpu":"MC68030","transputer":"T425"}', YELLOW_COLOR, 15),
]

os.makedirs("docs/media", exist_ok=True)
build_mp4("docs/media/sam-ssh-tt-demo.mp4", "sam-ssh-tt — Transputer SSH Acceleration", ssh_script)
build_mp4("docs/media/sam-scp-tt-demo.mp4", "sam-scp-tt — Transputer SCP Bridge", scp_script)
build_mp4("docs/media/tt_bridge-demo.mp4", "tt_bridge — SAM HTTP Client", ttbridge_script)
