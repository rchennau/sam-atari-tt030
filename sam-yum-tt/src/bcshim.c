/* bcshim — bc 1.05's stdio, signal, getenv and exit calls, in memory, so bc's own sources run unchanged on
 * the T425 (bcserv.c) AND in tbc's 68030 fallback (tt030-t425-kernel-ports W3). No host-runtime call may
 * be linked into a T425 server (the libjpeg getenv lesson), so every one bc makes is renamed here:
 *   stdin    the whole program text (tbc concatenates its files, then stdin; bc then sees one stdin)
 *   stdout   captured; stderr captured separately (bc's error messages)
 *   getenv   always NULL: BC_ENV_ARGS / BC_LINE_LENGTH / POSIXLY_CORRECT are not honoured (line length
 *            70, stock's default); isatty 0 (never interactive); signal a no-op; exit -> longjmp.
 * One run per process on either CPU (t4call boots a fresh server per process), so bc's globals start clean. */
#include <setjmp.h>
#include "bcshim.h"

static const char *bs_in;
static long bs_in_len, bs_in_pos;
static unsigned char *bs_out;
static long bs_cap, bs_olen;
static char bs_err[4096];
static long bs_elen;
static int bs_full, bs_code;
static jmp_buf bs_jb;

extern int bc_main(int argc, char **argv);

static void bs_emit(FILE *f, const char *s, long n)
{
    if (f == stderr) {
        if (n > (long)sizeof bs_err - bs_elen)
            n = (long)sizeof bs_err - bs_elen;
        memcpy(bs_err + bs_elen, s, (size_t)n);
        bs_elen += n;
    } else if (bs_olen + n > bs_cap) {
        bs_full = 1;
        longjmp(bs_jb, 3);
    } else {
        memcpy(bs_out + bs_olen, s, (size_t)n);
        bs_olen += n;
    }
}

int bs_vfprintf(FILE *f, const char *fmt, va_list ap)
{
    static char b[4096];                         /* bc formats short messages; numbers go through putchar */
    int n = vsprintf(b, fmt, ap);
    bs_emit(f, b, n);
    return n;
}

int bs_fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = bs_vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

int bs_printf(const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = bs_vfprintf(stdout, fmt, ap);
    va_end(ap);
    return n;
}

int bs_putc(int c, FILE *f)
{
    char ch = (char)c;
    bs_emit(f, &ch, 1);
    return c & 255;
}

int bs_putchar(int c)
{
    return bs_putc(c, stdout);
}

int bs_fputs(const char *s, FILE *f)
{
    bs_emit(f, s, (long)strlen(s));
    return 0;
}

int bs_puts(const char *s)
{
    bs_fputs(s, stdout);
    return bs_putc('\n', stdout);
}

int bs_fflush(FILE *f)
{
    (void)f;
    return 0;
}

size_t bs_fwrite(const void *p, size_t size, size_t n, FILE *f)
{
    bs_emit(f, (const char *)p, (long)(size * n));
    return n;
}

int bs_getchar(void)
{
    return bs_in_pos < bs_in_len ? (unsigned char)bs_in[bs_in_pos++] : EOF;
}

int bs_read(int fd, char *buf, int max)
{
    long n = bs_in_len - bs_in_pos;
    (void)fd;
    if (n > max)
        n = max;
    memcpy(buf, bs_in + bs_in_pos, (size_t)n);
    bs_in_pos += n;
    return (int)n;
}

int bs_fileno(FILE *f)
{
    (void)f;
    return 0;
}

int bs_isatty(int fd)
{
    (void)fd;
    return 0;
}

void (*bs_signal(int sig, void (*h)(int)))(int)
{
    (void)sig;
    return h;
}

void bs_exit(int code)
{
    bs_code = code;
    longjmp(bs_jb, code == 0 ? 1 : 2);
}

char *bs_getenv(const char *name)
{
    (void)name;
    return NULL;
}

FILE *bs_fopen(const char *name, const char *mode)
{
    (void)name;
    (void)mode;
    return NULL;                                 /* bc only opens file arguments; tbc passes none */
}

int bs_fclose(FILE *f)
{
    (void)f;
    return 0;
}

long bs_run(int mathlib, const char *prog, long n, unsigned char *out, long cap)
{
    char *argv[3];
    int argc = 0;
    if (cap < 5)
        return -1;
    bs_in = prog;
    bs_in_len = n;
    bs_in_pos = 0;
    bs_out = out + 5;
    bs_cap = cap - 5 - (long)sizeof bs_err;
    bs_olen = bs_elen = 0;
    bs_full = bs_code = 0;
    argv[argc++] = "bc";
    if (mathlib)
        argv[argc++] = "-l";
    argv[argc] = NULL;
    if (bs_cap < 0)
        return -1;
    if (setjmp(bs_jb) == 0)
        bc_main(argc, argv);
    if (bs_full)
        return -1;
    out[0] = (unsigned char)bs_olen; out[1] = (unsigned char)(bs_olen >> 8);
    out[2] = (unsigned char)(bs_olen >> 16); out[3] = (unsigned char)(bs_olen >> 24);
    out[4] = (unsigned char)bs_code;
    memcpy(out + 5 + bs_olen, bs_err, (size_t)bs_elen);
    return 5 + bs_olen + bs_elen;
}
