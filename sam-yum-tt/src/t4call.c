/* t4call — see t4call.h. Extracted from tgzip.c (t425-compress 1.1), which was the first user. */
#include "t4call.h"
#include "atwboot.h"
#include "zlib.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef T4LOCK
#define T4LOCK "/tmp/t425.lock"
#endif

static int state;                                /* 0 not tried, 1 up, -1 off for this process */
static char booted[256];
static char whybuf[80];
static const char *lastwhy;

/* Which program a pid is running, from /kern/<pid>/fname (e.g. "U:/bin/tgzip"). A pid alone is not
 * enough: MiNT reuses them. Not a start time: measured 2026-09-24, /kern/<pid>/stat has none that is
 * stable (fields 21-22 are the same for every process, field 23 changes between reads). "" when the
 * process does not exist or the name cannot be read. */
static void prog_of(long pid, char *out, int size)
{
    char path[32];
    FILE *f;
    int n = 0;

    out[0] = 0;
    sprintf(path, "/kern/%ld/fname", pid);
    if ((f = fopen(path, "r"))) {
        if (fgets(out, size, f))
            n = (int)strcspn(out, " \r\n");
        out[n] = 0;
        fclose(f);
    }
}

static void unlock_at_exit(void)
{
    t4lock_release();
}

int t4lock_take(const char **why)
{
    char buf[300], held[256], now[256];
    long pid;
    int fd, n, tries;
    FILE *f;

    for (tries = 0; tries < 2; tries++) {
        fd = open(T4LOCK, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd >= 0) {
            prog_of(getpid(), now, sizeof now);
            n = sprintf(buf, "%ld %s\n", (long)getpid(), now[0] ? now : "-");
            write(fd, buf, n);
            close(fd);
            return 1;
        }
        if (errno != EEXIST)
            break;
        pid = 0;
        held[0] = 0;
        if ((f = fopen(T4LOCK, "r"))) {
            if (fscanf(f, "%ld %255s", &pid, held) < 1)
                pid = 0;
            fclose(f);
        }
        /* live = the pid exists and runs the program that took the lock (or either name is unknown) */
        prog_of(pid, now, sizeof now);
        if (pid > 0 && kill((int)pid, 0) == 0
            && (!held[0] || !strcmp(held, "-") || !now[0] || !strcmp(held, now))) {
            sprintf(whybuf, "busy (pid %ld)", pid);
            *why = whybuf;
            return 0;
        }
        unlink(T4LOCK);                          /* stale: owner gone, or the pid now runs another program */
    }
    *why = "lock unavailable";
    return -1;
}

void t4lock_release(void)
{
    char buf[64];
    long pid = 0;
    FILE *f = fopen(T4LOCK, "r");

    if (f) {
        if (fgets(buf, sizeof buf, f))
            pid = atol(buf);
        fclose(f);
    }
    if (pid == (long)getpid())                   /* never remove someone else's lock */
        unlink(T4LOCK);
}

static long off(const char *why, const char **out)
{
    *out = lastwhy = why;
    state = -1;
    return -1;
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
        state = 1;
    }
    if (send_op((char)op, level, n) || link_write(in, n) != n || link_read(h, 4, 120000) != 4)
        return off("did not answer", why);
    len = (long)get4(h);
    if (len < 0 || len > cap)                    /* 0xffffffff reads as > cap on a 32-bit long */
        return off("failed the call", why);
    if (link_read(a, 4, 5000) != 4 || link_read(out, len, 30000) != len)
        return off("sent a short result", why);
    if (adler32(adler32(0L, Z_NULL, 0), out, (uInt)len) != get4(a))
        return off("sent a damaged result", why);
    return len;
}
