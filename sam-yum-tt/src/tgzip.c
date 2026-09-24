/* tgzip / tbzip2 — gzip and bzip2 compression on the ATW800/2 T425, 68030 fallback
 * (tt030-rpm-pipeline Rev. 7, operator 2026-09-24: the first package to use the transputer by default).
 *
 *   tgzip  [-1..-9] [-c] [-k] [-f] [-v] [FILE...]     FILE -> FILE.gz  (no FILE: stdin -> stdout)
 *   tbzip2 [-1..-9] [-c] [-k] [-f] [-v] [FILE...]     FILE -> FILE.bz2 (same binary, by argv[0])
 *
 * The input goes to the T425 in 256 KB pieces; each piece comes back as one gzip member / bzip2 stream,
 * and members concatenate, so gunzip / bunzip2 read the result like any other .gz / .bz2. Measured on
 * the real TT (compbench, 2026-09-24): the T425 takes 0.66-0.81 of the 68030's time incl. the link.
 *
 * A piece is compressed on the 68030 instead, with the same compkern code, when: fpgabios is not
 * resident, the T425 is busy (/tmp/t425.lock names a live pid), or a piece fails or arrives damaged
 * (compserv sends its adler32 of each piece; see compserv.c for the one damage ever seen, and its fix).
 * After a failure the rest of the run stays on the 68030: the link may be out of step. Each fallback
 * is said on stderr. Booting compserv resets the T425, so Dropbear's X25519 offload restarts at the
 * next SSH login.
 *
 * bzip2: a piece is 256 KB, so a block size above 3 (300 KB) compresses identically; levels 4-9 are
 * clamped to 3, which keeps the T425's memory under IBOARDSIZE #400000.
 * ponytail: the input's mode and mtime are not copied to the output; add if anyone needs it.
 */
#include "atwboot.h"
#include "compkern.h"
#include "zlib.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PIECE (256L * 1024)
#define CAP (PIECE + PIECE / 2 + 1024)
#define SERVER "/usr/lib/t425/compserv.btl"
#define LOCK "/tmp/t425.lock"

static const char *me;
static int op, level, verbose;
static int t4state;                              /* 0 not tried, 1 up, -1 not used */
static long t4pieces, pieces;

static void note(const char *why)
{
    fprintf(stderr, "%s: T425 %s; compressing on the 68030\n", me, why);
    t4state = -1;
}

static void unlock(void)
{
    unlink(LOCK);
}

static int t4_up(void)
{
    char buf[32];
    int fd, n;
    long pid;

    if (t4state)
        return t4state > 0;
    fd = open(LOCK, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0 && errno == EEXIST) {
        FILE *f = fopen(LOCK, "r");
        pid = f && fscanf(f, "%ld", &pid) == 1 ? pid : 0;
        if (f)
            fclose(f);
        if (pid > 0 && kill((int)pid, 0) == 0) {
            sprintf(buf, "busy (pid %ld)", pid);
            note(buf);
            return 0;
        }
        unlink(LOCK);                            /* stale: its owner is gone */
        fd = open(LOCK, O_WRONLY | O_CREAT | O_EXCL, 0644);
    }
    if (fd < 0) {
        note("lock unavailable");
        return 0;
    }
    n = sprintf(buf, "%ld\n", (long)getpid());
    write(fd, buf, n);
    close(fd);
    atexit(unlock);
    atw_verbose = 0;
    if (atw_boot(SERVER, "#400000")) {
        note("did not boot (fpgabios resident? " SERVER " present?)");
        return 0;
    }
    t4state = 1;
    return 1;
}

static unsigned long get4(const unsigned char *b)
{
    return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
}

/* One piece on the T425; -1 = use the 68030 for it (and for the rest of the run). */
static long t4_piece(const unsigned char *in, long n, unsigned char *out)
{
    unsigned char h[4], a[4];
    long len;

    if (!t4_up())
        return -1;
    if (send_op((char)op, level, n) || link_write(in, n) != n || link_read(h, 4, 120000) != 4) {
        note("did not answer");
        return -1;
    }
    len = (long)get4(h);
    if (len < 0 || len > CAP || link_read(a, 4, 5000) != 4) {
        note("failed a piece");
        return -1;
    }
    if (link_read(out, len, 30000) != len) {
        note("sent a short piece");
        return -1;
    }
    if (adler32(adler32(0L, Z_NULL, 0), out, (uInt)len) != get4(a)) {
        note("sent a damaged piece");
        return -1;
    }
    t4pieces++;
    return len;
}

/* Compress in -> out; returns 0, or 1 with a message. */
static int squeeze(FILE *in, FILE *out, const char *name)
{
    static unsigned char *buf, *res;
    long n, len, total = 0, written = 0;
    int first = 1;

    if (!buf && (!(buf = malloc(PIECE)) || !(res = malloc(CAP)))) {
        fprintf(stderr, "%s: out of memory\n", me);
        return 1;
    }
    while ((n = (long)fread(buf, 1, PIECE, in)) > 0 || first) {
        first = 0;
        len = t4state >= 0 ? t4_piece(buf, n, res) : -1;
        if (len < 0)
            len = ck_compress(op, level, buf, n, res, CAP);
        if (len < 0) {
            fprintf(stderr, "%s: %s: compression failed\n", me, name);
            return 1;
        }
        if ((long)fwrite(res, 1, len, out) != len) {
            fprintf(stderr, "%s: %s: write failed\n", me, name);
            return 1;
        }
        pieces++;
        total += n;
        written += len;
        if (n < PIECE)
            break;
    }
    if (ferror(in)) {
        fprintf(stderr, "%s: %s: read failed\n", me, name);
        return 1;
    }
    if (verbose)
        fprintf(stderr, "%s: %s: %ld -> %ld bytes\n", me, name, total, written);
    return 0;
}

int main(int argc, char **argv)
{
    const char *base, *ext;
    int i, n, tostdout = 0, keep = 0, force = 0, rc = 0;

    base = strrchr(argv[0], '/');
    me = base ? base + 1 : argv[0];
    op = strstr(me, "bz") ? 'b' : 'g';
    ext = op == 'b' ? ".bz2" : ".gz";
    level = op == 'b' ? 9 : 6;
    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        const char *o;
        for (o = argv[i] + 1; *o; o++) {
            if (*o >= '1' && *o <= '9')
                level = *o - '0';
            else if (*o == 'c')
                tostdout = 1;
            else if (*o == 'k')
                keep = 1;
            else if (*o == 'f')
                force = 1;
            else if (*o == 'v')
                verbose = 1;
            else {
                fprintf(stderr, "usage: %s [-1..-9] [-c] [-k] [-f] [-v] [FILE...]\n", me);
                return 2;
            }
        }
    }
    if (op == 'b' && level > 3)
        level = 3;                               /* see the header: same output within a 256 KB piece */

    if (i == argc)
        rc = squeeze(stdin, stdout, "stdin");
    for (; i < argc; i++) {
        FILE *in = fopen(argv[i], "rb"), *out;
        char *dst;
        if (!in) {
            fprintf(stderr, "%s: %s: %s\n", me, argv[i], strerror(errno));
            rc = 1;
            continue;
        }
        if (tostdout) {
            rc |= squeeze(in, stdout, argv[i]);
            fclose(in);
            continue;
        }
        dst = malloc(strlen(argv[i]) + 5);
        sprintf(dst, "%s%s", argv[i], ext);
        if (!force && access(dst, F_OK) == 0) {
            fprintf(stderr, "%s: %s exists; -f to overwrite\n", me, dst);
            fclose(in);
            free(dst);
            rc = 1;
            continue;
        }
        if (!(out = fopen(dst, "wb"))) {
            fprintf(stderr, "%s: %s: %s\n", me, dst, strerror(errno));
            fclose(in);
            free(dst);
            rc = 1;
            continue;
        }
        n = squeeze(in, out, argv[i]) | (fclose(out) != 0);
        fclose(in);
        if (n) {
            unlink(dst);                         /* never leave a truncated .gz beside a deleted input */
            rc = 1;
        } else if (!keep)
            unlink(argv[i]);
        free(dst);
    }
    if (verbose)
        fprintf(stderr, "%s: %ld of %ld pieces on the T425\n", me, t4pieces, pieces);
    fflush(stdout);
    return rc;
}
