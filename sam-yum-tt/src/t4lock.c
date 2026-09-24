/* t4lock — who may use the ATW800/2's T425, and which server it holds (tt030-t425-kernel-ports FR-2/FR-6).
 * Shared by t4call (every kernel package) and Dropbear's X25519 offload, which #includes this file, so
 * it has no dependency beyond libc. Rules: plan Eng §2. */
#include "t4lock.h"
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
#ifndef T4LOADED
#define T4LOADED "/tmp/t425.loaded"
#endif

static char whybuf[80];

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

/* The T425 runs one server at a time and nothing on the board says which. Whoever boots one records it
 * here, so the next user boots its own server at once instead of probing a stranger's (a probe of
 * compserv by Dropbear waited out a 10 s timeout: first X25519 13,630 ms vs 2,715 ms, 2026-09-24). */
void t4_loaded_set(const char *server)
{
    FILE *f = fopen(T4LOADED, "w");
    if (f) {
        fprintf(f, "%s\n", server);
        fclose(f);
    }
}

int t4_loaded_is(const char *server)
{
    char buf[256];
    int yes = -1;                                    /* no marker: unknown (e.g. booted by mint.cnf) */
    FILE *f = fopen(T4LOADED, "r");
    if (f) {
        if (fgets(buf, sizeof buf, f)) {
            buf[strcspn(buf, "\r\n")] = 0;
            yes = strcmp(buf, server) == 0 ? 1 : 0;
        }
        fclose(f);
    }
    return yes;
}
