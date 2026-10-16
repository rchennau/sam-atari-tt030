/* atwmono — use the ATW800/2 copy engine for mono (1-plane) screen-to-screen blits (video FR-8).
 * xVDI (Jul 30 2026, 68000 build) drives the card's copy engine only at 8 planes and up; in mono every
 * vro_cpyfm is a CPU copy over the bus. This resident hook replaces the mono depth table's blit slot with a
 * routine that sends byte-aligned, S_ONLY, screen-to-screen copies to the engine (exactly as the 8-bit path
 * does) and passes everything else to the original routine.
 * Usage: atwmono      install (stays resident; a reboot removes it)
 *        atwmono -u   restore the original slot (the resident copy stays in memory, unused)
 * Refuses unless the driver's bytes match the build it was written against.
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o atwmono.ttp src/atwbg/atwmono.c
 * ponytail: byte-aligned rects only (left/right edges multiples of 8 px); add CPU edge bytes + engine middle
 * if misaligned window moves matter.
 */
#include <mint/osbind.h>
#include <stdio.h>
#include <string.h>

#define SPAN 0x1df4eUL               /* xVDI text size */
#define V_TABLE 0x1ecceUL             /* BSS: pointer to the live op table (a RAM copy made at init) */
#define SLOT_BLIT 0x58
#define F_BLIT1 0x15f4UL             /* original mono blit */
#define V_ENGINE 0x219f0UL           /* BSS: pointer to the copy engine's register window */

typedef struct { void *addr; short w, h, wdw, stand, planes, r1, r2, r3; } MFDB;

unsigned long mono_orig __asm__("mono_orig");
unsigned long mono_xvdi __asm__("mono_xvdi");
unsigned long mono_hits __asm__("mono_hits");
void mono_blit(void) __asm__("mono_blit");
long mono_blit_c(unsigned char *ws) __asm__("mono_blit_c");

/* Slot entry: a0 = workstation. Handled -> rts; otherwise tail-jump to the original with a0 intact. */
__asm__(
	".text\n"
	"mono_blit:\n"
	"	move.l %a0,-(%sp)\n"
	"	jsr mono_blit_c\n"
	"	move.l (%sp)+,%a0\n"
	"	tst.l %d0\n"
	"	bne.s 1f\n"
	"	move.l mono_orig,-(%sp)\n"
	"1:	rts\n");

#define W(o) (*(short *)(ws + (o)))
#define L(o) (*(unsigned long *)(ws + (o)))

long mono_blit_c(unsigned char *ws)
{
	short *contrl = (short *)L(0x8dc), *intin = (short *)L(0x8e0), *p = (short *)L(0x8e4);
	MFDB *src = *(MFDB **)((char *)contrl + 14), *dst = *(MFDB **)((char *)contrl + 18);
	short sx = p[0], sy = p[1], w = p[2] - p[0] + 1, h = p[3] - p[1] + 1, dx = p[4], dy = p[5];
	long pitch = W(0x8c2), so, d, spin = 4000000;
	volatile unsigned char *e;

	if (intin[0] != 3 || src->addr || dst->addr || W(0x4b0) || W(0x918) != 1)
		return 0;
	if (p[6] - p[4] + 1 != w || p[7] - p[5] + 1 != h)
		return 0;
	if (W(0x4b4)) {                          /* clip the destination, move the source with it */
		short cx1 = W(0x4b6), cy1 = W(0x4b8), cx2 = W(0x4ba), cy2 = W(0x4bc);
		if (dx < cx1) { sx += cx1 - dx; w -= cx1 - dx; dx = cx1; }
		if (dy < cy1) { sy += cy1 - dy; h -= cy1 - dy; dy = cy1; }
		if (dx + w - 1 > cx2) w = cx2 - dx + 1;
		if (dy + h - 1 > cy2) h = cy2 - dy + 1;
		if (w <= 0 || h <= 0)
			return 1;
	}
	if ((sx | dx | w) & 7 || sx < 0 || sy < 0 || dx < 0 || dy < 0)
		return 0;
	e = *(volatile unsigned char **)(mono_xvdi + V_ENGINE);
	if (!e)
		return 0;
	so = sy * pitch + sx / 8;
	d = dy * pitch + dx / 8;
	*(volatile short *)(e + 0x10c) = w / 8;
	*(volatile short *)(e + 0x10e) = h;
	if (d > so) {                            /* overlap-safe: copy from the last byte backwards */
		*(volatile long *)(e + 0x104) = d + (h - 1) * pitch + w / 8 - 1;
		*(volatile long *)(e + 0x100) = so + (h - 1) * pitch + w / 8 - 1;
		*(volatile short *)(e + 0x108) = -pitch;
		*(volatile short *)(e + 0x10a) = -pitch;
		*(volatile short *)(e + 0x110) = 3;
	} else {
		*(volatile long *)(e + 0x104) = d;
		*(volatile long *)(e + 0x100) = so;
		*(volatile short *)(e + 0x108) = pitch;
		*(volatile short *)(e + 0x10a) = pitch;
		*(volatile short *)(e + 0x110) = 1;
	}
	while (*(volatile short *)(e + 0x110) && --spin)   /* bounded: a stuck engine must not hang the VDI */
		;
	mono_hits++;
	return 1;
}

static const unsigned char head[6] = {0x2a, 0x6f, 0x00, 0x04, 0x4f, 0xf9};
static char banner[24];
static int uninstall;
static long rc;

static int match(unsigned long a)
{
	return !memcmp((void *)(a + 0x94), banner, 22) && !memcmp((void *)a, head, 6)
		&& *(unsigned short *)(a + 0x11546) == 0x0c40;
}

static void scan(unsigned long lo, unsigned long hi)
{
	unsigned long a;
	for (a = lo; a + SPAN < hi && !mono_xvdi; a += 2)
		if (*(unsigned char *)(a + 0x94) == 'x' && match(a))
			mono_xvdi = a;
}

static long patch(void)
{
	unsigned long *slot;
	scan(0x800, *(unsigned long *)0x42e);
	if (!mono_xvdi && *(unsigned long *)0x5a8 == 0x1357bd13UL)
		scan(0x01000000UL, *(unsigned long *)0x5a4);
	if (!mono_xvdi)
		return rc = 1;
	slot = (unsigned long *)(*(unsigned long *)(mono_xvdi + V_TABLE) + SLOT_BLIT);
	if (uninstall) {
		if (*slot == mono_xvdi + F_BLIT1)
			return rc = 3;
		*slot = mono_xvdi + F_BLIT1;
		return rc = 0;
	}
	if (*slot != mono_xvdi + F_BLIT1 || *(unsigned short *)(mono_xvdi + F_BLIT1) != 0x48e7)
		return rc = 2;
	mono_orig = *slot;
	*slot = (unsigned long)mono_blit;
	return rc = 0;
}

int main(int argc, char **argv)
{
	extern long _PgmSize;
	uninstall = argc > 1 && !strcmp(argv[1], "-u");
	strcpy(banner, "yVDI 68000 Jul 30 2026");
	banner[0] = 'x';
	Supexec(patch);
	switch (rc) {
	case 1: printf("atwmono: xVDI Jul 30 2026 68000 build not found\n"); return 1;
	case 2: printf("atwmono: mono blit slot not as expected (already installed, or another driver build)\n"); return 1;
	case 3: printf("atwmono: not installed\n"); return 0;
	}
	if (uninstall) { printf("atwmono: original mono blit restored\n"); return 0; }
	printf("atwmono: xVDI at 0x%lx, mono blits use the copy engine\n", mono_xvdi);
	fflush(stdout);
	Ptermres(_PgmSize, 0);
	return 0;
}
