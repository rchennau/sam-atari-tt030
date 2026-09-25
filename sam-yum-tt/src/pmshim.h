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
#include <stdio.h>
size_t ps_fread(void *buf, size_t size, size_t n, FILE *file);
void ps_qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));
long ps_run(int (*prog_main)(int, char **), int argc, char **argv, const unsigned char *in, long n,
            unsigned char *out, long cap);
#endif
