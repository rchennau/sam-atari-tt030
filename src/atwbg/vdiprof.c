/* vdiprof — sampling profile of the resident xVDI driver while a program runs (video FR-8).
 * Finds xVDI's text segment in RAM by its version banner, hooks the 200 Hz MFP timer C vector, runs the
 * given program with Pexec, and counts the interrupted PC per 16-byte bucket of xVDI's text. Prints the hottest
 * buckets as text-segment offsets (match them against a local disassembly of the same driver build).
 * Usage: vdiprof PROGRAM [ARGS]   Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o vdiprof.ttp src/atwbg/vdiprof.c
 * ponytail: 200 Hz sampling, so a ~20 s run gives ~4000 samples; enough for hot spots, not for small functions.
 */
#include <mint/osbind.h>
#include <mint/sysvars.h>
#include <stdio.h>
#include <string.h>

#define SPAN 0x1df4eUL               /* xVDI text size (Jul 30 2026, 68000 build) */
#define NB (SPAN / 16 + 1)
#define VEC_TIMERC ((void **)0x114)

unsigned long prof_base __asm__("prof_base");
unsigned long prof_span __asm__("prof_span") = SPAN;
unsigned long prof_total __asm__("prof_total");
void *prof_old __asm__("prof_old");
unsigned long prof_ring[4096] __asm__("prof_ring");   /* PCs outside xVDI, last 4096 */
unsigned long prof_ri __asm__("prof_ri");
unsigned short prof_hist[NB] __asm__("prof_hist");
void prof_isr(void) __asm__("prof_isr");

/* Stacked frame at entry: SR (2), PC (4). After pushing d0: PC at 6(sp). */
__asm__(
	".text\n"
	"prof_isr:\n"
	"	move.l %d0,-(%sp)\n"
	"	addq.l #1,prof_total\n"
	"	move.l 6(%sp),%d0\n"
	"	sub.l prof_base,%d0\n"
	"	cmp.l prof_span,%d0\n"
	"	bcc.s 2f\n"
	"	lsr.l #4,%d0\n"
	"	add.l %d0,%d0\n"
	"	move.l %a0,-(%sp)\n"
	"	lea prof_hist,%a0\n"
	"	addq.w #1,0(%a0,%d0.l)\n"
	"	move.l (%sp)+,%a0\n"
	"	bra.s 1f\n"
	"2:	move.l %a0,-(%sp)\n"
	"	move.l prof_ri,%d0\n"
	"	addq.l #1,prof_ri\n"
	"	and.l #4095,%d0\n"
	"	lsl.l #2,%d0\n"
	"	lea prof_ring,%a0\n"
	"	move.l 10(%sp),0(%a0,%d0.l)\n"
	"	move.l (%sp)+,%a0\n"
	"1:	move.l (%sp)+,%d0\n"
	"	move.l prof_old,-(%sp)\n"
	"	rts\n");

static const unsigned char head[6] = {0x2a, 0x6f, 0x00, 0x04, 0x4f, 0xf9};   /* text+0: moveal sp@(4),a5; lea */
static char banner[24];

static int match(unsigned long a)
{
	return !memcmp((void *)(a + 0x94), banner, 22) && !memcmp((void *)a, head, 6)
		&& *(unsigned short *)(a + 0x11546) == 0x0c40;          /* trap #2 entry: cmpiw #115,d0 */
}

static void scan(unsigned long lo, unsigned long hi)
{
	unsigned long a;
	for (a = lo; a + SPAN < hi && !prof_base; a += 2)
		if (*(unsigned char *)(a + 0x94) == 'x' && match(a))
			prof_base = a;
}

static long find(void)
{
	scan(0x800, *(unsigned long *)0x42e);                         /* ST RAM up to phystop */
	if (!prof_base && *(unsigned long *)0x5a8 == 0x1357bd13UL)    /* ramvalid: TT RAM present */
		scan(0x01000000UL, *(unsigned long *)0x5a4);
	return 0;
}

static long hook(void) { prof_old = *VEC_TIMERC; *VEC_TIMERC = (void *)prof_isr; return 0; }
static long unhook(void) { *VEC_TIMERC = prof_old; return 0; }

int main(int argc, char **argv)
{
	char tail[128] = "";
	long rc, in = 0, i, j, best;
	int k;

	if (argc < 2) { fprintf(stderr, "usage: vdiprof PROGRAM [ARGS]\n"); return 2; }
	strcpy(banner, "yVDI 68000 Jul 30 2026");
	banner[0] = 'x';                                              /* so the scan cannot match this program's own copy */
	Supexec(find);
	if (!prof_base) { fprintf(stderr, "vdiprof: xVDI text not found\n"); return 1; }
	for (k = 2; k < argc; k++) { strcat(tail + 1, k > 2 ? " " : ""); strcat(tail + 1, argv[k]); }
	tail[0] = (char)strlen(tail + 1);
	Supexec(hook);
	rc = Pexec(0, argv[1], tail, NULL);
	Supexec(unhook);
	for (i = 0; i < (long)NB; i++)
		in += prof_hist[i];
	printf("xvdi text at 0x%lx; %s rc %ld; samples %lu, in xVDI %ld\n", prof_base, argv[1], rc, prof_total, in);
	for (k = 0; k < 40; k++) {
		for (best = -1, j = 0; j < (long)NB; j++)
			if (prof_hist[j] && (best < 0 || prof_hist[j] > prof_hist[best]))
				best = j;
		if (best < 0)
			break;
		printf("  +%05lx %5u\n", best * 16, prof_hist[best]);
		prof_hist[best] = 0;
	}
	for (i = 0; i < 4096 && i < (long)prof_ri; i++)          /* outside xVDI: 256-byte buckets, crude top 15 */
		prof_ring[i] &= ~0xffUL;
	for (k = 0; k < 15; k++) {
		unsigned long v = 0; long c, bc = 0;
		for (i = 0; i < 4096 && i < (long)prof_ri; i++) {
			if (!prof_ring[i]) continue;
			for (c = 0, j = i; j < 4096 && j < (long)prof_ri; j++) c += prof_ring[j] == prof_ring[i];
			if (c > bc) { bc = c; v = prof_ring[i]; }
		}
		if (!bc) break;
		printf("  out %08lx %5ld\n", v, bc);
		for (i = 0; i < 4096; i++) if (prof_ring[i] == v) prof_ring[i] = 0;
	}
	for (i = 0; i < 4096 && i < (long)prof_ri; i++)          /* outside xVDI: 256-byte buckets, crude top 15 */
		prof_ring[i] &= ~0xffUL;
	for (k = 0; k < 15; k++) {
		unsigned long v = 0; long c, bc = 0;
		for (i = 0; i < 4096 && i < (long)prof_ri; i++) {
			if (!prof_ring[i]) continue;
			for (c = 0, j = i; j < 4096 && j < (long)prof_ri; j++) c += prof_ring[j] == prof_ring[i];
			if (c > bc) { bc = c; v = prof_ring[i]; }
		}
		if (!bc) break;
		printf("  out %08lx %5ld\n", v, bc);
		for (i = 0; i < 4096; i++) if (prof_ring[i] == v) prof_ring[i] = 0;
	}
	return 0;
}
