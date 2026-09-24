/* ttkbdd.c — TT030 remote-keyboard daemon, Phase 1 core (track tt030-remote-keyboard FR-2/3/6, D2).
 *
 * Injects key frames into FreeMiNT's keyboard path via the kbdvec slot at Kbdvbase()-4 (proven in
 * Phase 0, src/kbdinj.c): the same ikbd_scan() path as the attached keyboard, so both merge.
 *
 * Frame (protocol v2): 4 bytes {flags, code, dx, dy}; flags bit0 = key up, bit1 = heartbeat, bit3 = quit
 * (serial), bit4 = mouse (code = buttons 1 right / 2 left, dx/dy signed). TT -> fractal: 0x06 ack,
 * 0x05 = pointer reached the hot corner (-c N: 1 top-right [default], 2 top-left, 3/4 bottom, 0 off).
 * Session: load the unpatched key table (D2: remote ; and [ must type ; and [), inject frames,
 * then release every key still held and restore the space-patched table — also on SIGTERM,
 * SIGINT and SIGHUP.
 *
 * usage: ttkbdd [-l ADDR] [-k PUB] listen on ADDR:7590 (default 192.168.0.30, /etc/ttkbd.pub):
 *                                 handshake + encrypted frames from scripts/ttkbd_send.py
 *        ttkbdd -s /dev/ttyS1    raw serial on Modem 2 (D4): frames {0xA5, scancode, flags}, no crypto;
 *                                 stop ttygetty first — Modem 2 is otherwise the console
 *        ttkbdd --bench N        time N frame decrypts and N injects (Shift pairs; types nothing)
 *        ttkbdd -f FILE [-t] [-d MS]  session from a frame file, MS between frames ("-" = stdin); -t: test mode, read the
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
#include <sys/times.h>
#include <termios.h>
#include <sys/ioctl.h>
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
#define F_QUIT 8	/* serial only: end the session and exit, so ttygetty respawns the console */
#define F_MOUSE 0x10	/* code = buttons (1 right, 2 left), dx/dy signed (FR-8) */
#define MSG_ACK 0x06	/* TT -> fractal: frame injected (NFR-1 timing) */
#define MSG_CORNER 0x05	/* TT -> fractal: pointer reached the hot corner, hand control back (FR-10) */
#define S_LOADKBD 27
#define KBRATE_BOOT 0x0f02	/* delay 15, rate 2 (1/50 s); confirmed on the TT by --bench */

static void (*kbdvec)(void);
static void (*mousevec)(void);
static void *kbd_iorec;
static unsigned char held[SC_MAX + 1];
static const char *tbl_unpatched = "c:\\mint\\1-19-4eb\\keyboard\\en_uk.tbl";
static const char *tbl_patched = "c:\\mint\\1-19-4eb\\keyboard.tbl";
static volatile sig_atomic_t stop;

static long setup(void)
{
	kbdvec = ((void (**)(void))Kbdvbase())[-1];
	mousevec = ((void (**)(void))Kbdvbase())[4];	/* midivec vkbderr vmiderr statvec MOUSEVEC */
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

/* At most one frame per 20 ms (50 frames/s, 25 keys/s), sleeping only for what is left of the 20 ms.
 * Two limits sit downstream: ikbd_scan()'s 16-entry ring, drained once per 5 ms tick and silently
 * dropped when full, and the application's own key buffer. Injecting back to back lost keys (NFR-2,
 * 2026-09-23: 4 of 2,700 after network stalls); capping only at 8 per tick protected the ring but
 * scrambled the backlog queued during the handshake (2,600 of 2,700). A fixed 20 ms wait per frame
 * passed NFR-2 but cost 5.2 ms CPU each (--bench, 23.8 % at 10 keys/s); spaced typing now skips it. */
static void raw(unsigned char code)
{
	static clock_t last, min, us_per_tick;
	struct tms t;
	clock_t now = times(&t);

	if (!min) {	/* sysconf once, not per frame */
		us_per_tick = 1000000 / sysconf(_SC_CLK_TCK);
		min = 20000 / us_per_tick;
	}
	if (now - last < min)
		usleep((min - (now - last)) * us_per_tick);
	cur = code;
	Supexec(inject_one);
	/* read the clock again: usleep oversleeps, and assuming exactly 20 ms had passed let the next
	 * frame follow an inject back to back — 7 of 2,700 keys lost (2026-09-23) */
	last = times(&t);
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

/* ---- mouse (FR-8): relative IKBD packets through Kbdvbase()->mousevec, as FreeMiNT's own mouse
 * emulation does (sys/keyboard.c:238). Proven with src/kbdinj.c -m on the real TT, MEMPROT on. ---- */
static signed char mpkt[3];
static unsigned char mbuttons;	/* buttons we hold down on the TT, released on session end (FR-11) */

static long mouse_one(void)
{
	/* load the vector before pushing SR: a stack-relative operand read after the push is 2 bytes off */
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

static void mouse(unsigned char buttons, signed char dx, signed char dy)
{
	mpkt[0] = (signed char)(0xf8 | (buttons & 3));
	mpkt[1] = dx;
	mpkt[2] = dy;
	Supexec(mouse_one);
	mbuttons = buttons & 3;
}

static void release_mouse(void)
{
	if (mbuttons)
		mouse(0, 0, 0);
}

/* Line-A: pointer position and screen size, as the VDI/AES see them */
static short *linea;
static void pointer(short *x, short *y, short *w, short *h)
{
	if (!linea) {
		register char *base __asm__("a0");
		__asm__ volatile(".word 0xA000" : "=r"(base) : : "d0", "a1", "a2", "d1", "d2", "cc");
		linea = (short *)base;
	}
	*x = *(short *)((char *)linea - 602);	/* GCURX */
	*y = *(short *)((char *)linea - 600);	/* GCURY */
	*w = *(short *)((char *)linea - 12);	/* V_REZ_HZ */
	*h = *(short *)((char *)linea - 4);	/* V_REZ_VT */
}

static int corner = 1;	/* -c: 0 off, 1 top-right, 2 top-left, 3 bottom-right, 4 bottom-left */

static int in_corner(void)
{
	short x, y, w, h, e = 2;
	if (!corner)
		return 0;
	pointer(&x, &y, &w, &h);
	switch (corner) {
	case 1: return x >= w - e && y <= e;
	case 2: return x <= e && y <= e;
	case 3: return x >= w - e && y >= h - e;
	default: return x <= e && y >= h - e;
	}
}

static void release_held(void);
static void session_end(const char *why);

/* One v2 frame {flags, code, dx, dy}. reply_fd < 0: no replies (file mode).
 * Returns 1 on a quit frame, 0 otherwise. */
static int frame(const uint8_t f[4], int reply_fd)
{
	int ok;
	if (f[0] & F_QUIT)
		return 1;
	if (f[0] & F_HEARTBEAT)
		return 0;
	if (f[0] & F_MOUSE) {
		mouse(f[1], (signed char)f[2], (signed char)f[3]);
		if (reply_fd >= 0 && in_corner() && write(reply_fd, "\005", 1) == 1)
			fprintf(stderr, "ttkbdd: hot corner — control back to fractal\n");
		return 0;
	}
	ok = key(f[1], f[0]) == 0;
	if (ok && reply_fd >= 0 && write(reply_fd, "\006", 1) != 1)
		;	/* ack for NFR-1 timing only */
	return 0;
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
static void restore_kbrate(void);

/* Clean up inside the handler: a blocked read() on FreeMiNT is restarted after the handler even
 * without SA_RESTART (measured on the real TT 2026-09-23), so a flag alone never ends the session.
 * ponytail: not async-signal-safe in general; fine here — single-threaded, and the only other work
 * is the same inject path. Upgrade: select() with a timeout in Phase 2's socket loop. */
static void on_signal(int sig)
{
	(void)sig;
	stop = 1;
	release_held();
	release_mouse();
	restore_kbrate();
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

/* Key repeat comes from the sender during a session: the TT's own repeat fires whenever a break is
 * late (network stall, busy CPU) and doubled keys (NFR-2, 2026-09-23). Kbrate delay 255 = 5.1 s. */
static long saved_kbrate = -1;

static int session_begin(void)
{
	if (load_table(tbl_unpatched))
		return -1;	/* refuse to type with the wrong table */
	saved_kbrate = Kbrate(255, -1) & 0xffff;
	return 0;
}

static void restore_kbrate(void)
{
	if (saved_kbrate >= 0)
		Kbrate((short)(saved_kbrate >> 8), (short)(saved_kbrate & 0xff));
	saved_kbrate = -1;
}

static void session_end(const char *why)
{
	release_held();
	release_mouse();
	restore_kbrate();
	load_table(tbl_patched);
	fprintf(stderr, "ttkbdd: session end (%s)\n", why);
}

static long pace_ms;	/* -d: delay between frames in file mode (NFR-2 replay without the network) */

static int session_file(const char *file, int test)
{
	uint8_t fr[4];
	int fd = strcmp(file, "-") ? open(file, O_RDONLY) : 0;
	long frames = 0, dropped = 0;

	if (fd < 0) {
		perror(file);
		return 1;
	}
	if (session_begin())
		return 1;
	while (!stop && read(fd, fr, 4) == 4) {
		frames++;
		if (!(fr[0] & (F_MOUSE | F_HEARTBEAT | F_QUIT)) && (fr[1] < SC_MIN || fr[1] > SC_MAX))
			dropped++;
		frame(fr, -1);
		if (pace_ms)
			usleep(pace_ms * 1000);
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

static int recv_exact(int fd, uint8_t *b, int n, int secs);

/* --bench N: per-frame cost on this CPU (NFR-3), wall and process CPU (times(): user+system) per op.
 * Injects Shift make/break pairs, which type nothing. */
static void bench_report(const char *what, int n, clock_t w0, struct tms *c0)
{
	struct tms c1;
	clock_t w1 = times(&c1);
	double tck = sysconf(_SC_CLK_TCK);
	printf("ttkbdd: %-28s wall %6.2f ms  cpu %6.2f ms (user %.2f sys %.2f)\n", what,
	       (w1 - w0) * 1000.0 / tck / n,
	       ((c1.tms_utime - c0->tms_utime) + (c1.tms_stime - c0->tms_stime)) * 1000.0 / tck / n,
	       (c1.tms_utime - c0->tms_utime) * 1000.0 / tck / n,
	       (c1.tms_stime - c0->tms_stime) * 1000.0 / tck / n);
}

static int bench(int n)
{
	uint8_t key32[32] = {1}, nonce[24] = {0}, pt[2] = {0x2a, 0}, mac[16], ct[2], out[2], rec[18];
	struct tms c0;
	clock_t w0;
	int i, bad = 0, pfd[2];

	printf("ttkbdd: Kbrate now 0x%04lx (boot default assumed 0x%04x)\n", (long)Kbrate(-1, -1) & 0xffff, KBRATE_BOOT);
	crypto_aead_lock(ct, mac, key32, nonce, NULL, 0, pt, 2);
	w0 = times(&c0);
	for (i = 0; i < n; i++)
		bad |= crypto_aead_unlock(out, mac, key32, nonce, NULL, 0, ct, 2);
	bench_report(bad ? "aead_unlock (FAILED)" : "aead_unlock", n, w0, &c0);

	w0 = times(&c0);
	for (i = 0; i < n; i++) {
		cur = i & 1 ? 0xaa : 0x2a;
		Supexec(inject_one);
	}
	cur = 0xaa;
	Supexec(inject_one);
	bench_report("Supexec inject only", n, w0, &c0);

	w0 = times(&c0);
	for (i = 0; i < n; i++)
		usleep(5000);
	bench_report("usleep(5000)", n, w0, &c0);

	w0 = times(&c0);
	for (i = 0; i < n; i++)
		Syield();
	bench_report("Syield", n, w0, &c0);

	if (pipe(pfd) == 0) {
		w0 = times(&c0);
		for (i = 0; i < n; i++) {
			if (write(pfd[1], rec, sizeof rec) != sizeof rec || recv_exact(pfd[0], rec, sizeof rec, 1))
				bad = 1;
		}
		bench_report("pipe write + select/read 18B", n, w0, &c0);
	}
	return bad;
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
	uint8_t hello[49], auth[96], msg[86], shared[32], sess_key[32], tt_sk[32], nonce[24], rec[20], fr[4];
	uint64_t ctr = 0;
	int r, i;
	const char *why;

	/* ponytail: keypair made per connection (~one X25519 at connect); precompute while idle if slow */
	if (urandom(hello + 1, 16) || urandom(tt_sk, 32)) {
		fprintf(stderr, "ttkbdd: /dev/urandom failed\n");
		return;
	}
	hello[0] = 2;	/* protocol v2: 4-byte frames {flags, code, dx, dy} */
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
		if (crypto_aead_unlock(fr, rec, sess_key, nonce, NULL, 0, rec + 16, 4)) {
			why = "bad record";	/* tampered, replayed or reordered: never typed */
			break;
		}
		ctr++;
		frame(fr, c);
	}
	session_end(why);
out:
	crypto_wipe(tt_sk, sizeof tt_sk);
	crypto_wipe(sess_key, sizeof sess_key);
}

/* ---- raw serial (decision D4): 3-byte frames {0xA5, scancode, flags} on Modem 2, no crypto — the
 * null-modem cable is the trust boundary. A "session" is a burst of activity: the first frame after
 * idle loads the unpatched table, 3 s of silence releases keys and restores it. ---- */
#define SYNC 0xA5	/* serial frame: {SYNC, flags, code, dx, dy} */
static int verbose;	/* -v: hex-dump every byte received (serial diagnosis) */

static int serial_loop(const char *dev)
{
	struct termios tio;
	uint8_t b[5];
	/* ttygetty's recipe for this port (src/ttygetty.c, measured 2026-09-13): open non-blocking until
	 * CLOCAL is set — the cable has no DCD and a blocking open can wait for carrier — and clear the SCC
	 * driver's own XON/XOFF and RTS/CTS through TIOCSFLAGSB, which termios alone cannot reach. */
	int fd, in_session = 0, r;
	long fb[2] = { -1, 0 };

	/* "-S": the line is already open on stdin, set up by ttygetty (run as `ttygetty /dev/ttyS1 38400
	 * ttkbdd -S`) — the one path proven to receive on this port. Opening the device here from another
	 * session read nothing at all, even with ttygetty's own open/flag recipe (2026-09-23). */
	if (!strcmp(dev, "-")) {
		fd = 0;
	} else {
		setsid();
		fd = open(dev, O_RDWR | O_NONBLOCK);
		if (fd >= 0)
			ioctl(fd, TIOCSCTTY, 0);
	}
	if (fd < 0 || tcgetattr(fd, &tio)) {
		perror(dev);
		return 1;
	}
	cfmakeraw(&tio);
	tio.c_cc[VMIN] = 1;	/* MiNTLib's cfmakeraw may leave these unset */
	tio.c_cc[VTIME] = 0;
	tio.c_cflag |= CLOCAL | CREAD;
	tio.c_cflag &= ~CRTSCTS;
	cfsetispeed(&tio, B38400);	/* scc.xdd refuses 57600 (measured 2026-09-13) */
	cfsetospeed(&tio, B38400);
	if (tcsetattr(fd, TCSANOW, &tio)) {
		perror("ttkbdd: tcsetattr");
		return 1;
	}
	if (ioctl(fd, TIOCSFLAGSB, fb) == 0) {
		fb[0] &= ~(TANDEM | RTSCTS);
		fb[1] = TANDEM | RTSCTS;
		if (ioctl(fd, TIOCSFLAGSB, fb))
			perror("ttkbdd: TIOCSFLAGSB");
	}
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
	tcgetattr(fd, &tio);
	fprintf(stderr, "ttkbdd: serial on %s, 38400 raw (iflag %lx lflag %lx cflag %lx, ispeed %ld)\n", dev,
	        (long)tio.c_iflag, (long)tio.c_lflag, (long)tio.c_cflag, (long)cfgetispeed(&tio));
	if (verbose > 1) {	/* -vv: plain blocking read, no select — isolates select() on this tty */
		for (;;) {
			int k = read(fd, b, 1);
			fprintf(stderr, "raw read %d %02x\n", k, k > 0 ? b[0] : 0);
			if (k <= 0)
				return 1;
		}
	}
	for (;;) {
		r = recv_exact(fd, b, 1, SILENCE_S);
		if (r == -2) {
			if (in_session) {
				session_end("serial silence");
				in_session = 0;
			}
			continue;
		}
		if (r)
			return 1;
		if (verbose)
			fprintf(stderr, "rx %02x\n", b[0]);
		if (b[0] != SYNC)
			continue;	/* resync: skip until the next frame start */
		if (recv_exact(fd, b + 1, 4, SILENCE_S))
			continue;
		if (b[1] & F_QUIT) {
			if (in_session)
				session_end("serial quit");
			fprintf(stderr, "ttkbdd: quit — console returns\n");
			return 0;
		}
		if (!in_session && !(b[1] & F_HEARTBEAT)) {
			if (session_begin())
				return 1;
			in_session = 1;
			fprintf(stderr, "ttkbdd: serial session up\n");
		}
		frame(b + 1, fd);
	}
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
	/* ponytail: the pre-session rate died with the process; put back the value this TT boots with
	 * (read by --bench, 2026-09-23). Persist it if the boot value ever changes. */
	Kbrate(KBRATE_BOOT >> 8, KBRATE_BOOT & 0xff);
	return load_table(tbl_patched) ? 1 : 0;
}

int main(int argc, char **argv)
{
	const char *file = NULL, *addr = "192.168.0.30", *pubfile = "/etc/ttkbd.pub", *serdev = NULL;
	int test = 0, rel = 0, bench_n = 0, i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--release"))
			rel = 1;
		else if (!strcmp(argv[i], "-t"))
			test = 1;
		else if (!strcmp(argv[i], "-v"))
			verbose++;
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
		else if (i + 1 < argc && !strcmp(argv[i], "-s"))
			serdev = argv[++i];
		else if (i + 1 < argc && !strcmp(argv[i], "-c"))
			corner = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-S"))
			serdev = "-";
		else if (i + 1 < argc && !strcmp(argv[i], "-d"))
			pace_ms = atol(argv[++i]);
		else if (i + 1 < argc && !strcmp(argv[i], "--bench"))
			bench_n = atoi(argv[++i]);
		else {
			fprintf(stderr, "usage: ttkbdd [-l ADDR] [-k PUBFILE] | -s DEV | -f FILE [-t] [-d MS] | --release | --bench N  [-u TABLE] [-p TABLE]\n");
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
	if (bench_n)
		return bench(bench_n);
	if (file)
		return session_file(file, test);
	if (serdev)
		return serial_loop(serdev);
	return listen_loop(addr, 7590, pubfile);
}
