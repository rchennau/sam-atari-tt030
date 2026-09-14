/* SHA512 — port header for the ATW800/2 T425 (INMOS icc has no uint64_t). Same API as Daniel Beer's
 * sha512.h, but the state is 8 words held as 16 x uint32 (hi, lo pairs), so it compiles everywhere.
 * ed25519/edsign only pass the state around; they never touch h[] directly. Kanban 3c29ce8a.
 */
#ifndef SHA512_H_
#define SHA512_H_
#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct sha512_state {
    uint32_t h[16];                    /* h[2i] = hi, h[2i+1] = lo of 64-bit word i */
};

extern const struct sha512_state sha512_initial_state;

static void sha512_init(struct sha512_state *s)
{
    memcpy(s, &sha512_initial_state, sizeof(*s));
}

#define SHA512_BLOCK_SIZE  128
#define SHA512_HASH_SIZE   64

void sha512_block(struct sha512_state *s, const uint8_t *blk);
void sha512_final(struct sha512_state *s, const uint8_t *blk, size_t total_size);
void sha512_get(const struct sha512_state *s, uint8_t *hash, unsigned int offset, unsigned int len);

#endif
