/* f25519 replacement for the ATW800/2 T425: arithmetic mod p = 2^255-19 on 8 x 32-bit words, with the
 * transputer's `lmul` (32x32+32 -> 64) doing the multiplies. Same API as Daniel Beer's f25519.c (public
 * domain), so his c25519.c ladder links against it unchanged. Kanban a764de13 step 1b.
 *
 * Invariant (differs from the original): every function accepts any value < 2^256 and returns a value
 * < 2^256 that is congruent mod p. Only f25519_normalize fully reduces to [0, p). Overflow past 2^256
 * is folded with 2^256 = 38 (mod p).
 * ponytail: bytes are converted to words inside every call; keep words across the ladder if the
 * conversion shows up in the timing.
 * Constant-time (kanban 37a7a860, 2026-09-13): no branch or loop count depends on the data. Carry folds run
 * a fixed 3 passes, borrow folds a fixed 2, add_small always walks all 8 words, and normalize selects with a
 * mask. Bounds: after one fold the carry is 0 or 1; after two the value can only have wrapped if it was
 * below 38, so a third pass can't carry again. lmul's cycle count on the T425 is taken as data-independent
 * (INMOS instruction timing) — not measured here.
 */
#include "f25519.h"

const uint8_t f25519_zero[F25519_SIZE] = {0};
const uint8_t f25519_one[F25519_SIZE] = {1};

typedef uint32_t w8[8];

/* (x * y + c) as hi:lo. INMOS icc (the only non-GCC compiler here) uses the T425 instruction; GCC
 * builds use a 64-bit multiply so this file can be checked on fractal against the original f25519.c. */
#ifdef __GNUC__
static void mac(uint32_t x, uint32_t y, uint32_t c, uint32_t *lo, uint32_t *hi)
{
    unsigned long long t = (unsigned long long)x * y + c;
    *lo = (uint32_t)t;
    *hi = (uint32_t)(t >> 32);
}
#else
static void mac(uint32_t x, uint32_t y, uint32_t c, uint32_t *lo, uint32_t *hi)
{
    uint32_t l, h;
    __asm { ld c; ld y; ld x; lmul; st l; st h; }  /* verified on t4 against Python, 2026-09-13 */
    *lo = l;
    *hi = h;
}
#endif

static void load(w8 w, const uint8_t *b)
{
    int i;
    for (i = 0; i < 8; i++)
        w[i] = (uint32_t)b[4 * i] | ((uint32_t)b[4 * i + 1] << 8) |
               ((uint32_t)b[4 * i + 2] << 16) | ((uint32_t)b[4 * i + 3] << 24);
}

static void store(uint8_t *b, const w8 w)
{
    int i;
    for (i = 0; i < 8; i++) {
        b[4 * i] = (uint8_t)w[i];
        b[4 * i + 1] = (uint8_t)(w[i] >> 8);
        b[4 * i + 2] = (uint8_t)(w[i] >> 16);
        b[4 * i + 3] = (uint8_t)(w[i] >> 24);
    }
}

/* w += k (k small); returns the carry out of 2^256 (0 or 1). Always touches all 8 words. */
static uint32_t add_small(w8 w, uint32_t k)
{
    int i;
    for (i = 0; i < 8; i++) {
        uint32_t s = w[i] + k;
        k = s < w[i];
        w[i] = s;
    }
    return k;
}

/* w -= k (k small); returns the borrow out (0 or 1). Always touches all 8 words. */
static uint32_t sub_small(w8 w, uint32_t k)
{
    int i;
    for (i = 0; i < 8; i++) {
        uint32_t d = w[i] - k;
        k = w[i] < k;
        w[i] = d;
    }
    return k;
}

/* Fold a carry c (value c * 2^256) back in as c * 38: a fixed three passes (see header). */
static void fold(w8 w, uint32_t c)
{
    int pass;
    for (pass = 0; pass < 3; pass++) {
        uint32_t lo, hi;
        mac(c, 38, 0, &lo, &hi);
        c = hi + add_small(w, lo);
    }
}

void f25519_load(uint8_t *x, uint32_t c)
{
    unsigned int i;
    for (i = 0; i < sizeof(c); i++) {
        x[i] = c;
        c >>= 8;
    }
    for (; i < F25519_SIZE; i++)
        x[i] = 0;
}

void f25519_select(uint8_t *dst, const uint8_t *zero, const uint8_t *one, uint8_t condition)
{
    const uint8_t mask = -condition;
    int i;
    for (i = 0; i < F25519_SIZE; i++)
        dst[i] = zero[i] ^ (mask & (one[i] ^ zero[i]));
}

void f25519_add(uint8_t *r, const uint8_t *a, const uint8_t *b)
{
    w8 x, y;
    uint32_t c = 0;
    int i;
    load(x, a);
    load(y, b);
    for (i = 0; i < 8; i++) {
        uint32_t s = x[i] + c;
        c = s < c;
        x[i] = s + y[i];
        c += x[i] < s;
    }
    fold(x, c);
    store(r, x);
}

void f25519_sub(uint8_t *r, const uint8_t *a, const uint8_t *b)
{
    w8 x, y;
    uint32_t borrow = 0;
    int i;
    load(x, a);
    load(y, b);
    for (i = 0; i < 8; i++) {
        uint32_t d = x[i] - y[i];
        uint32_t b2 = x[i] < y[i];
        x[i] = d - borrow;
        borrow = b2 | (d < borrow);
    }
    /* A borrow means the result is x - 2^256, and 2^256 = 38 mod p: subtract 38 * borrow, twice. */
    borrow = sub_small(x, 38 & (0 - borrow));
    sub_small(x, 38 & (0 - borrow));
    store(r, x);
}

void f25519_mul__distinct(uint8_t *r, const uint8_t *a, const uint8_t *b)
{
    w8 x, y, res;
    uint32_t t[16], c, lo, hi;
    int i, j;

    load(x, a);
    load(y, b);
    for (i = 0; i < 16; i++)
        t[i] = 0;
    for (i = 0; i < 8; i++) {                  /* schoolbook 8x8 -> 16 words */
        c = 0;
        for (j = 0; j < 8; j++) {
            mac(x[i], y[j], c, &lo, &hi);
            lo += t[i + j];
            hi += lo < t[i + j];
            t[i + j] = lo;
            c = hi;
        }
        t[i + 8] = c;
    }
    c = 0;                                     /* res = t_low + 38 * t_high */
    for (i = 0; i < 8; i++) {
        mac(t[i + 8], 38, c, &lo, &hi);
        lo += t[i];
        hi += lo < t[i];
        res[i] = lo;
        c = hi;
    }
    fold(res, c);
    store(r, res);
}

void f25519_mul(uint8_t *r, const uint8_t *a, const uint8_t *b)
{
    uint8_t tmp[F25519_SIZE];
    f25519_mul__distinct(tmp, a, b);
    f25519_copy(r, tmp);
}

void f25519_mul_c(uint8_t *r, const uint8_t *a, uint32_t b)
{
    w8 x;
    uint32_t c = 0, lo, hi;
    int i;
    load(x, a);
    for (i = 0; i < 8; i++) {
        mac(x[i], b, c, &lo, &hi);
        x[i] = lo;
        c = hi;
    }
    fold(x, c);
    store(r, x);
}

void f25519_normalize(uint8_t *x)
{
    w8 w, t;
    uint32_t top;
    int i;
    load(w, x);
    top = w[7] >> 31;                          /* fold bit 255: 2^255 = 19 mod p */
    w[7] &= 0x7fffffffUL;
    add_small(w, 19 * top);                    /* now < 2^255 + 19 */
    for (i = 0; i < 8; i++)                    /* w >= p  <=>  w + 19 >= 2^255 */
        t[i] = w[i];
    add_small(t, 19);
    {
        uint32_t m = 0 - (t[7] >> 31);         /* all ones when w >= p: take t - 2^255 */
        t[7] &= 0x7fffffffUL;
        for (i = 0; i < 8; i++)
            w[i] = (t[i] & m) | (w[i] & ~m);
    }
    store(x, w);
}

void f25519_inv__distinct(uint8_t *r, const uint8_t *x)
{
    /* Fermat: x^(p-2), p-2 = 2^255-21 = 11111111...01011 in binary. Same chain as the original. */
    uint8_t s[F25519_SIZE];
    int i;

    f25519_mul__distinct(s, x, x);
    f25519_mul__distinct(r, s, x);
    for (i = 0; i < 248; i++) {
        f25519_mul__distinct(s, r, r);
        f25519_mul__distinct(r, s, x);
    }
    f25519_mul__distinct(s, r, r);
    f25519_mul__distinct(r, s, s);
    f25519_mul__distinct(s, r, x);
    f25519_mul__distinct(r, s, s);
    f25519_mul__distinct(s, r, r);
    f25519_mul__distinct(r, s, x);
    f25519_mul__distinct(s, r, r);
    f25519_mul__distinct(r, s, x);
}

void f25519_inv(uint8_t *r, const uint8_t *x)
{
    uint8_t tmp[F25519_SIZE];
    f25519_inv__distinct(tmp, x);
    f25519_copy(r, tmp);
}
