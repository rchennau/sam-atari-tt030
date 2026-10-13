/* chacha20 — see chacha20.h. Words are kept in unsigned long and masked, so the code is correct whether long is 32
 * (icc, m68k) or 64 bits (x86-64 host build used as the test reference). */
#include "chacha20.h"
#define M32 0xFFFFFFFFUL
#define ROTL(v, c) ((((v) << (c)) | ((v) >> (32 - (c)))) & M32)
#define QR(a, b, c, d) \
    a = (a + b) & M32; d = ROTL(d ^ a, 16); c = (c + d) & M32; b = ROTL(b ^ c, 12); \
    a = (a + b) & M32; d = ROTL(d ^ a, 8);  c = (c + d) & M32; b = ROTL(b ^ c, 7)
static unsigned long le32(const unsigned char *p)
{ return p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24); }
void chacha20_xor(unsigned char *out, const unsigned char *in, long n,
                  const unsigned char key[32], const unsigned char nonce[12], unsigned long counter)
{
    unsigned long s[16], x[16];
    unsigned char ks[64];
    long off, i;
    int k;
    s[0] = 0x61707865UL; s[1] = 0x3320646eUL; s[2] = 0x79622d32UL; s[3] = 0x6b206574UL;
    for (k = 0; k < 8; k++) s[4 + k] = le32(key + 4 * k);
    s[13] = le32(nonce); s[14] = le32(nonce + 4); s[15] = le32(nonce + 8);
    for (off = 0; off < n; off += 64) {
        s[12] = counter & M32;
        for (k = 0; k < 16; k++) x[k] = s[k];
        for (k = 0; k < 10; k++) {
            QR(x[0], x[4], x[8], x[12]); QR(x[1], x[5], x[9], x[13]); QR(x[2], x[6], x[10], x[14]); QR(x[3], x[7], x[11], x[15]);
            QR(x[0], x[5], x[10], x[15]); QR(x[1], x[6], x[11], x[12]); QR(x[2], x[7], x[8], x[13]); QR(x[3], x[4], x[9], x[14]);
        }
        for (k = 0; k < 16; k++) {
            unsigned long v = (x[k] + s[k]) & M32;
            ks[4 * k] = (unsigned char)v; ks[4 * k + 1] = (unsigned char)(v >> 8);
            ks[4 * k + 2] = (unsigned char)(v >> 16); ks[4 * k + 3] = (unsigned char)(v >> 24);
        }
        for (i = 0; i < 64 && off + i < n; i++) out[off + i] = in[off + i] ^ ks[i];
        counter++;
    }
}
