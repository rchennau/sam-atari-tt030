/* t4serv — generic T425 kernel server loop (tt030-t425-kernel-ports FR-2). A family server is this
 * loop plus one t4_kernel(); see compserv.c. Wire format: t4call.h / the plan's Eng §2. */
#ifndef T4SERV_H
#define T4SERV_H
/* Returns the result length, or -1 (the TT then runs the kernel on the 68030). */
long t4_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);
/* Worst-case output for n input bytes; the server allocates this much. */
long t4_out_cap(long n);
#endif
