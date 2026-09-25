/* lhamemio — memory-backed stand-ins for the stdio calls lha's encoder makes, so slide.c/huf.c/crcio.c
 * compile unchanged for the T425 (lhaserv.c; tt030-t425-kernel-ports W1). Force-included through a
 * one-line patch to lha.h (t4lha.sh). Nothing here touches the host: after boot the TT answers no
 * host-I/O requests, so a real fread/printf on the T425 would hang. */
#ifndef LHAMEMIO_H
#define LHAMEMIO_H
#include <stdio.h>
#include <stddef.h>

extern const unsigned char *lm_in;
extern long lm_in_len, lm_in_pos;
extern unsigned char *lm_out;
extern long lm_out_cap, lm_out_pos;

size_t t4_fread(void *p, size_t size, size_t n);
size_t t4_fwrite(const void *p, size_t size, size_t n);
int t4_getc(void);
int t4_noprint(const char *fmt, ...);
int t4_nofprint(FILE *f, const char *fmt, ...);
void t4_abort(int code);

#undef fread
#undef fwrite
#undef getc
#undef fgetc
#undef putchar
#undef fflush
#undef printf
#undef fprintf
#undef exit
#define fread(p, s, n, f)  t4_fread((p), (s), (n))
#define fwrite(p, s, n, f) t4_fwrite((p), (s), (n))
#define getc(f)            t4_getc()
#define fgetc(f)           t4_getc()
#define putchar(c)         0
#define fflush(f)          0
#define printf             t4_noprint
#define fprintf            t4_nofprint
#define exit(c)            t4_abort(c)
#endif
