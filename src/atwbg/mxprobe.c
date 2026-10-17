/* mxprobe — read-only facts FR-9 (matrix saver) needs: Line-A geometry, Logbase/Physbase, the hardware LUT at
 * 0xFEDFF000, the 8x16 system font header, and the 68030's VRAM read rate. Writes nothing to the card.
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o mxprobe.ttp src/atwbg/mxprobe.c */
#include <mint/osbind.h>
#include <mint/linea.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static unsigned char lut[64];
static unsigned char *buf;
static unsigned long t0, t1;
static long rd(void) {
	int i;
	for (i = 0; i < 64; i++) lut[i] = ((volatile unsigned char *)0xFEDFF000UL)[i];
	return 0;
}
static long copy(void) {
	volatile unsigned long *s = (volatile unsigned long *)0xFEC00000UL;
	unsigned long *d = (unsigned long *)buf;
	long i;
	t0 = *(volatile unsigned long *)0x4BAUL;
	for (i = 0; i < 1920L * 1080 / 4; i++) d[i] = s[i];
	t1 = *(volatile unsigned long *)0x4BAUL;
	return 0;
}

int main(void)
{
	int i;
	__FONT *f;
	linea0();
	printf("linea xmax %d ymax %d planes %d vwrap %d bytes_lin %d cel_ht %d fnt_ad %p\n", V_X_MAX, V_Y_MAX, VPLANES,
	       VWRAP, V_BYTES_LIN, V_CEL_HT, (void *)V_FNT_AD);
	printf("logbase %p physbase %p\n", Logbase(), Physbase());
	for (i = 0; i < 3; i++) {
		f = __fonts[i];
		if (f) printf("font %d: id %d size %d first %d last %d fw %d fh %d dat %p off %p\n", i, f->font_id, f->size,
		              f->first_ade, f->last_ade, f->form_width, f->form_height, (void *)f->dat_table, (void *)f->off_table);
	}
	Supexec(rd);
	printf("lut:");
	for (i = 0; i < 64; i++) printf(" %02x", lut[i]);
	printf("\n");
	if ((buf = malloc(1920L * 1080))) {
		Supexec(copy);
		printf("vram read 2073600 B in %lu ticks (200 Hz)\n", t1 - t0);
	}
	return 0;
}
