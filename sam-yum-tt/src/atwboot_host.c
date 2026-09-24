/* atwboot_host — host stand-in for atwboot.c: no ATW board, so atw_boot fails and tgzip takes its
 * 68030 path. Lets the host test run the real tgzip.c (scripts/test_t425_compress.py). */
#include "atwboot.h"
long short_writes;
int atw_verbose = 1;
long link_write(const unsigned char *buf, long len) { (void)buf; (void)len; return -1; }
long link_read(unsigned char *buf, long len, long timeout_ms) { (void)buf; (void)len; (void)timeout_ms; return 0; }
long ms_since(clock_t t0) { return (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC); }
int atw_boot(const char *path, const char *board) { (void)path; (void)board; return 1; }
int send_hdr(char op, long len) { (void)op; (void)len; return 1; }
int send_op(char op, int level, long len) { (void)op; (void)level; (void)len; return 1; }
void rate(const char *what, long kb, long ms) { (void)what; (void)kb; (void)ms; }
