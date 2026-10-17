"""tt030-atw-display-tuning FR-9: src/atwbg/matrix.c on a RAM framebuffer (gcc on the host).
The animation must stay inside its cell area, use only the chosen colours, and pick its colours from the live LUT
(the amber LUT bytes below were read from the TT by mxprobe, 2026-10-17).  pytest scripts/test_matrix.py"""
import os
import subprocess

R = os.path.join(os.path.dirname(__file__), "..")
SRC = os.path.join(R, "src", "atwbg")
# First 32 hardware LUT entries (RGB565) under the amber look; the rest are left 0 (black) here.
AMBER = [0x0000, 0xb4c0, 0x0000, 0x18c0, 0xe600, 0x93e0, 0x4a00, 0x41a0, 0x8360, 0x5a60, 0x3160, 0x0860, 0x7300,
         0x4a00, 0x2100, 0x93e0, 0xfec0, 0xee40, 0xddc0, 0xcd40, 0xbce0, 0xa460, 0x93e0, 0x8360, 0x72e0, 0x5a80,
         0x4a00, 0x41a0, 0x3140, 0x20e0, 0x1060, 0x0000]
DRV = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "matrix.h"
#define COLS 100
#define ROWS 30
#define STRIDE (COLS * 8 + 16)          /* 16 guard bytes at the end of every line */
#define GUARD 0xAA
static unsigned short lut[256] = { LUT };
int main(int argc, char **argv)
{
	static unsigned char font[16 * 256], fb[(ROWS * 16 + 4) * STRIDE];
	struct mx m;
	long i, frames = atol(argv[1]), cells = 0;
	int x, y, ok;
	for (i = 0; i < (long)sizeof font; i++) font[i] = (unsigned char)(i * 2654435761UL >> 24);
	memset(fb, GUARD, sizeof fb);
	memset(&m, 0, sizeof m);
	m.fb = fb; m.stride = STRIDE; m.cols = COLS; m.rows = ROWS; m.font = font; m.fwidth = 256;
	mx_colours(&m, lut);
	printf("bg %d head %d bands %d %d %d %d\n", m.bg, m.head, m.band[0], m.band[1], m.band[2], m.band[3]);
	if (mx_init(&m, 12345)) return 1;
	mx_clear(&m);
	for (i = 0; i < frames; i++) cells += mx_frame(&m);
	for (y = 0, ok = 1; y < ROWS * 16 + 4; y++)
		for (x = 0; x < STRIDE; x++) {
			unsigned char v = fb[y * STRIDE + x];
			int inside = y < ROWS * 16 && x < COLS * 8;
			if (!inside && v != GUARD) ok = 0;
			if (inside && v != m.bg && v != m.head && v != m.band[0] && v != m.band[1] && v != m.band[2] && v != m.band[3]) ok = 0;
		}
	for (i = 0, x = 0; i < ROWS * 16 * STRIDE; i++) x += fb[i] != m.bg && fb[i] != GUARD;
	printf("cells %ld lit %d ok %d sum %lu\n", cells, x, ok, (unsigned long)m.seed);
	mx_free(&m);
	return 0;
}
'''


def _run(tmp_path, frames):
    (tmp_path / "d.c").write_text(DRV.replace("LUT", ", ".join(hex(v) for v in AMBER)))
    exe = tmp_path / "d"
    subprocess.run(["gcc", "-std=c99", "-Wall", "-Werror", "-I", SRC, "-o", exe, tmp_path / "d.c", f"{SRC}/matrix.c"],
                   check=True)
    out = subprocess.run([exe, str(frames)], text=True, capture_output=True, check=True).stdout.split("\n")
    colours = [int(w) for w in out[0].split()[1::2][:2]] + [int(w) for w in out[0].split()[5:]]
    stats = dict(zip(out[1].split()[0::2], (int(w) for w in out[1].split()[1::2])))
    return colours, stats


def test_colours_from_amber_lut(tmp_path):
    (bg, head, *bands), _ = _run(tmp_path, 0)
    lum = [((v >> 11) & 31) * 8 * 299 + ((v >> 5) & 63) * 4 * 587 + (v & 31) * 8 * 114 for v in AMBER] + [0] * 224
    assert lum[bg] == 0 and head == 16                          # black background, brightest amber head
    seq = [lum[head]] + [lum[b] for b in bands] + [lum[bg]]
    assert seq == sorted(seq, reverse=True) and len(set(seq)) == len(seq)   # strictly fading


def test_frames_stay_in_area_and_palette(tmp_path):
    _, s = _run(tmp_path, 3000)
    assert s["ok"] == 1                                         # no guard byte touched, no stray colour
    assert s["lit"] > 0 and s["cells"] > 3000                   # it actually drew, ~6 cells per step
    _, again = _run(tmp_path, 3000)
    assert again == s                                           # deterministic for a seed
