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
 *   no arguments: types "ab" + Shift+a (1e 9e 30 b0 2a 1e 9e aa) and reads back
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

static long setup(void)
{
	kbdvec = ((void (**)(void))Kbdvbase())[-1];
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
	static const unsigned char deflt[] = {0x1e, 0x9e, 0x30, 0xb0, 0x2a, 0x1e, 0x9e, 0xaa};
	int i;

	Supexec(setup);
	printf("kbdinj: kbdvec=%08lx iorec=%08lx\n", (unsigned long)kbdvec, (unsigned long)kbd_iorec);
	if (argc < 2) {
		for (i = 0; i < (int)sizeof deflt; i++)
			inject(deflt[i]);
		readback();
		return 0;
	}
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-r"))
			readback();
		else if (!strcmp(argv[i], "-k") && i + 1 < argc) {
			long r = Ssystem(27 /* S_LOADKBD */, (long)argv[i + 1], 0L);
			printf("kbdinj: S_LOADKBD %s -> %ld\n", argv[i + 1], r);
			i++;
		} else
			inject((unsigned char)strtoul(argv[i], NULL, 16));
	}
	return 0;
}
