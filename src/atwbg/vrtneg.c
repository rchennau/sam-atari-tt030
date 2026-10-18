/* vrtneg — reproduce defect db0bb9ec: xVDI vrt_cpyfm to the screen with a negative destination y and clipping
 * off multiplies y unsigned (mulu), so y = -2 writes ~65534 lines past the framebuffer and bus-errors at
 * xVDI text +0x2a94 (TBCAD2TT: PC 0x10291A0, address 0x53FF97F in mode 39).
 * Usage: vrtneg pos    clip off, y = 10       expect "returned"
 *        vrtneg clip   clip on (screen), y = -2  expect "returned"
 *        vrtneg neg    clip off, y = -2       expect a bus error (unpatched driver) or "returned" (fixed)
 * Draws a 16x16 mono pattern at x = 100 on the screen; the AES redraws it away.
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o vrtneg.ttp src/atwbg/vrtneg.c -lgem
 */
#include <gem.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
	static unsigned short bits[16];
	short work_in[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2}, work_out[57], d, h, col[2] = {1, 0}, xy[8], y;
	short clip[4];
	MFDB src = {bits, 16, 16, 1, 0, 1, 0, 0, 0}, scr = {0};
	const char *m = argc > 1 ? argv[1] : "";
	int i;

	if (strcmp(m, "pos") && strcmp(m, "clip") && strcmp(m, "neg")) {
		fprintf(stderr, "usage: vrtneg pos|clip|neg\n");
		return 2;
	}
	for (i = 0; i < 16; i++)
		bits[i] = i & 1 ? 0xaaaa : 0x5555;
	if (appl_init() < 0)
		return 2;
	h = graf_handle(&d, &d, &d, &d);
	v_opnvwk(work_in, &h, work_out);
	if (!h)
		return 2;
	y = strcmp(m, "pos") ? -2 : 10;
	clip[0] = 0; clip[1] = 0; clip[2] = work_out[0]; clip[3] = work_out[1];
	vs_clip(h, !strcmp(m, "clip"), clip);
	xy[0] = 0; xy[1] = 0; xy[2] = 15; xy[3] = 15;
	xy[4] = 100; xy[5] = y; xy[6] = 115; xy[7] = y + 15;
	printf("vrtneg %s: vrt_cpyfm to screen y=%d clip=%d ...\n", m, y, !strcmp(m, "clip"));
	fflush(stdout);
	vrt_cpyfm(h, MD_REPLACE, xy, &src, &scr, col);
	printf("vrtneg %s: returned\n", m);
	v_clsvwk(h);
	appl_exit();
	return 0;
}
