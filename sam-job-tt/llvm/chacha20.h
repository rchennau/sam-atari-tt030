/* chacha20 — RFC 8439 ChaCha20 (IETF: 96-bit nonce, 32-bit block counter), 32-bit integers only. */
#ifndef CHACHA20_H
#define CHACHA20_H
/* out[i] = in[i] ^ keystream(key, nonce, counter)[i]; out may equal in. */
void chacha20_xor(unsigned char *out, const unsigned char *in, long n,
                  const unsigned char key[32], const unsigned char nonce[12], unsigned long counter);
#endif
