/* fillchk — differential check for filled rectangles (video FR-8, atwmono fill hook).
 * Draws a fixed scene, fills one rectangle per case, reads the area around it back with vro_cpyfm and prints a
 * CRC per case. Run once without and once with the hook: every CRC must match. Cases cover aligned and
 * misaligned edges, narrow and one-row rects, both colours, hollow, XOR (must fall back) and clipping.
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o fillchk.ttp src/atwbg/fillchk.c -lgem
 */
#include <gem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static short h;

static void scene(short w, short hgt)
{
	short xy[4], i;
	vswr_mode(h, MD_REPLACE);
	vsf_interior(h, FIS_SOLID);
	vsf_color(h, 0);
	xy[0] = 0; xy[1] = 0; xy[2] = w - 1; xy[3] = hgt - 1;
	vs_clip(h, 0, xy);
	v_bar(h, xy);
	for (i = 0; i < 120; i++) {
		xy[0] = (i * 37) % w; xy[1] = 0; xy[2] = (i * 91) % w; xy[3] = hgt - 1;
		vsl_color(h, 1);
		v_pline(h, 2, xy);
	}
}

static unsigned long crc32(const unsigned char *p, long n)
{
	unsigned long c = 0xffffffffUL;
	int k;
	while (n--) {
		c ^= *p++;
		for (k = 0; k < 8; k++)
			c = c & 1 ? (c >> 1) ^ 0xedb88320UL : c >> 1;
	}
	return ~c;
}

int main(void)
{
	/* x1, y1, x2, y2, colour, interior, writing mode, clip (0 none, 1 = clip to 100..400 x 50..250) */
	static const short cases[][8] = {
		{0, 0, 255, 99, 1, FIS_SOLID, MD_REPLACE, 0},
		{3, 5, 300, 77, 1, FIS_SOLID, MD_REPLACE, 0},
		{3, 5, 300, 77, 0, FIS_SOLID, MD_REPLACE, 0},
		{10, 10, 20, 200, 1, FIS_SOLID, MD_REPLACE, 0},
		{17, 40, 470, 40, 1, FIS_SOLID, MD_REPLACE, 0},
		{9, 3, 422, 230, 1, FIS_HOLLOW, MD_REPLACE, 0},
		{9, 3, 422, 230, 1, FIS_SOLID, MD_XOR, 0},
		{50, 20, 480, 290, 1, FIS_SOLID, MD_REPLACE, 1},
		{120, 60, 135, 61, 1, FIS_SOLID, MD_REPLACE, 1},
		{5, 7, 1000, 700, 1, FIS_SOLID, MD_REPLACE, 0},     /* big: engine path */
		{0, 0, 1919, 1079, 0, FIS_SOLID, MD_REPLACE, 0},    /* full screen, colour 0 */
	};
	short work_in[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2}, work_out[57], d, i, w, hgt, planes;
	MFDB scr = {0}, m = {0};
	long sz;

	if (appl_init() < 0)
		return 2;
	h = graf_handle(&d, &d, &d, &d);
	v_opnvwk(work_in, &h, work_out);
	w = work_out[0] + 1;
	hgt = work_out[1] + 1;
	vq_extnd(h, 1, work_out);
	planes = work_out[4];
	m.fd_w = 512; m.fd_h = 320; m.fd_wdwidth = 32; m.fd_nplanes = planes;
	sz = 64L * 320 * planes;
	if (!(m.fd_addr = malloc(sz)))
		return 2;
	graf_mouse(M_OFF, NULL);
	for (i = 0; i < (short)(sizeof cases / sizeof cases[0]); i++) {
		const short *c = cases[i];
		short r[4] = {c[0], c[1], c[2], c[3]}, cl[4] = {100, 50, 400, 250}, p[8] = {0, 0, 511, 319, 0, 0, 511, 319};
		scene(w, hgt);
		vsf_color(h, c[4]);
		vsf_interior(h, c[5]);
		vswr_mode(h, c[6]);
		vs_clip(h, c[7], cl);
		v_bar(h, r);
		vs_clip(h, 0, cl);
		vswr_mode(h, MD_REPLACE);
		memset(m.fd_addr, 0, sz);
		vro_cpyfm(h, S_ONLY, p, &scr, &m);
		printf("fill %d %d,%d-%d,%d c%d i%d m%d clip%d: crc %08lx\n", i, c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
			crc32(m.fd_addr, sz));
	}
	graf_mouse(M_ON, NULL);
	form_dial(FMD_FINISH, 0, 0, 0, 0, 0, 0, w, hgt);
	v_clsvwk(h);
	appl_exit();
	return 0;
}
