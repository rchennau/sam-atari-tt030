/* atwboot — ATW800/2 T425 link helpers for TT clients (see atwboot.c). */
#ifndef ATWBOOT_H
#define ATWBOOT_H
#include <time.h>
extern long short_writes;                        /* driver writes that sent less than asked */
extern int atw_verbose;                          /* 0: atw_boot prints nothing (default 1) */
long link_write(const unsigned char *buf, long len);
long link_read(unsigned char *buf, long len, long timeout_ms);
long ms_since(clock_t t0);
/* Boot a raw .btl and answer its start-up requests; board is IBOARDSIZE, e.g. "#200000" (2 MB). */
int atw_boot(const char *path, const char *board);
int send_hdr(char op, long len);                 /* op (1) || len (4, little-endian) */
int send_op(char op, int level, long len);        /* op (1) || level (1) || len (4): compserv */
void rate(const char *what, long kb, long ms);   /* one "what  ms  KB/s" line */
#endif
