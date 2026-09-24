/* t4call — see t4call.h. Extracted from tgzip.c (t425-compress 1.1), which was the first user. */
#include "t4call.h"
#include "t4lock.h"
#include "atwboot.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static int state;                                /* 0 not tried, 1 up, -1 off for this process */
static char booted[256];
static const char *lastwhy;

static void unlock_at_exit(void)
{
    t4lock_release();
}

static long off(const char *why, const char **out)
{
    *out = lastwhy = why;
    state = -1;
    return -1;
}

/* adler32 (RFC 1950), so a package links t4call without zlib; must match t4serv.c's zlib adler32. */
unsigned long t4_adler32(const unsigned char *p, long n)
{
    unsigned long a = 1, b = 0;
    long k;

    while (n > 0) {
        k = n < 5552 ? n : 5552;                 /* largest block before b can overflow 32 bits */
        n -= k;
        while (k--) {
            a += *p++;
            b += a;
        }
        a %= 65521UL;
        b %= 65521UL;
    }
    return (b << 16) | a;
}

int t4_enabled(const char *pkg)
{
    char path[160];

    sprintf(path, "/etc/t425/enabled/%.100s", pkg);
    return access(path, F_OK) == 0;
}

static unsigned long get4(const unsigned char *b)
{
    return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
}

long t4call(const char *btl, int op, int level, const void *in, long n, void *out, long cap,
            const char **why)
{
    unsigned char h[4], a[4];
    long len;
    int l;

    if (state < 0)
        return off(lastwhy ? lastwhy : "off", why);
    if (state == 0 || strcmp(booted, btl)) {
        if (state == 0) {
            l = t4lock_take(why);
            if (l <= 0)
                return off(*why, why);
            atexit(unlock_at_exit);
        }
        atw_verbose = 0;
        if (atw_boot(btl, "#400000"))
            return off("did not boot (fpgabios resident? server present?)", why);
        strncpy(booted, btl, sizeof booted - 1);
        t4_loaded_set(strrchr(btl, '/') ? strrchr(btl, '/') + 1 : btl);
        state = 1;
    }
    if (send_op((char)op, level, n) || link_write(in, n) != n || link_read(h, 4, 120000) != 4)
        return off("did not answer", why);
    len = (long)get4(h);
    if (len < 0 || len > cap)                    /* 0xffffffff reads as > cap on a 32-bit long */
        return off("failed the call", why);
    if (link_read(a, 4, 5000) != 4 || link_read(out, len, 30000) != len)
        return off("sent a short result", why);
    if (t4_adler32(out, len) != get4(a))
        return off("sent a damaged result", why);
    return len;
}
