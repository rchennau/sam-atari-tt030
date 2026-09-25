/* jpegserv — libjpeg 8b on the T425 (tt030-t425-kernel-ports W2). The server loop is t4serv.c; this file
 * is the family's hooks. libjpeg is compiled unchanged, so the T425 result is the 68030 result, provided
 * the parameters are set in the same order as cjpeg/djpeg do (they are, for the plain invocations the TT
 * front ends send here):
 *   op 'D'           decode: payload = JPEG bytes; result = width (4) || height (4) || components (1)
 *                    || rows (width * components bytes each), as djpeg with no switches produces
 *   op 'C', q | 0    encode: payload = width (4) || height (4) || components (1: gray, 3: RGB) || rows;
 *                    level = -quality (1-100), 0 = cjpeg's default; result = the JPEG file bytes, as cjpeg
 *                    with no switches but -quality on a PPM (maxval 255) produces
 */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cdjpeg.h"                              /* jpeglib.h + set_quality_ratings (rdswitch.c) */
#include "t4serv.h"

struct t4err {
    struct jpeg_error_mgr pub;
    jmp_buf jb;
};

static long last_code = -1, last_stage = 0;      /* op 'Q': why the previous call failed */

static void t4_error_exit(j_common_ptr c)
{
    last_code = c->err->msg_code;
    longjmp(((struct t4err *)c->err)->jb, 1);
}

static void t4_no_output(j_common_ptr c)
{
    (void)c;                                     /* no host I/O on the T425 after boot */
}

static unsigned long get4(const unsigned char *b)
{
    return b[0] | ((unsigned long)b[1] << 8) | ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
}

static void put4(unsigned char *b, unsigned long v)
{
    b[0] = (unsigned char)v;
    b[1] = (unsigned char)(v >> 8);
    b[2] = (unsigned char)(v >> 16);
    b[3] = (unsigned char)(v >> 24);
}

static long decode(const unsigned char *in, long n, unsigned char *out, long cap)
{
    struct jpeg_decompress_struct c;
    struct t4err e;
    long stride, need;
    JSAMPROW row;

    c.err = jpeg_std_error(&e.pub);
    e.pub.error_exit = t4_error_exit;
    e.pub.output_message = t4_no_output;
    if (setjmp(e.jb)) {
        jpeg_destroy_decompress(&c);
        return -1;
    }
    jpeg_create_decompress(&c);
    jpeg_mem_src(&c, (unsigned char *)in, (unsigned long)n);
    (void)jpeg_read_header(&c, TRUE);
    jpeg_start_decompress(&c);
    stride = (long)c.output_width * c.output_components;
    need = 9 + stride * (long)c.output_height;
    if (need > cap) {
        last_stage = 1;
        jpeg_destroy_decompress(&c);
        return -1;
    }
    put4(out, c.output_width);
    put4(out + 4, c.output_height);
    out[8] = (unsigned char)c.output_components;
    while (c.output_scanline < c.output_height) {
        row = out + 9 + stride * (long)c.output_scanline;
        (void)jpeg_read_scanlines(&c, &row, 1);
    }
    jpeg_finish_decompress(&c);
    jpeg_destroy_decompress(&c);
    return need;
}

static long encode(int quality, const unsigned char *in, long n, unsigned char *out, long cap)
{
    struct jpeg_compress_struct c;
    struct t4err e;
    unsigned char *mem = NULL;
    unsigned long size = 0, w, h;
    int comp;
    char q[8];
    JSAMPROW row;

    if (n < 9)
        return -1;
    w = get4(in);
    h = get4(in + 4);
    comp = in[8];
    if ((comp != 1 && comp != 3) || (long)(9 + w * h * comp) != n) {
        last_stage = 2;
        return -1;
    }
    c.err = jpeg_std_error(&e.pub);
    e.pub.error_exit = t4_error_exit;
    e.pub.output_message = t4_no_output;
    if (setjmp(e.jb)) {
        jpeg_destroy_compress(&c);
        free(mem);
        return -1;
    }
    jpeg_create_compress(&c);
    c.in_color_space = JCS_RGB;                  /* cjpeg: "arbitrary guess", then defaults */
    jpeg_set_defaults(&c);
    c.image_width = (JDIMENSION)w;               /* what rdppm's start_input sets */
    c.image_height = (JDIMENSION)h;
    c.input_components = comp;
    c.in_color_space = comp == 3 ? JCS_RGB : JCS_GRAYSCALE;
    c.data_precision = 8;
    jpeg_default_colorspace(&c);
    if (quality > 0) {                           /* cjpeg's -quality, second parse_switches pass */
        sprintf(q, "%d", quality);
        if (!set_quality_ratings(&c, q, FALSE))
            longjmp(e.jb, 1);
    }
    jpeg_mem_dest(&c, &mem, &size);
    jpeg_start_compress(&c, TRUE);
    while (c.next_scanline < c.image_height) {
        row = (JSAMPROW)(in + 9 + (long)c.next_scanline * (long)(w * comp));
        (void)jpeg_write_scanlines(&c, &row, 1);
    }
    jpeg_finish_compress(&c);
    jpeg_destroy_compress(&c);
    if ((long)size > cap) {
        last_stage = 3;
        free(mem);
        return -1;
    }
    memcpy(out, mem, size);
    free(mem);
    return (long)size;
}

/* Width and height from the first SOF marker, for sizing a decode's output. 0 if not found. */
static long sof_size(const unsigned char *p, long n)
{
    long i = 2;
    while (i + 9 < n) {
        if (p[i] != 0xFF) {
            i++;
            continue;
        }
        if (p[i + 1] >= 0xC0 && p[i + 1] <= 0xCF && p[i + 1] != 0xC4 && p[i + 1] != 0xC8
            && p[i + 1] != 0xCC) {
            long h = (p[i + 5] << 8) | p[i + 6], w = (p[i + 7] << 8) | p[i + 8];
            return w * h * p[i + 9];
        }
        if (p[i + 1] == 0xD8 || p[i + 1] == 0x01 || (p[i + 1] >= 0xD0 && p[i + 1] <= 0xD7)) {
            i += 2;
            continue;
        }
        i += 2 + ((p[i + 2] << 8) | p[i + 3]);
    }
    return 0;
}

long t4_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    long r;
    if (op == 'Q') {                             /* diagnostics: libjpeg msg_code + stage of the last failure */
        if (cap < 8)
            return -1;
        put4(out, (unsigned long)last_code);
        put4(out + 4, (unsigned long)last_stage);
        return 8;
    }
    last_code = -1;
    last_stage = 0;
    if (op == 'D')
        r = decode(in, n, out, cap);
    else if (op == 'C')
        r = encode(level, in, n, out, cap);
    else
        return -1;
    if (r < 0 && last_stage == 0)
        last_stage = 99;                         /* failed outside libjpeg's error path */
    return r;
}

long t4_out_cap(int op, int level, const unsigned char *in, long n)
{
    (void)level;
    if (op == 'Q')
        return 64;
    if (op == 'D') {
        long s = sof_size(in, n);
        return s > 0 ? 9 + s + 64 : 0;
    }
    return n + 4096;                             /* a JPEG is smaller than its raw rows */
}
