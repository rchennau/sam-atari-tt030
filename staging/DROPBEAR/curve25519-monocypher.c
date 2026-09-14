/* Replacement for Dropbear's src/curve25519.c: the same four functions, backed by Monocypher 4.0.3.
 *
 * Why: Dropbear's TweetNaCl-style field math (typedef i64 gf[16]) needs 64-bit multiplies, which the
 * TT's 68030 does in software (__muldi3). On the real TT a login took 224 s: 117 s kex + 106 s
 * ed25519 verify (2026-09-12). Monocypher uses 10 x 32-bit limbs (typedef i32 fe[10]), which map to
 * the 68030's 32x32->64 mulu.l. Same algorithms (X25519; Ed25519 with SHA-512, RFC 8032), so the
 * wire format and key files are unchanged.
 *
 * Monocypher is compiled in by #include so Dropbear's Makefile needs no edit.
 */
#include "includes.h"
#include "dbrandom.h"
#include "curve25519.h"
#include <time.h>

#include "monocypher.c"
/* One translation unit for both files: they share the static helper hash_reduce and some macros. */
#undef FOR
#undef COPY
#undef ZERO
#undef WIPE_CTX
#undef WIPE_BUFFER
#define hash_reduce ed25519_hash_reduce
#include "monocypher-ed25519.c"
#undef hash_reduce

/* X25519 on the ATW800/2's T425 when it is there (2.25 s vs 4.1 s on the 68030), else Monocypher. */
#include "atwx25519.c"

void dropbear_curve25519_scalarmult(unsigned char *q, const unsigned char *n, const unsigned char *p)
{
	/* Per-call timing in the log: login wall time over the TT's WiFi is too noisy to compare paths
	 * (40% ping loss while measuring, 2026-09-13). The first T425 call includes the boot. */
	clock_t t0 = clock();
	const char *path = "T425";

	if (atw_x25519(q, n, p) != 0) {
		crypto_x25519(q, n, p);
		path = "68030";
	}
	dropbear_log(LOG_INFO, "atw: x25519 on the %s took %ld ms", path,
		     (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC));
}

/* sk is Dropbear's 32-byte seed; Monocypher's secret key is seed || public key (64 bytes). */
void dropbear_ed25519_make_key(unsigned char *pk, unsigned char *sk)
{
	uint8_t seed[32], sk64[64];

	genrandom(sk, 32);
	memcpy(seed, sk, 32);  /* crypto_ed25519_key_pair wipes its seed argument */
	crypto_ed25519_key_pair(sk64, pk, seed);
	crypto_wipe(sk64, sizeof sk64);
}

void dropbear_ed25519_sign(const unsigned char *m, unsigned long mlen,
			   unsigned char *s, unsigned long *slen,
			   const unsigned char *sk, const unsigned char *pk)
{
	uint8_t sk64[64];

	memcpy(sk64, sk, 32);
	memcpy(sk64 + 32, pk, 32);
	crypto_ed25519_sign(s, sk64, m, mlen);
	crypto_wipe(sk64, sizeof sk64);
	*slen = 64;
}

int dropbear_ed25519_verify(const unsigned char *m, unsigned long mlen,
			    const unsigned char *s, unsigned long slen,
			    const unsigned char *pk)
{
	if (slen != 64)
		return -1;
	return crypto_ed25519_check(s, pk, m, mlen);  /* 0 = valid, -1 = invalid, like Dropbear's */
}
