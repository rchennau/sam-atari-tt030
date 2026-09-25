/* lhaserv — lha's -lh5-/-lh6-/-lh7- encoder on the T425 (tt030-t425-kernel-ports W1). The server loop is
 * t4serv.c; this file is the family's two hooks plus the memory streams of lhamemio.h. lha's own
 * slide.c/huf.c/crcio.c/maketree.c are compiled unchanged, so the T425 result is the 68030 result.
 *   op '5' | '6'        compress the whole input with that method; no lh7 (see t4lha.sh), and a
 *                       member's stream cannot be split
 *   result              crc16 (2 bytes, LE) || packed bytes; packed stops at the input size, as lha's
 *                       putcode does ("unpackable" -> the TT stores the member as -lh0-)
 *   op 'd', level 5|6   decode: payload = original size (4, LE) || packed bytes; result = crc16 (2) ||
 *                       the original bytes (lha's own decode(), as extract.c's decode_lzhuf calls it)
 */
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>
#define LHA_MAIN_SRC                             /* this file owns lha.h's EXTERN globals */
#include "lha.h"
#include "t4serv.h"

const unsigned char *lm_in;
long lm_in_len, lm_in_pos;
unsigned char *lm_out;
long lm_out_cap, lm_out_pos;
static jmp_buf t4_fail;

size_t t4_fread(void *p, size_t size, size_t n)
{
    long want = (long)(size * n), left = lm_in_len - lm_in_pos;
    if (want > left)
        want = left;
    memcpy(p, lm_in + lm_in_pos, (size_t)want);
    lm_in_pos += want;
    return size ? (size_t)want / size : 0;
}

size_t t4_fwrite(const void *p, size_t size, size_t n)
{
    long len = (long)(size * n);
    if (lm_out_pos + len > lm_out_cap)
        return 0;                                /* putcode then calls fatal_error: the call fails */
    memcpy(lm_out + lm_out_pos, p, (size_t)len);
    lm_out_pos += len;
    return n;
}

int t4_getc(void)
{
    return lm_in_pos < lm_in_len ? lm_in[lm_in_pos++] : EOF;
}

int t4_noprint(const char *fmt, ...)
{
    (void)fmt;
    return 0;
}

int t4_nofprint(FILE *f, const char *fmt, ...)
{
    (void)f;
    (void)fmt;
    return 0;
}

void t4_abort(int code)
{
    longjmp(t4_fail, code ? code : 1);
}

void fatal_error(char *msg)
{
    (void)msg;
    t4_abort(1);
}

void error(char *msg, char *arg)
{
    (void)msg;
    (void)arg;
    t4_abort(1);
}

static unsigned long get4(const unsigned char *b)
{
    return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
}

long t4_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    static int crctable_made;
    int method = op == 'd' ? level : op - '0';

    if (method < 5 || method > 6 || cap < 2)   /* lh7 needs SUPPORT_LH7, which the TT build lacks */
        return -1;
    if (op == 'd' && n < 4)
        return -1;
    if (setjmp(t4_fail))
        return -1;
    if (!crctable_made) {
        make_crctable();
        crctable_made = 1;
    }
    lm_in = op == 'd' ? in + 4 : in;
    lm_in_len = op == 'd' ? n - 4 : n;
    lm_in_pos = 0;
    lm_out = out + 2;
    lm_out_cap = cap - 2;
    lm_out_pos = 0;
    quiet = 1;
    text_mode = 0;
    reading_size = 0;
    if (op == 'd') {                             /* as extract.c: lh5 dicbit 13, lh6 15 */
        interface.method = method;
        interface.dicbit = method == 6 ? 15 : 13;
        interface.infile = stdin;
        interface.outfile = stdout;
        interface.original = (long)get4(in);
        interface.packed = n - 4;
        if (interface.original > cap - 2)
            return -1;
        decode(&interface);
        out[0] = (unsigned char)crc;
        out[1] = (unsigned char)(crc >> 8);
        return 2 + lm_out_pos;
    }
    /* lha numbers methods lh0=0 ... lh7=7 (LZHUFF5_METHOD_NUM etc.) */
    interface.method = encode_alloc(method);
    interface.infile = stdin;                    /* never read: fread is t4_fread */
    interface.outfile = stdout;
    interface.original = n;
    encode(&interface);
    out[0] = (unsigned char)crc;
    out[1] = (unsigned char)(crc >> 8);
    return 2 + lm_out_pos;
}

long t4_out_cap(int op, int level, const unsigned char *in, long n)
{
    (void)level;
    if (op == 'd')                               /* decode: the original size leads the payload */
        return n >= 4 ? (long)get4(in) + 2 + 64 : 0;
    return n + 2 + 64;                           /* putcode stops at the input size */
}
