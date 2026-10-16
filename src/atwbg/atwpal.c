/* atwpal — set VDI colours 0 and 1 (e.g. amber on black in mono) and show whether the card's LUT follows.
 * Usage: atwpal R0 G0 B0 R1 G1 B1   (VDI 0..1000 per channel)   e.g. atwpal 0 0 0 1000 690 0
 *        atwpal -d FILE              dump all colours in xVDI DEFAULT.PAL format (256 x R,G,B big-endian words)
 *        atwpal -l FILE              load a palette in that format now (what xVDI does at boot)
 *        atwpal -p X Y [X Y...]      print the VDI colour index and its RGB at screen points
 * Prints the first 8 LUT bytes at 0xFEDFF000 (ATW800/2 Programmer's Manual memory map) before and after.
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o atwpal.ttp src/atwbg/atwpal.c -lgem
 */
#include <gem.h>
#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char lut[8];
static long peek(void) { int i; for (i = 0; i < 8; i++) lut[i] = ((volatile unsigned char *)0xFEDFF000UL)[i]; return 0; }
static void show(const char *t) { int i; Supexec(peek); printf("%s LUT:", t); for (i = 0; i < 8; i++) printf(" %02x", lut[i]); printf("\n"); }

int main(int argc, char **argv)
{
	short work_in[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2}, work_out[57], d, h, rgb[3], i;
	if (argc >= 4 && !strcmp(argv[1], "-p")) {
		short pel, idx;
		if (appl_init() < 0) return 2;
		h = graf_handle(&d, &d, &d, &d);
		v_opnvwk(work_in, &h, work_out);
		for (i = 2; i + 1 < argc; i += 2) {
			v_get_pixel(h, atoi(argv[i]), atoi(argv[i + 1]), &pel, &idx);
			vq_color(h, idx, 1, rgb);
			printf("pixel %s,%s: index %d (hw %d) rgb %d %d %d\n", argv[i], argv[i + 1], idx, pel, rgb[0], rgb[1], rgb[2]);
		}
		v_clsvwk(h);
		appl_exit();
		return 0;
	}
	if (argc == 3 && !strcmp(argv[1], "-l")) {
		FILE *f;
		if (!(f = fopen(argv[2], "rb"))) { perror(argv[2]); return 1; }
		if (appl_init() < 0) return 2;
		h = graf_handle(&d, &d, &d, &d);
		v_opnvwk(work_in, &h, work_out);
		for (i = 0; i < 256 && fread(rgb, 2, 3, f) == 3; i++)
			vs_color(h, i, rgb);
		fclose(f);
		printf("atwpal: %d colours loaded from %s\n", i, argv[2]);
		v_clsvwk(h);
		appl_exit();
		return 0;
	}
	if (argc == 3 && !strcmp(argv[1], "-d")) {
		FILE *f;
		if (appl_init() < 0) return 2;
		h = graf_handle(&d, &d, &d, &d);
		v_opnvwk(work_in, &h, work_out);
		if (!(f = fopen(argv[2], "wb"))) { perror(argv[2]); return 1; }
		for (i = 0; i < work_out[13] && i < 256; i++) {
			vq_color(h, i, 0, rgb);
			fwrite(rgb, 2, 3, f);
		}
		fclose(f);
		printf("atwpal: %d colours written to %s\n", i, argv[2]);
		v_clsvwk(h);
		appl_exit();
		return 0;
	}
	if (argc != 7) { fprintf(stderr, "usage: atwpal R0 G0 B0 R1 G1 B1 (0..1000)\n"); return 2; }
	if (appl_init() < 0) return 2;
	h = graf_handle(&d, &d, &d, &d);
	v_opnvwk(work_in, &h, work_out);
	show("before");
	for (i = 0; i < 2; i++) {
		rgb[0] = atoi(argv[1 + 3 * i]); rgb[1] = atoi(argv[2 + 3 * i]); rgb[2] = atoi(argv[3 + 3 * i]);
		vs_color(h, i, rgb);
		vq_color(h, i, 1, rgb);
		printf("colour %d now %d %d %d\n", i, rgb[0], rgb[1], rgb[2]);
	}
	show("after ");
	v_clsvwk(h);
	appl_exit();
	return 0;
}
