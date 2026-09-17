/* sha256 — minimal FIPS 180-4 SHA-256, C89, 32-bit `unsigned long` (T425 and 68030 alike).
 * Shared by shaserv (T425, INMOS icc) and shabench (68030, m68k gcc). Checked on fractal against
 * sha256sum by sha256test.c.
 */
#include "sha256.h"

static const unsigned long K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL, 0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL, 0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL};

#define M32 0xffffffffUL
#define ROR(x, n) ((((x) >> (n)) | ((x) << (32 - (n)))) & M32)

static void block(unsigned long *h, const unsigned char *p)
{
    unsigned long w[64], a, b, c, d, e, f, g, hh, t1, t2;
    int i;

    for (i = 0; i < 16; i++)
        w[i] = ((unsigned long)p[4 * i] << 24) | ((unsigned long)p[4 * i + 1] << 16) |
               ((unsigned long)p[4 * i + 2] << 8) | p[4 * i + 3];
    for (i = 16; i < 64; i++) {
        t1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        t2 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        w[i] = (t1 + w[i - 7] + t2 + w[i - 16]) & M32;
    }
    a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
    for (i = 0; i < 64; i++) {
        t1 = (hh + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i]) & M32;
        t2 = ((ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c))) & M32;
        hh = g; g = f; f = e; e = (d + t1) & M32;
        d = c; c = b; b = a; a = (t1 + t2) & M32;
    }
    h[0] = (h[0] + a) & M32; h[1] = (h[1] + b) & M32; h[2] = (h[2] + c) & M32; h[3] = (h[3] + d) & M32;
    h[4] = (h[4] + e) & M32; h[5] = (h[5] + f) & M32; h[6] = (h[6] + g) & M32; h[7] = (h[7] + hh) & M32;
}

void sha256_init(struct sha256 *s)
{
    static const unsigned long iv[8] = {0x6a09e667UL, 0xbb67ae85UL, 0x3c6ef372UL, 0xa54ff53aUL,
                                        0x510e527fUL, 0x9b05688cUL, 0x1f83d9abUL, 0x5be0cd19UL};
    int i;
    for (i = 0; i < 8; i++)
        s->h[i] = iv[i];
    s->len = 0;
    s->fill = 0;
}

void sha256_update(struct sha256 *s, const unsigned char *p, unsigned long n)
{
    s->len += n;   /* ponytail: 32-bit byte count, fine up to 512 MB; widen if hashing more */
    while (n--) {
        s->buf[s->fill++] = *p++;
        if (s->fill == 64) {
            block(s->h, s->buf);
            s->fill = 0;
        }
    }
}

void sha256_final(struct sha256 *s, unsigned char *out)
{
    unsigned long bits = s->len << 3, hi = s->len >> 29;
    int i;

    s->buf[s->fill++] = 0x80;
    if (s->fill > 56) {
        while (s->fill < 64)
            s->buf[s->fill++] = 0;
        block(s->h, s->buf);
        s->fill = 0;
    }
    while (s->fill < 56)
        s->buf[s->fill++] = 0;
    for (i = 0; i < 4; i++) {
        s->buf[56 + i] = (unsigned char)(hi >> (24 - 8 * i));
        s->buf[60 + i] = (unsigned char)(bits >> (24 - 8 * i));
    }
    block(s->h, s->buf);
    for (i = 0; i < 32; i++)
        out[i] = (unsigned char)(s->h[i / 4] >> (24 - 8 * (i % 4)));
}
