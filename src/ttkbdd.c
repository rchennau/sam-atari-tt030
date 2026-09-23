/* ttkbdd.c — TT030 remote-keyboard daemon, Phase 1 core (track tt030-remote-keyboard FR-2/3/6, D2).
 *
 * Injects key frames into FreeMiNT's keyboard path via the kbdvec slot at Kbdvbase()-4 (proven in
 * Phase 0, src/kbdinj.c): the same ikbd_scan() path as the attached keyboard, so both merge.
 *
 * Frame: 2 bytes {scancode, flags}; flags bit0 = break, bit1 = heartbeat (no key).
 * Session: load the unpatched key table (D2: remote ; and [ must type ; and [), inject frames,
 * then release every key still held and restore the space-patched table — also on SIGTERM,
 * SIGINT and SIGHUP.
 *
 * usage: ttkbdd -f FILE [-t]     session from a frame file ("-" = stdin); -t: test mode, read the
 *                                 BIOS keyboard queue back afterwards and print it (steals keys), then
 *                                 inject 'a' + 0x27 and read back again (expects 'a' ' ')
 *        ttkbdd --release        recovery (ttkbd-release): break every scancode 0x01-0x72, restore
 *                                 the patched table — for after a kill -9, when held state is lost
 *   -u TABLE  unpatched table (default c:\mint\1-19-4eb\keyboard\en_uk.tbl)
 *   -p TABLE  patched table to restore (default c:\mint\1-19-4eb\keyboard.tbl)
 *
 * Phase 2 replaces -f with the TCP listener + handshake; the core below stays.
 * ponytail: no network yet; frames come from a file.
 */
#include <mint/osbind.h>
#include <mint/mintbind.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#define SC_MIN 0x01
#define SC_MAX 0x72
#define F_BREAK 1
#define F_HEARTBEAT 2
#define S_LOADKBD 27

static void (*kbdvec)(void);
static void *kbd_iorec;
static unsigned char held[SC_MAX + 1];
static const char *tbl_unpatched = "c:\\mint\\1-19-4eb\\keyboard\\en_uk.tbl";
static const char *tbl_patched = "c:\\mint\\1-19-4eb\\keyboard.tbl";
static volatile sig_atomic_t stop;

static long setup(void)
{
	kbdvec = ((void (**)(void))Kbdvbase())[-1];
	kbd_iorec = Iorec(1);
	return 0;
}

/* IPL 7 around the call: a real key interrupt must not interleave with ikbd_scan()'s ring update. */
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

static void raw(unsigned char code)
{
	cur = code;
	Supexec(inject_one);
	Syield();	/* MiNT processes the scan queue in a root timeout */
}

/* One frame. Returns 0 if applied, -1 if dropped (bad scancode). */
static int key(unsigned char sc, unsigned char flags)
{
	if (flags & F_HEARTBEAT)
		return 0;
	if (sc < SC_MIN || sc > SC_MAX)
		return -1;
	if (flags & F_BREAK) {
		raw(sc | 0x80);
		held[sc] = 0;
	} else {
		raw(sc);
		held[sc] = 1;
	}
	return 0;
}

static void release_held(void)
{
	int sc;
	for (sc = SC_MIN; sc <= SC_MAX; sc++)
		if (held[sc])
			key(sc, F_BREAK);
}

static long load_table(const char *path)
{
	long r = Ssystem(S_LOADKBD, (long)path, 0L);
	if (r)
		fprintf(stderr, "ttkbdd: S_LOADKBD %s -> %ld\n", path, r);
	return r;
}

static long load_table(const char *path);
static void release_held(void);

/* Clean up inside the handler: a blocked read() on FreeMiNT is restarted after the handler even
 * without SA_RESTART (measured on the real TT 2026-09-23), so a flag alone never ends the session.
 * ponytail: not async-signal-safe in general; fine here — single-threaded, and the only other work
 * is the same inject path. Upgrade: select() with a timeout in Phase 2's socket loop. */
static void on_signal(int sig)
{
	(void)sig;
	stop = 1;
	release_held();
	load_table(tbl_patched);
	_exit(0);
}

static void readback(void)
{
	clock_t end = clock() + CLOCKS_PER_SEC;
	int got = 0;
	while (clock() < end) {
		while (Bconstat(2)) {
			long k = Bconin(2);
			printf("KEY scan=%02lx ascii=%02lx '%c'\n", (k >> 16) & 0xff, k & 0xff,
			       (int)(k & 0xff) >= 32 ? (int)(k & 0xff) : '.');
			got++;
		}
	}
	printf("ttkbdd: %d key(s) read back\n", got);
}

static int session(const char *file, int test)
{
	unsigned char fr[2];
	int fd = strcmp(file, "-") ? open(file, O_RDONLY) : 0;
	long frames = 0, dropped = 0;

	if (fd < 0) {
		perror(file);
		return 1;
	}
	if (load_table(tbl_unpatched))
		return 1;	/* refuse to type with the wrong table rather than guess */
	while (!stop && read(fd, fr, 2) == 2) {
		frames++;
		if (key(fr[0], fr[1]))
			dropped++;
	}
	release_held();
	load_table(tbl_patched);
	printf("ttkbdd: session end, %ld frame(s), %ld dropped%s\n", frames, dropped,
	       stop ? ", stopped by signal" : "");
	if (test) {
		readback();
		/* post-check: with the session over, Shift must be released and the patched table back,
		 * so 'a' and 0x27 must read back as 'a' and ' ' */
		raw(0x1e); raw(0x9e); raw(0x27); raw(0xa7);
		readback();
	}
	return 0;
}

static int release_all(void)
{
	int sc;
	for (sc = SC_MIN; sc <= SC_MAX; sc++)
		raw(sc | 0x80);
	printf("ttkbdd: released 0x%02x-0x%02x\n", SC_MIN, SC_MAX);
	return load_table(tbl_patched) ? 1 : 0;
}

int main(int argc, char **argv)
{
	const char *file = NULL;
	int test = 0, rel = 0, i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--release"))
			rel = 1;
		else if (!strcmp(argv[i], "-t"))
			test = 1;
		else if (i + 1 < argc && !strcmp(argv[i], "-f"))
			file = argv[++i];
		else if (i + 1 < argc && !strcmp(argv[i], "-u"))
			tbl_unpatched = argv[++i];
		else if (i + 1 < argc && !strcmp(argv[i], "-p"))
			tbl_patched = argv[++i];
		else {
			fprintf(stderr, "usage: ttkbdd -f FILE [-t] [-u TABLE] [-p TABLE] | --release\n");
			return 2;
		}
	}
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_signal;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	Supexec(setup);
	if (rel)
		return release_all();
	if (!file) {
		fprintf(stderr, "ttkbdd: -f FILE required (network listener is Phase 2)\n");
		return 2;
	}
	return session(file, test);
}
