/* difftest — build once against Daniel Beer's f25519.c and once against fe32/f25519.c, then diff the
 * outputs. Same fixed-seed random operands for both; every result is normalized before printing, so
 * different internal representations still print identically when they agree mod p.
 * Operands include near-2^256 values, which the fe32 invariant allows and the original may not: the
 * original is only fed reduced inputs.
 */
#include <stdio.h>
#include "f25519.h"

static unsigned long seed = 12345;
static unsigned int rnd8(void)
{
    seed = seed * 1103515245UL + 12345UL;
    return (unsigned int)(seed >> 16) & 0xff;
}

static void rand_fe(uint8_t *x, int reduced)
{
    int i;
    for (i = 0; i < F25519_SIZE; i++)
        x[i] = (uint8_t)rnd8();
    if (reduced)
        f25519_normalize(x);
}

static void put(const char *op, int n, uint8_t *x)
{
    int i;
    f25519_normalize(x);
    printf("%s %d ", op, n);
    for (i = F25519_SIZE - 1; i >= 0; i--)
        printf("%02x", x[i]);
    printf("\n");
}

int main(void)
{
    uint8_t a[F25519_SIZE], b[F25519_SIZE], r[F25519_SIZE];
    int n;

    for (n = 0; n < 400; n++) {
        rand_fe(a, 1);
        rand_fe(b, 1);
        f25519_add(r, a, b);          put("add", n, r);
        f25519_sub(r, a, b);          put("sub", n, r);
        f25519_sub(r, b, a);          put("rsub", n, r);
        f25519_mul__distinct(r, a, b); put("mul", n, r);
        f25519_mul__distinct(r, a, a); put("sqr", n, r);
        f25519_mul_c(r, a, 486662);   put("mulc", n, r);
        f25519_mul_c(r, a, 4);        put("mul4", n, r);
    }
    for (n = 0; n < 20; n++) {
        rand_fe(a, 1);
        f25519_inv__distinct(r, a);   put("inv", n, r);
    }
    /* edge values: 0, 1, p-1, p, p+1 (unnormalized forms) */
    {
        static const uint8_t pm1[32] = { 0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f };
        int i;
        for (i = 0; i < 32; i++) a[i] = pm1[i];
        f25519_mul__distinct(r, a, a); put("pm1sq", 0, r);
        f25519_add(r, a, f25519_one);  put("pm1p1", 0, r);
        f25519_sub(r, f25519_zero, f25519_one); put("zm1", 0, r);
        f25519_mul_c(r, a, 486662);    put("pm1c", 0, r);
    }
    return 0;
}
