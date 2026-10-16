/* blitchk — check that screen-to-screen vro_cpyfm copies exactly (video FR-8, mono copy-loop patch).
 * Draws a fixed pattern, copies a rectangle on the screen, reads the source and destination back into
 * memory with vro_cpyfm and compares them. Cases cover forward/backward copies, odd byte addresses and
 * widths that are and are not a multiple of 4 bytes. Prints one PASS/FAIL line per case, exit 1 on any FAIL.
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o blitchk.ttp src/atwbg/blitchk.c -lgem
 */
#include <gem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static short h;

static void pattern(short w, short hgt)
{
	short xy[4], i;
	vsf_interior(h, FIS_SOLID);
	vsf_color(h, 0);
	xy[0] = 0; xy[1] = 0; xy[2] = w - 1; xy[3] = hgt - 1;
	v_bar(h, xy);
	for (i = 0; i < 120; i++) {
		xy[0] = (i * 37) % w; xy[1] = 0; xy[2] = (i * 91) % w; xy[3] = hgt - 1;
		vsl_color(h, 1 + (i & 7));
		v_pline(h, 2, xy);
	}
	for (i = 0; i < 40; i++)
		v_gtext(h, (i * 53) % (w - 200), 16 + (i * 29) % (hgt - 32), "Pack my box 0123456789");
}

static void grab(MFDB *scr, MFDB *mem, short x, short y, short cw, short ch)
{
	short p[8] = {x, y, x + cw - 1, y + ch - 1, 0, 0, cw - 1, ch - 1};
	vro_cpyfm(h, S_ONLY, p, scr, mem);
}

int main(void)
{
	/* sx, sy, dx, dy, width, height */
	static const short cases[][6] = {
		{0, 0, 256, 128, 256, 256},   /* vdibench shape, dst after src: backward loop */
		{256, 128, 0, 0, 256, 256},   /* dst before src: forward loop */
		{8, 8, 264, 136, 256, 200},   /* odd byte addresses */
		{16, 0, 400, 40, 200, 100},   /* 25 bytes: long loop + 1 byte remainder */
		{24, 0, 32, 2, 104, 100},     /* 13 bytes, overlapping, dst after src */
		{40, 4, 32, 0, 104, 100},     /* 13 bytes, overlapping, dst before src */
		{3, 5, 300, 9, 211, 77},      /* unaligned edges: the other path, as a control */
	};
	short work_in[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2}, work_out[57], d, i, w, hgt, planes, fails = 0;
	MFDB scr = {0}, a = {0}, b = {0};
	long wdw = (256 + 15) / 16, sz;

	if (appl_init() < 0)
		return 2;
	h = graf_handle(&d, &d, &d, &d);
	v_opnvwk(work_in, &h, work_out);
	w = work_out[0] + 1;
	hgt = work_out[1] + 1;
	vq_extnd(h, 1, work_out);
	planes = work_out[4];
	sz = wdw * 2 * 256 * planes;
	a.fd_addr = malloc(sz); b.fd_addr = malloc(sz);
	if (!a.fd_addr || !b.fd_addr)
		return 2;
	a.fd_w = b.fd_w = 256; a.fd_h = b.fd_h = 256; a.fd_wdwidth = b.fd_wdwidth = wdw;
	a.fd_stand = b.fd_stand = 0; a.fd_nplanes = b.fd_nplanes = planes;
	graf_mouse(M_OFF, NULL);

	for (i = 0; i < (short)(sizeof cases / sizeof cases[0]); i++) {
		const short *c = cases[i];
		short p[8] = {c[0], c[1], c[0] + c[4] - 1, c[1] + c[5] - 1, c[2], c[3], c[2] + c[4] - 1, c[3] + c[5] - 1};
		long n, diff = 0;
		unsigned char *pa = a.fd_addr, *pb = b.fd_addr;
		pattern(w, hgt);
		memset(pa, 0, sz); memset(pb, 0, sz);
		grab(&scr, &a, c[0], c[1], c[4], c[5]);   /* source before the copy (it may be overwritten) */
		vro_cpyfm(h, S_ONLY, p, &scr, &scr);
		grab(&scr, &b, c[2], c[3], c[4], c[5]);
		for (n = 0; n < sz; n++)
			diff += pa[n] != pb[n];
		printf("case %d src %d,%d dst %d,%d %dx%d planes %d: %s (%ld bytes differ)\n",
			i, c[0], c[1], c[2], c[3], c[4], c[5], planes, diff ? "FAIL" : "PASS", diff);
		fails += diff != 0;
	}
	graf_mouse(M_ON, NULL);
	form_dial(FMD_FINISH, 0, 0, 0, 0, 0, 0, w, hgt);   /* ask the desktop to redraw */
	v_clsvwk(h);
	appl_exit();
	return fails ? 1 : 0;
}
