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
 * usage: ttkbdd [-l ADDR] [-k PUB] listen on ADDR:7590 (default 192.168.0.30, /etc/ttkbd.pub):
 *                                 handshake + encrypted frames from scripts/ttkbd_send.py
 *        ttkbdd -f FILE [-t]     session from a frame file ("-" = stdin); -t: test mode, read the
 *                                 BIOS keyboard queue back afterwards and print it (steals keys), then
 *                                 inject 'a' + 0x27 and read back again (expects 'a' ' ')
 *        ttkbdd --release        recovery (ttkbd-release): break every scancode 0x01-0x72, restore
 *                                 the patched table — for after a kill -9, when held state is lost
 *   -u TABLE  unpatched table (default c:\mint\1-19-4eb\keyboard\en_uk.tbl)
 *   -p TABLE  patched table to restore (default c:\mint\1-19-4eb\keyboard.tbl)
 *
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -o ttkbdd src/ttkbdd.c tools/monocypher-4.0.3/src/monocypher.c \
 *        tools/monocypher-4.0.3/src/optional/monocypher-ed25519.c -Itools/monocypher-4.0.3/src -Itools/monocypher-4.0.3/src/optional
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "monocypher.h"
#include "monocypher-ed25519.h"

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

static int session_begin(void)
{
	return load_table(tbl_unpatched) ? -1 : 0;	/* refuse to type with the wrong table */
}

static void session_end(const char *why)
{
	release_held();
	load_table(tbl_patched);
	fprintf(stderr, "ttkbdd: session end (%s)\n", why);
}

static int session_file(const char *file, int test)
{
	unsigned char fr[2];
	int fd = strcmp(file, "-") ? open(file, O_RDONLY) : 0;
	long frames = 0, dropped = 0;

	if (fd < 0) {
		perror(file);
		return 1;
	}
	if (session_begin())
		return 1;
	while (!stop && read(fd, fr, 2) == 2) {
		frames++;
		if (key(fr[0], fr[1]))
			dropped++;
	}
	session_end("file");
	printf("ttkbdd: %ld frame(s), %ld dropped\n", frames, dropped);
	if (test) {
		readback();
		/* post-check: with the session over, Shift must be released and the patched table back,
		 * so 'a' and 0x27 must read back as 'a' and ' ' */
		raw(0x1e); raw(0x9e); raw(0x27); raw(0xa7);
		readback();
	}
	return 0;
}

/* ---- network (Phase 2): handshake + XChaCha20-Poly1305 records; see scripts/ttkbd_send.py ---- */

#define SILENCE_S 3	/* heartbeat is 1 s; 3 s of nothing = link lost -> release (FR-6) */
#define AUTH_S 60	/* time allowed for the fractal side to answer the hello */

static uint8_t f_verify_pub[32];	/* fractal's Ed25519 public key (/etc/ttkbd.pub) */

/* Read exactly n bytes, giving up after `secs` of silence. 0 ok, -1 closed/error, -2 timeout. */
static int recv_exact(int fd, uint8_t *b, int n, int secs)
{
	while (n > 0) {
		fd_set r;
		struct timeval tv = {secs, 0};
		int k;
		FD_ZERO(&r);
		FD_SET(fd, &r);
		k = select(fd + 1, &r, NULL, NULL, &tv);
		if (k == 0)
			return -2;
		if (k < 0)
			return -1;
		k = read(fd, b, n);
		if (k <= 0)
			return -1;
		b += k;
		n -= k;
	}
	return 0;
}

static int urandom(uint8_t *b, int n)
{
	int fd = open("/dev/urandom", O_RDONLY), ok;
	if (fd < 0)
		return -1;
	ok = read(fd, b, n) == n;
	close(fd);
	return ok ? 0 : -1;
}

static void serve(int c)
{
	uint8_t hello[49], auth[96], msg[86], shared[32], sess_key[32], tt_sk[32], nonce[24], rec[18], fr[2];
	uint64_t ctr = 0;
	int r, i;
	const char *why;

	/* ponytail: keypair made per connection (~one X25519 at connect); precompute while idle if slow */
	if (urandom(hello + 1, 16) || urandom(tt_sk, 32)) {
		fprintf(stderr, "ttkbdd: /dev/urandom failed\n");
		return;
	}
	hello[0] = 1;
	crypto_x25519_public_key(hello + 17, tt_sk);
	if (write(c, hello, sizeof hello) != sizeof hello)
		return;
	if (recv_exact(c, auth, sizeof auth, AUTH_S)) {
		fprintf(stderr, "ttkbdd: no auth\n");
		goto out;
	}
	memcpy(msg, hello + 1, 48);	/* chal | tt_pub */
	memcpy(msg + 48, auth, 32);	/* f_pub */
	memcpy(msg + 80, "ttkbd1", 6);
	if (crypto_ed25519_check(auth + 32, f_verify_pub, msg, sizeof msg)) {
		fprintf(stderr, "ttkbdd: bad signature, refused\n");
		goto out;
	}
	crypto_x25519(shared, tt_sk, auth);
	{	/* key = BLAKE2b-256(shared | chal | tt_pub | f_pub) */
		uint8_t kdf[112];
		memcpy(kdf, shared, 32);
		memcpy(kdf + 32, hello + 1, 48);
		memcpy(kdf + 80, auth, 32);
		crypto_blake2b(sess_key, 32, kdf, sizeof kdf);
		crypto_wipe(kdf, sizeof kdf);
	}
	crypto_wipe(shared, sizeof shared);
	if (session_begin())
		goto out;
	fprintf(stderr, "ttkbdd: session up\n");
	for (;;) {
		r = recv_exact(c, rec, sizeof rec, SILENCE_S);
		if (r) {
			why = r == -2 ? "silence" : "closed";
			break;
		}
		memset(nonce, 0, sizeof nonce);
		for (i = 0; i < 8; i++)
			nonce[i] = (uint8_t)(ctr >> (8 * i));
		if (crypto_aead_unlock(fr, rec, sess_key, nonce, NULL, 0, rec + 16, 2)) {
			why = "bad record";	/* tampered, replayed or reordered: never typed */
			break;
		}
		ctr++;
		key(fr[0], fr[1]);
	}
	session_end(why);
out:
	crypto_wipe(tt_sk, sizeof tt_sk);
	crypto_wipe(sess_key, sizeof sess_key);
}

static int listen_loop(const char *addr, int port, const char *pubfile)
{
	struct sockaddr_in sa;
	int s, one = 1, fd = open(pubfile, O_RDONLY);

	if (fd < 0 || read(fd, f_verify_pub, 32) != 32) {
		fprintf(stderr, "ttkbdd: cannot read 32-byte key %s\n", pubfile);
		return 1;
	}
	close(fd);
	s = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = inet_addr(addr);
	if (bind(s, (struct sockaddr *)&sa, sizeof sa) || listen(s, 1)) {
		perror("ttkbdd: bind/listen");
		return 1;
	}
	fprintf(stderr, "ttkbdd: listening on %s:%d\n", addr, port);
	for (;;) {
		int c = accept(s, NULL, NULL);
		if (c < 0)
			continue;
		if (setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one))
			fprintf(stderr, "ttkbdd: TCP_NODELAY not set\n");
		serve(c);	/* one session at a time */
		close(c);
	}
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
	const char *file = NULL, *addr = "192.168.0.30", *pubfile = "/etc/ttkbd.pub";
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
		else if (i + 1 < argc && !strcmp(argv[i], "-l"))
			addr = argv[++i];
		else if (i + 1 < argc && !strcmp(argv[i], "-k"))
			pubfile = argv[++i];
		else {
			fprintf(stderr, "usage: ttkbdd [-l ADDR] [-k PUBFILE] | -f FILE [-t] | --release  [-u TABLE] [-p TABLE]\n");
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
	if (file)
		return session_file(file, test);
	return listen_loop(addr, 7590, pubfile);
}
