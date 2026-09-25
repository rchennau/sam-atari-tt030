/* pmshim — see pmshim.c. */
#ifndef PMSHIM_H
#define PMSHIM_H
#include <stddef.h>
void ps_exit(int code);
void *ps_malloc(size_t n);
void ps_free(void *p);
long ps_time(long *t);
int ps_getpid(void);
void ps_srand(unsigned s);
int ps_rand(void);
void ps_qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));
long ps_run(int ncolors, const unsigned char *ppm, long n, unsigned char *out, long cap);
#endif
