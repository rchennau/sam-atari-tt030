/* Host check: sha256test FILE... prints "<hex>  FILE" like sha256sum. Compare with sha256sum. */
#include <stdio.h>
#include "sha256.h"

int main(int argc, char **argv)
{
    unsigned char buf[4096], out[32];
    struct sha256 s;
    size_t n;
    int i, j;

    for (i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (!f)
            return 1;
        sha256_init(&s);
        while ((n = fread(buf, 1, sizeof buf, f)) > 0)
            sha256_update(&s, buf, n);
        fclose(f);
        sha256_final(&s, out);
        for (j = 0; j < 32; j++)
            printf("%02x", out[j]);
        printf("  %s\n", argv[i]);
    }
    return 0;
}
