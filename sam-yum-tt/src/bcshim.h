/* bcshim — see bcshim.c. Prepended (via bcren.h, t4bc.sh) to every bc 1.05 source on both CPUs. */
#ifndef BCSHIM_H
#define BCSHIM_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
int bs_printf(const char *fmt, ...);
int bs_fprintf(FILE *f, const char *fmt, ...);
int bs_vfprintf(FILE *f, const char *fmt, va_list ap);
int bs_putchar(int c);
int bs_putc(int c, FILE *f);
int bs_puts(const char *s);
int bs_fputs(const char *s, FILE *f);
int bs_fflush(FILE *f);
size_t bs_fwrite(const void *p, size_t size, size_t n, FILE *f);
int bs_getchar(void);
int bs_read(int fd, char *buf, int max);
int bs_fileno(FILE *f);
int bs_isatty(int fd);
void (*bs_signal(int sig, void (*h)(int)))(int);
void bs_exit(int code);
char *bs_getenv(const char *name);
FILE *bs_fopen(const char *name, const char *mode);
int bs_fclose(FILE *f);
/* Run bc [-l] over the program text prog (n bytes, as stdin). Result into out: stdout length (4, LE), bc's
 * exit code (1), stdout, then stderr. Returns the result length, or -1 if out is too small. */
long bs_run(int mathlib, const char *prog, long n, unsigned char *out, long cap);
#endif
