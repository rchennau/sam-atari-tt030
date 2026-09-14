/* sha512_32 — SHA-512 with a 32-bit-pair emulation of uint64, for the ATW800/2 T425 (INMOS icc has no
 * 64-bit type). Drop-in for Daniel Beer's sha512.c: same struct, same sha512_block/final/get/init, so
 * ed25519/edsign link against it unchanged. Kanban 3c29ce8a (Ed25519 offload).
 *
 * SHA-512 uses only 64-bit add, xor, right-shift and rotate — no multiply — so each uint64 is a
 * {hi, lo} pair of uint32 and every op is a few 32-bit instructions. On a GCC host the same file
 * compiles with native uint64_t (guarded by __GNUC__) so it can be checked against the reference.
 */
#include "sha512.h"

const struct sha512_state sha512_initial_state = { {
    0x6a09e667, 0xf3bcc908, 0xbb67ae85, 0x84caa73b,
    0x3c6ef372, 0xfe94f82b, 0xa54ff53a, 0x5f1d36f1,
    0x510e527f, 0xade682d1, 0x9b05688c, 0x2b3e6c1f,
    0x1f83d9ab, 0xfb41bd6b, 0x5be0cd19, 0x137e2179,
} };

#if defined(__GNUC__) && !defined(FORCE_EMU64)
/* Native path (fractal): lets sha512_32.c be diff-tested against the reference sha512.c. */
typedef uint64_t u64;
#define U64(hi, lo) (((uint64_t)(hi) << 32) | (lo))
#define ADD(a, b) ((a) + (b))
#define XOR(a, b) ((a) ^ (b))
#define AND(a, b) ((a) & (b))
#define NOTAND(a, b) ((~(a)) & (b))
#define SHR(x, n) ((x) >> (n))
#define ROT(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define HI(x) ((uint32_t)((x) >> 32))
#define LO(x) ((uint32_t)(x))
#define SET(x, hi, lo) ((x) = U64(hi, lo))
#else
/* T425 path: uint64 as {hi, lo}. */
typedef struct { uint32_t hi, lo; } u64;
#define HI(x) ((x).hi)
#define LO(x) ((x).lo)

static u64 mk(uint32_t hi, uint32_t lo) { u64 r; r.hi = hi; r.lo = lo; return r; }
#define SET(x, hi, lo) ((x) = mk((hi), (lo)))

static u64 ADD(u64 a, u64 b)
{
    uint32_t lo = a.lo + b.lo;
    return mk(a.hi + b.hi + (lo < a.lo), lo);
}
static u64 XOR(u64 a, u64 b) { return mk(a.hi ^ b.hi, a.lo ^ b.lo); }
static u64 AND(u64 a, u64 b) { return mk(a.hi & b.hi, a.lo & b.lo); }
static u64 NOTAND(u64 a, u64 b) { return mk(~a.hi & b.hi, ~a.lo & b.lo); }

static u64 SHR(u64 x, int n)                       /* 1..63 */
{
    if (n < 32)
        return mk(x.hi >> n, (x.lo >> n) | (x.hi << (32 - n)));
    return mk(0, x.hi >> (n - 32));
}
static u64 ROTR(u64 x, int n)                      /* rotate right, 1..63; bit i -> (i-n) mod 64 */
{
    if (n == 32)
        return mk(x.lo, x.hi);
    if (n < 32)
        return mk((x.hi >> n) | (x.lo << (32 - n)), (x.lo >> n) | (x.hi << (32 - n)));
    n -= 32;                                        /* 1..31 */
    return mk((x.lo >> n) | (x.hi << (32 - n)), (x.hi >> n) | (x.lo << (32 - n)));
}
#define ROT(x, n) ROTR(x, n)
#endif

static const u64 round_k_init[80] = { 0 };         /* filled by first-call init (icc has no LL consts) */

#if defined(__GNUC__) && !defined(FORCE_EMU64)
static const uint64_t RK[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};
#define K(i) RK[i]
#else
/* icc has no 64-bit literals: store the 80 constants as {hi, lo} uint32 pairs. */
static const uint32_t RK[80][2] = {
    {0x428a2f98,0xd728ae22},{0x71374491,0x23ef65cd},{0xb5c0fbcf,0xec4d3b2f},{0xe9b5dba5,0x8189dbbc},
    {0x3956c25b,0xf348b538},{0x59f111f1,0xb605d019},{0x923f82a4,0xaf194f9b},{0xab1c5ed5,0xda6d8118},
    {0xd807aa98,0xa3030242},{0x12835b01,0x45706fbe},{0x243185be,0x4ee4b28c},{0x550c7dc3,0xd5ffb4e2},
    {0x72be5d74,0xf27b896f},{0x80deb1fe,0x3b1696b1},{0x9bdc06a7,0x25c71235},{0xc19bf174,0xcf692694},
    {0xe49b69c1,0x9ef14ad2},{0xefbe4786,0x384f25e3},{0x0fc19dc6,0x8b8cd5b5},{0x240ca1cc,0x77ac9c65},
    {0x2de92c6f,0x592b0275},{0x4a7484aa,0x6ea6e483},{0x5cb0a9dc,0xbd41fbd4},{0x76f988da,0x831153b5},
    {0x983e5152,0xee66dfab},{0xa831c66d,0x2db43210},{0xb00327c8,0x98fb213f},{0xbf597fc7,0xbeef0ee4},
    {0xc6e00bf3,0x3da88fc2},{0xd5a79147,0x930aa725},{0x06ca6351,0xe003826f},{0x14292967,0x0a0e6e70},
    {0x27b70a85,0x46d22ffc},{0x2e1b2138,0x5c26c926},{0x4d2c6dfc,0x5ac42aed},{0x53380d13,0x9d95b3df},
    {0x650a7354,0x8baf63de},{0x766a0abb,0x3c77b2a8},{0x81c2c92e,0x47edaee6},{0x92722c85,0x1482353b},
    {0xa2bfe8a1,0x4cf10364},{0xa81a664b,0xbc423001},{0xc24b8b70,0xd0f89791},{0xc76c51a3,0x0654be30},
    {0xd192e819,0xd6ef5218},{0xd6990624,0x5565a910},{0xf40e3585,0x5771202a},{0x106aa070,0x32bbd1b8},
    {0x19a4c116,0xb8d2d0c8},{0x1e376c08,0x5141ab53},{0x2748774c,0xdf8eeb99},{0x34b0bcb5,0xe19b48a8},
    {0x391c0cb3,0xc5c95a63},{0x4ed8aa4a,0xe3418acb},{0x5b9cca4f,0x7763e373},{0x682e6ff3,0xd6b2b8a3},
    {0x748f82ee,0x5defb2fc},{0x78a5636f,0x43172f60},{0x84c87814,0xa1f0ab72},{0x8cc70208,0x1a6439ec},
    {0x90befffa,0x23631e28},{0xa4506ceb,0xde82bde9},{0xbef9a3f7,0xb2c67915},{0xc67178f2,0xe372532b},
    {0xca273ece,0xea26619c},{0xd186b8c7,0x21c0c207},{0xeada7dd6,0xcde0eb1e},{0xf57d4f7f,0xee6ed178},
    {0x06f067aa,0x72176fba},{0x0a637dc5,0xa2c898a6},{0x113f9804,0xbef90dae},{0x1b710b35,0x131c471b},
    {0x28db77f5,0x23047d84},{0x32caab7b,0x40c72493},{0x3c9ebe0a,0x15c9bebc},{0x431d67c4,0x9c100d4c},
    {0x4cc5d4be,0xcb3e42b6},{0x597f299c,0xfc657e2a},{0x5fcb6fab,0x3ad6faec},{0x6c44198c,0x4a475817},
};
static u64 K(int i) { return mk(RK[i][0], RK[i][1]); }
#endif

static u64 load64(const uint8_t *x)
{
    uint32_t hi = ((uint32_t)x[0] << 24) | ((uint32_t)x[1] << 16) | ((uint32_t)x[2] << 8) | x[3];
    uint32_t lo = ((uint32_t)x[4] << 24) | ((uint32_t)x[5] << 16) | ((uint32_t)x[6] << 8) | x[7];
#if defined(__GNUC__) && !defined(FORCE_EMU64)
    return U64(hi, lo);
#else
    return mk(hi, lo);
#endif
}
static void store64(uint8_t *x, u64 v)
{
    uint32_t hi = HI(v), lo = LO(v);
    x[0] = hi >> 24; x[1] = hi >> 16; x[2] = hi >> 8; x[3] = hi;
    x[4] = lo >> 24; x[5] = lo >> 16; x[6] = lo >> 8; x[7] = lo;
}

void sha512_block(struct sha512_state *s, const uint8_t *blk)
{
    u64 w[16], a, b, c, d, e, f, g, h;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = load64(blk);
        blk += 8;
    }
#define LDH(v, i) SET(v, s->h[2*(i)], s->h[2*(i)+1])
    LDH(a, 0); LDH(b, 1); LDH(c, 2); LDH(d, 3);
    LDH(e, 4); LDH(f, 5); LDH(g, 6); LDH(h, 7);
#undef LDH
    for (i = 0; i < 80; i++) {
        u64 wi = w[i & 15], wi15 = w[(i + 1) & 15], wi2 = w[(i + 14) & 15], wi7 = w[(i + 9) & 15];
        u64 s0 = XOR(XOR(ROT(wi15, 1), ROT(wi15, 8)), SHR(wi15, 7));
        u64 s1 = XOR(XOR(ROT(wi2, 19), ROT(wi2, 61)), SHR(wi2, 6));
        u64 S0 = XOR(XOR(ROT(a, 28), ROT(a, 34)), ROT(a, 39));
        u64 S1 = XOR(XOR(ROT(e, 14), ROT(e, 18)), ROT(e, 41));
        u64 ch = XOR(AND(e, f), NOTAND(e, g));
        u64 temp1 = ADD(ADD(ADD(ADD(h, S1), ch), K(i)), wi);
        u64 maj = XOR(XOR(AND(a, b), AND(a, c)), AND(b, c));
        u64 temp2 = ADD(S0, maj);
        h = g; g = f; f = e; e = ADD(d, temp1); d = c; c = b; b = a; a = ADD(temp1, temp2);
        w[i & 15] = ADD(ADD(ADD(wi, s0), wi7), s1);
    }
#define STH(v, i) { u64 t; SET(t, s->h[2*(i)], s->h[2*(i)+1]); t = ADD(t, v); s->h[2*(i)] = HI(t); s->h[2*(i)+1] = LO(t); }
    STH(a, 0); STH(b, 1); STH(c, 2); STH(d, 3);
    STH(e, 4); STH(f, 5); STH(g, 6); STH(h, 7);
#undef STH
    (void)round_k_init;
}

void sha512_final(struct sha512_state *s, const uint8_t *blk, size_t total_size)
{
    uint8_t temp[SHA512_BLOCK_SIZE];
    size_t last_size = total_size & (SHA512_BLOCK_SIZE - 1), i;
    uint32_t bit_hi = (uint32_t)(total_size >> 29), bit_lo = (uint32_t)(total_size << 3);

    for (i = 0; i < SHA512_BLOCK_SIZE; i++)
        temp[i] = 0;
    if (last_size)
        memcpy(temp, blk, last_size);
    temp[last_size] = 0x80;
    if (last_size > 111) {
        sha512_block(s, temp);
        for (i = 0; i < SHA512_BLOCK_SIZE; i++)
            temp[i] = 0;
    }
    temp[120] = bit_hi >> 24; temp[121] = bit_hi >> 16; temp[122] = bit_hi >> 8; temp[123] = bit_hi;
    temp[124] = bit_lo >> 24; temp[125] = bit_lo >> 16; temp[126] = bit_lo >> 8; temp[127] = bit_lo;
    sha512_block(s, temp);
}

void sha512_get(const struct sha512_state *s, uint8_t *hash, unsigned int offset, unsigned int len)
{
    uint8_t out[SHA512_HASH_SIZE];
    unsigned int i;
    for (i = 0; i < 8; i++) {
        u64 wv; SET(wv, s->h[2*i], s->h[2*i+1]); store64(out + i * 8, wv);
    }
    for (i = 0; i < len && offset + i < SHA512_HASH_SIZE; i++)
        hash[i] = out[offset + i];
}
