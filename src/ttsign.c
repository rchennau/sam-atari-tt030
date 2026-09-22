/* ttsign — Ed25519-sign stdin with the TT's request key (tt030-rpm-pipeline FR-4 / NFR-4).
 *
 *   ttsign KEYFILE < message        prints the 64-byte RFC 8032 signature as 128 hex digits
 *
 * KEYFILE holds the 32-byte seed, mode 600, on the card only; fractal keeps just the public key.
 * Why not openssl: RSA-2048 signing took 2 min 29 s on the 68030 (RSA-1024 28.5 s, 2026-09-21);
 * Monocypher's Ed25519 — the copy Dropbear already uses on this machine — signs in ~2.7 s.
 * Exit 0 signed · 1 key/input error · 2 usage.
 * Build: m68k-atari-mint-gcc -m68020-60 -O2 -s -o ttsign src/ttsign.c \
 *        staging/DROPBEAR/monocypher.c staging/DROPBEAR/monocypher-ed25519.c -Istaging/DROPBEAR
 */
#include <stdio.h>
#include <string.h>
#include "monocypher.h"
#include "monocypher-ed25519.h"

#define MAXMSG 4096

int main(int argc, char **argv)
{
    static uint8_t msg[MAXMSG];
    uint8_t seed[32], sk[64], pk[32], sig[64];
    size_t n;
    int i;
    FILE *f;

    if (argc != 2) {
        fprintf(stderr, "usage: ttsign KEYFILE < message\n");
        return 2;
    }
    if (!(f = fopen(argv[1], "rb")) || fread(seed, 1, 32, f) != 32) {
        fprintf(stderr, "ttsign: cannot read a 32-byte seed from %s\n", argv[1]);
        return 1;
    }
    fclose(f);
    n = fread(msg, 1, MAXMSG, stdin);
    if (!feof(stdin) || ferror(stdin)) {
        fprintf(stderr, "ttsign: message missing or over %d bytes\n", MAXMSG);
        return 1;
    }
    crypto_ed25519_key_pair(sk, pk, seed);          /* also wipes seed */
    crypto_ed25519_sign(sig, sk, msg, n);
    crypto_wipe(sk, sizeof sk);
    for (i = 0; i < 64; i++)
        printf("%02x", sig[i]);
    putchar('\n');
    return 0;
}
