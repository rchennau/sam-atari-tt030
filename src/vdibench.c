/* vdibench — FR-8 VDI drawing rate. Times four VDI primitives and writes ops/s to
 * VDIBENCH.TXT in the current directory (append), so the same binary compares
 * TOS 3.06, EmuTOS and XaAES. Timer: clock() = _hz_200, 5 ms resolution.
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o vdibench.prg src/vdibench.c -lgem
 * ponytail: fixed op counts, no warm-up; make them adaptive if a config runs < 1 s.
 */
#include <gem.h>
#include <stdio.h>
#include <time.h>

static short h, work_out[57];

static double rate(long n, clock_t t0)
{
	clock_t dt = clock() - t0;
	return dt > 0 ? n * (double)CLOCKS_PER_SEC / dt : -1;
}

int main(void)
{
	short work_in[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2}, d, i, xy[8];
	MFDB scr = {0};
	short w, hgt;
	double r[4];
	clock_t t0;
	FILE *f;

	if (appl_init() < 0)
		return 1;
	h = graf_handle(&d, &d, &d, &d);
	v_opnvwk(work_in, &h, work_out);
	w = work_out[0] + 1;
	hgt = work_out[1] + 1;
	graf_mouse(M_OFF, NULL);

	t0 = clock();
	for (i = 0; i < 4000; i++) {
		xy[0] = i % w; xy[1] = 0; xy[2] = w - 1 - i % w; xy[3] = hgt - 1;
		vsl_color(h, i & 15);
		v_pline(h, 2, xy);
	}
	r[0] = rate(4000, t0);

	vsf_interior(h, FIS_SOLID);
	t0 = clock();
	for (i = 0; i < 1000; i++) {
		xy[0] = i % (w - 100); xy[1] = i % (hgt - 100); xy[2] = xy[0] + 99; xy[3] = xy[1] + 99;
		vsf_color(h, i & 15);
		v_bar(h, xy);
	}
	r[1] = rate(1000, t0);

	t0 = clock();
	for (i = 0; i < 2000; i++)
		v_gtext(h, i % (w - 300), 16 + i % (hgt - 32), "The quick brown fox jumps");
	r[2] = rate(2000, t0);

	t0 = clock();
	for (i = 0; i < 200; i++) {
		short p[8] = {0, 0, 255, 255, (i & 1) ? 256 : 0, 128, (i & 1) ? 511 : 255, 383};
		vro_cpyfm(h, S_ONLY, p, &scr, &scr);
	}
	r[3] = rate(200, t0);

	graf_mouse(M_ON, NULL);
	f = fopen("VDIBENCH.TXT", "a");
	if (f) {
		fprintf(f, "screen %dx%d colours %d | lines/s %.0f | bars100/s %.0f | text25/s %.0f | blit256/s %.1f\n",
			w, hgt, work_out[13], r[0], r[1], r[2], r[3]);
		fclose(f);
	}
	v_clsvwk(h);
	appl_exit();
	return f ? 0 : 2;
}
