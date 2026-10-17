/* atwshot — save the ATW800/2 screen to a file (the inverse of atwbg), so the TT's display can be checked remotely.
 * Mode from Line-A (V_X_MAX, V_Y_MAX, VPLANES, VWRAP); framebuffer at 0xFEC00000 (Logbase, mxprobe 2026-10-17).
 * File: "ATWSHOT W H PLANES\n", then for 8 planes the 256-entry RGB565 LUT (512 B, as read at 0xFEDFF000) and W*H
 * index bytes; for 16 planes W*H RGB565 little-endian words. Other plane counts are refused.
 * Read-only: copies video memory in supervisor mode, 64 KB per call so MiNT keeps running.
 * Convert on the remote system management host: src/atwbg/atwshot2png.py FILE.raw OUT.png
 * Usage: atwshot FILE.raw */
#include <mint/osbind.h>
#include <mint/linea.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *c_dst, *c_src;
static long c_len;
static long k_copy(void) { memcpy(c_dst, c_src, c_len); return 0; }

int main(int argc, char **argv)
{
	unsigned char *img;
	long w, h, bpp, row, y, o, n;
	FILE *f;
	if (argc != 2) { fprintf(stderr, "usage: atwshot FILE.raw\n"); return 2; }
	linea0();
	w = V_X_MAX; h = V_Y_MAX;
	if (VPLANES != 8 && VPLANES != 16) { fprintf(stderr, "atwshot: %d planes not supported\n", VPLANES); return 1; }
	bpp = VPLANES / 8;
	row = w * bpp;
	n = 512 + row * h;
	if (!(img = malloc(n))) { fprintf(stderr, "atwshot: no memory\n"); return 1; }
	c_dst = img; c_src = (unsigned char *)0xFEDFF000UL; c_len = 512;
	Supexec(k_copy);
	for (y = 0; y < h; y++)                     /* row by row: VWRAP may exceed the visible width */
		for (o = 0; o < row; o += 65536) {
			c_dst = img + 512 + y * row + o;
			c_src = (unsigned char *)0xFEC00000UL + y * VWRAP + o;
			c_len = row - o < 65536 ? row - o : 65536;
			Supexec(k_copy);
		}
	if (!(f = fopen(argv[1], "wb"))) { perror(argv[1]); return 1; }
	fprintf(f, "ATWSHOT %ld %ld %d\n", w, h, VPLANES);
	if (bpp == 1 ? fwrite(img, 1, n, f) != (size_t)n : fwrite(img + 512, 1, n - 512, f) != (size_t)(n - 512)) {
		perror(argv[1]);
		return 1;
	}
	return fclose(f) ? 1 : 0;
}
