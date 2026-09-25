/* gift4test — giftopnm through pmshim under t4 vs on the host (tt030-t425-kernel-ports W2): each GIF named
 * on the command line converted twice in one process (the server runs it per request, so its statics must
 * reset); prints length and checksum of each run. No arguments: c.gif g.gif b.gif i.gif (color, gray,
 * two-color, interlaced) in the current directory. */
#include <stdio.h>
#include <stdlib.h>
#include "gifrun.h"

int main(int argc, char **argv)
{
    static char *dflt[] = { "gift4test", "c.gif", "g.gif", "b.gif", "i.gif" };
    int a, k;
    if (argc < 2) {
        argc = 5;
        argv = dflt;
    }
    for (a = 1; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb");
        unsigned char *b = malloc(1L << 20), *o;
        long n, cap, r, i;
        unsigned long s;
        if (!f || !b)
            return 1;
        n = (long)fread(b, 1, 1L << 20, f);
        fclose(f);
        if ((cap = gf_cap(b, n)) < 0 || !(o = malloc(cap)))
            return 1;
        for (k = 0; k < 2; k++) {
            r = gf_run(1, b, n, o, cap);
            for (s = 0, i = 0; i < r; i++)
                s = s * 31 + o[i];
            printf("%s run%d -> %ld %08lx %.2s\n", argv[a], k + 1, r, s & 0xffffffffUL, r > 2 ? (char *)o : "--");
        }
        free(o);
        free(b);
    }
    return 0;
}
