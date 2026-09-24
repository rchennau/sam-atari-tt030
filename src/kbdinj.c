/* kbdinj.c — Phase 0 spike (track tt030-remote-keyboard, FR-1 path (a)).
 *
 * Injects scancodes through the undocumented kbdvec vector at Kbdvbase()-4, the slot the
 * ACIA handler jumps to for every keyboard byte. Under FreeMiNT 1.19 that slot chains to
 * kbdvec_handler -> ikbd_scan(scancode, kbd_iorec) (sys/arch/intr.S, sys/keyboard.c), so an
 * injected byte takes the same path as a real key: keyboard.tbl, shift state, autorepeat.
 * Under TOS the slot is TOS's own handler, which takes d0 = scancode, a0 = keyboard IOREC.
 *
 * usage: kbdinj [hex | -r | -k FILE]...   run in order, e.g. kbdinj 1e 9e -r
 *   hex: inject that scancode (make; |0x80 = break)   -r: read back the BIOS keyboard queue
 *   -k FILE: Ssystem(S_LOADKBD, FILE) — swap the key table at runtime (root)
 *   -m DX DY BUTTONS: relative mouse packet via mousevec (-128..127; buttons 1 right, 2 left)
 *   -p: print the pointer position (Line-A GCURX/GCURY)
 *   no arguments: usage error (there is deliberately no default action)
 *
 * ponytail: spike only — no network, no table swap; ttkbdd replaces it in Phase 1.
 */
#include <mint/osbind.h>
#include <mint/mintbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void (*kbdvec)(void);
static void *kbd_iorec;

static void (*mousevec)(void);

static long setup(void)
{
	kbdvec = ((void (**)(void))Kbdvbase())[-1];
	mousevec = ((void (**)(void))Kbdvbase())[4];	/* midivec vkbderr vmiderr statvec MOUSEVEC */
	kbd_iorec = Iorec(1);
	return 0;
}

/* One scancode into kbdvec with the ACIA/MFP masked, so a real key interrupt cannot
 * interleave with ikbd_scan()'s ring-buffer update. */
static unsigned char cur;
static long inject_one(void)
{
	__asm__ volatile(
		"move.w %%sr,-(%%sp)\n\t"
		"ori.w #0x0700,%%sr\n\t"
		"moveq #0,%%d0\n\t"
		"move.b %0,%%d0\n\t"
		"move.l %1,%%a0\n\t"
		"move.l %2,%%a1\n\t"
		"jsr (%%a1)\n\t"
		"move.w (%%sp)+,%%sr"
		:
		: "m"(cur), "m"(kbd_iorec), "m"(kbdvec)
		: "d0", "d1", "d2", "a0", "a1", "a2", "cc", "memory");
	return 0;
}

/* ---- mouse spike (track tt030-remote-keyboard, FR-8): a relative IKBD packet through
 * Kbdvbase()->mousevec, the path FreeMiNT's own keyboard mouse-emulation uses (sys/keyboard.c:238,
 * send_packet in sys/arch/intr.S: a0 = packet, a1 = end, jsr vec). ---- */
static signed char mpkt[3];
static long mouse_one(void)
{
	if (!mousevec)
		return -1;
	/* Load the vector BEFORE pushing SR: a stack-relative operand read after the push points two bytes
	 * off — the first build jumped to 0 that way (SIGSEGV / ADDRESS ERROR, 2026-09-23). Globals only. */
	__asm__ volatile(
		"move.l %1,%%a2\n\t"
		"lea %0,%%a0\n\t"
		"lea 3(%%a0),%%a1\n\t"
		"move.w %%sr,-(%%sp)\n\t"
		"ori.w #0x0700,%%sr\n\t"
		"jsr (%%a2)\n\t"
		"move.w (%%sp)+,%%sr"
		:
		: "m"(mpkt), "m"(mousevec)
		: "d0", "d1", "d2", "a0", "a1", "a2", "cc", "memory");
	return 0;
}

/* Line-A GCURX/GCURY: the mouse position the VDI/AES track. */
static void pointer(short *x, short *y)
{
	register char *base __asm__("a0");
	__asm__ volatile(".word 0xA000" : "=r"(base) : : "d0", "a1", "a2", "d1", "d2", "cc");
	*x = *(short *)(base - 602);
	*y = *(short *)(base - 600);
}

static void readback(void)
{
	long k;
	int got = 0;
	clock_t end = clock() + CLOCKS_PER_SEC;	/* ~1 s; clock() is _hz_200, works under TOS too */
	while (clock() < end && got < 16) {
		while (Bconstat(2)) {
			k = Bconin(2);
			printf("KEY scan=%02lx ascii=%02lx '%c'\n", (k >> 16) & 0xff, k & 0xff,
			       (int)(k & 0xff) >= 32 ? (int)(k & 0xff) : '.');
			got++;
		}
	}
	printf("kbdinj: %d key(s) read back\n", got);
}

static void inject(unsigned char c)
{
	cur = c;
	Supexec(inject_one);
	Syield();	/* MiNT processes the queue in a root timeout */
}

/* Arguments run in order: hex = inject that scancode, -r = read back now,
 * -k FILE = Ssystem(S_LOADKBD) — reload the key table at runtime (root; D2 swap). */
int main(int argc, char **argv)
{
	int i;

	Supexec(setup);
	printf("kbdinj: kbdvec=%08lx iorec=%08lx\n", (unsigned long)kbdvec, (unsigned long)kbd_iorec);
	/* No default action: an empty argument list (a caller's mapping failed) once typed "abA" and hung
	 * the TT at 100 % CPU in the read-back loop, twice (2026-09-23). */
	if (argc < 2) {
		fprintf(stderr, "usage: kbdinj [hex | -r | -k FILE]...\n");
		return 2;
	}
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-r"))
			readback();
		else if (!strcmp(argv[i], "-p")) {
			short x, y;
			pointer(&x, &y);
			printf("pointer %d %d\n", x, y);
		} else if (!strcmp(argv[i], "-m") && i + 3 < argc) {	/* -m DX DY BUTTONS (1 = right, 2 = left) */
			mpkt[0] = (signed char)(0xf8 | (atoi(argv[i + 3]) & 3));
			mpkt[1] = (signed char)atoi(argv[i + 1]);
			mpkt[2] = (signed char)atoi(argv[i + 2]);
			Supexec(mouse_one);
			Syield();
			i += 3;
		}
		else if (!strcmp(argv[i], "-k") && i + 1 < argc) {
			long r = Ssystem(27 /* S_LOADKBD */, (long)argv[i + 1], 0L);
			printf("kbdinj: S_LOADKBD %s -> %ld\n", argv[i + 1], r);
			i++;
		} else
			inject((unsigned char)strtoul(argv[i], NULL, 16));
	}
	return 0;
}
