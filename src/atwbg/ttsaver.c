/* ttsaver — screensaver for the ATW800/2 (PMS tt030-atw-display-tuning FR-5, FR-9).
 * After N idle minutes it blanks the screen by loading an all-black hardware LUT (saved first, restored on wake);
 * the first key, mouse move or mouse button switches it back on. Idle = no change in the IKBD buffer tail index and
 * the Line-A mouse position/buttons, polled every 0.25 s; polling only reads, so no keystroke is swallowed.
 * Serial traffic does not wake the screen (automation over Modem 2 needs no picture).
 * SIGTERM/SIGINT/SIGHUP switch the picture back on before exiting (killing it while blanked left the screen dark,
 * 2026-10-17); SIGKILL cannot be caught, so after `kill -9` reload the palette: `atwpal -l /c/AUTO/DEFAULT.PAL`.
 * It never switches the video timing generator off: a VDI client drawing while ctrl_reg bit a = 0 hard-hangs the TT
 * (defect 36bc464e, reproduced 2/2 on 2026-10-22; 0/2 with the VTG on). > 8-plane modes do not use the LUT: no blank there.
 * -x matrix (FR-9): in 8-plane and 1-plane modes (1 plane: white-on-black dithered tail, 2026-10-21), instead of blanking it saves the screen to RAM, draws falling glyphs straight
 * into VRAM (src/atwbg/matrix.c; the VDI is not re-entrant from a background process) in colours picked from the
 * live LUT, and restores the saved screen on wake or on SIGTERM; after -f more minutes it restores and blanks.
 * Other plane counts, no RAM for the saved screen, or no 8x16 font: plain blanking. While the saver is on it holds the AES screen
 * lock (wind_update BEG_UPDATE), so GEM apps that lock before drawing (toswin2's cursor blink, textwin.c) wait instead
 * of drawing over the animation. ponytail: an app that draws without the lock is still overwritten by the restore.
 * Transputers (product rule 1): the T425 can write VRAM (0x80600000 up), but in mode 32 the picture blanks while
 * it runs a job (FR-6), so the animation stays on the 68030; the TRAMs cannot reach VRAM. In <= 102.6 MHz modes a
 * T425 renderer is possible but unmeasured (68030: 11.7 fps in mode 32, 2026-10-17).
 * -T SECONDS: test — animate at once for SECONDS, restore, print frames, fps and the VRAM checksum before/after.
 * Usage: ttsaver [-m MINUTES] [-s SECONDS] [-x matrix] [-f MINUTES] [-T SECONDS]   default 10 minutes, -f 10.
 * Installed as /usr/sbin/ttsaver.prg by the atwtools RPM (a .TTP would make XaAES open a toswin2 window for it).
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o ttsaver.prg src/atwbg/ttsaver.c src/atwbg/matrix.c -lgem
 */
#include <mint/osbind.h>
#include <mint/linea.h>
#include <gem.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "matrix.h"

#define VRAM ((unsigned char *)0xFEC00000UL)                   /* Logbase in every mode (mxprobe 2026-10-17) */
#define LUT ((volatile unsigned short *)0xFEDFF000UL)          /* 256 x RGB565 hardware LUT */
#define CHUNK 65536L                                           /* bytes per supervisor call: ~25 ms, MiNT keeps running */

static unsigned short lut_saved[256];
static int blanked;
static volatile int off, anim;
static int ap_id = -1, locked;
static unsigned char *saved;
static long scr_bytes;
static struct mx mx;

/* chunked supervisor copy/fill/sum; the arguments travel in globals because Supexec passes none */
static unsigned char *c_dst, *c_src;
static long c_len, c_val;
static unsigned long c_sum;
static long k_copy(void) { memcpy(c_dst, c_src, c_len); return 0; }
static long k_fill(void) { memset(c_dst, (int)c_val, c_len); return 0; }
static long k_sum(void) { long i; for (i = 0; i < c_len; i++) c_sum = c_sum * 31 + c_dst[i]; return 0; }
static long k_frame(void) { c_val = mx_frame(&mx); return 0; }
static long k_lut(void) { int i; for (i = 0; i < 256; i++) ((unsigned short *)c_dst)[i] = LUT[i]; return 0; }

static void chunked(long (*fn)(void), unsigned char *dst, unsigned char *src, long n)
{
	long o;
	for (o = 0; o < n; o += CHUNK) {
		c_dst = dst + o; c_src = src ? src + o : 0; c_len = n - o < CHUNK ? n - o : CHUNK;
		Supexec(fn);
	}
}

static unsigned long vram_sum(void) { c_sum = 0; chunked(k_sum, VRAM, 0, scr_bytes); return c_sum; }

static long scr_off(void)
{
	int i;
	if (VPLANES > 8 || blanked) return 0;
	for (i = 0; i < 256; i++) { lut_saved[i] = LUT[i]; LUT[i] = 0; }
	blanked = 1;
	return 0;
}
static long scr_on(void) { int i; if (blanked) for (i = 0; i < 256; i++) LUT[i] = lut_saved[i]; blanked = 0; return 0; }

static int matrix_start(void)
{
	unsigned short lut[256];
	__FONT *f = __fonts[2];
	long i, zero = 0;
	if ((VPLANES != 8 && VPLANES != 1) || VWRAP * 8 / VPLANES < V_X_MAX || !f || f->form_height != 16 ||
	    f->form_width < 256 || f->first_ade != 0)
		return -1;
	scr_bytes = (long)VWRAP * V_Y_MAX;
	if (!(saved = malloc(scr_bytes))) return -1;
	memset(&mx, 0, sizeof mx);
	mx.fb = VRAM; mx.stride = VWRAP; mx.cols = V_X_MAX / 8; mx.rows = V_Y_MAX / 16;
	mx.font = (const unsigned char *)f->dat_table; mx.fwidth = f->form_width;
	if (mx_init(&mx, (unsigned long)clock() | 1)) { free(saved); saved = 0; return -1; }
	chunked(k_copy, saved, VRAM, scr_bytes);
	if (VPLANES == 1) {          /* ponytail: background = the commoner of 0x00/0xFF on the saved screen; a mostly-ink screen inverts the look */
		for (i = 0; i < scr_bytes; i++) zero += saved[i] == 0 ? 1 : saved[i] == 0xFF ? -1 : 0;
		mx_mono(&mx, zero >= 0 ? 0x00 : 0xFF);
	} else {
		c_dst = (unsigned char *)lut; Supexec(k_lut);
		mx_colours(&mx, lut);
	}
	anim = 1;                                            /* from here a SIGTERM restores */
	c_val = mx.bg; chunked(k_fill, VRAM, 0, scr_bytes);
	return 0;
}

static void matrix_stop(void)
{
	if (!anim) return;
	chunked(k_copy, VRAM, saved, scr_bytes);
	anim = 0;                                            /* before the frees: bye() must not restore from freed memory */
	mx_free(&mx);
	free(saved); saved = 0;
}

static void lock(int on) { if (ap_id >= 0 && locked != on) { wind_update(on ? BEG_UPDATE : END_UPDATE); locked = on; } }
static void bye(int sig) { matrix_stop(); if (off) Supexec(scr_on); lock(0); _exit(128 + sig); }

static long frame(void)     /* one animation step, then sleep out the rest of a 1/15 s frame */
{
	clock_t t = clock();
	long dt;
	Supexec(k_frame);
	dt = (long)(clock() - t) * 1000000L / CLOCKS_PER_SEC;
	if (dt < 66000) usleep(66000 - dt);
	return c_val;
}

int main(int argc, char **argv)
{
	long limit = 600, fallback = 600, test = 0, idle = 0, frames;
	_IOREC *kb = (_IOREC *)Iorec(1);
	short tail, mx_, my, mb, t, x, y, b;
	int i, matrix = 0;
	for (i = 1; i + 1 < argc; i += 2) {
		if (!strcmp(argv[i], "-m")) limit = atol(argv[i + 1]) * 60;
		if (!strcmp(argv[i], "-s")) limit = atol(argv[i + 1]);
		if (!strcmp(argv[i], "-x")) matrix = !strcmp(argv[i + 1], "matrix");
		if (!strcmp(argv[i], "-f")) fallback = atol(argv[i + 1]) * 60;
		if (!strcmp(argv[i], "-T")) test = atol(argv[i + 1]);
	}
	signal(SIGTERM, bye); signal(SIGINT, bye); signal(SIGHUP, bye);
	linea0();
	if (test) {
		unsigned long before, after;
		clock_t t0;
		scr_bytes = (long)VWRAP * V_Y_MAX;
		before = vram_sum();
		if (matrix_start()) { printf("ttsaver: no matrix in this mode (%d planes)\n", VPLANES); return 1; }
		t0 = clock();
		for (frames = 0; clock() - t0 < test * CLOCKS_PER_SEC; frames++) frame();
		printf("ttsaver: %ld frames in %ld s = %ld.%ld fps; colours bg %d head %d bands %d %d %d %d\n", frames, test,
		       frames / test, frames * 10 / test % 10, mx.bg, mx.head, mx.band[0], mx.band[1], mx.band[2], mx.band[3]);
		matrix_stop();
		after = vram_sum();
		printf("ttsaver: vram sum before %08lx after %08lx %s\n", before, after, before == after ? "IDENTICAL" : "DIFFERENT");
		return before != after;
	}
	ap_id = appl_init();                                  /* no AES (plain shell): run without the screen lock */
	tail = kb->ibuftl; mx_ = GCURX; my = GCURY; mb = MOUSE_BT;
	if (isatty(1)) {                                     /* silent when started from XaAES autorun (.PRG, no window) */
		printf("ttsaver: %s after %ld s idle\n", matrix ? "matrix" : "blank", limit);
		fflush(stdout);
	}
	for (;;) {
		if (anim) frame(); else usleep(250000);
		t = kb->ibuftl; x = GCURX; y = GCURY; b = MOUSE_BT;
		if (t != tail || x != mx_ || y != my || b != mb) {
			tail = t; mx_ = x; my = y; mb = b;
			idle = 0;
			matrix_stop();
			if (off) { Supexec(scr_on); off = 0; }
			lock(0);
		} else if (anim) {
			if (++idle >= fallback * 15) { matrix_stop(); Supexec(scr_off); off = 1; }   /* idle counts frames here */
		} else if (!off && ++idle >= limit * 4) {
			lock(1);
			if (!matrix || matrix_start()) { Supexec(scr_off); off = 1; }
			else idle = 0;
		}
	}
}
