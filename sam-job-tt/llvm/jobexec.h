/* jobexec — run one jobwire op on a buffer. Portable C (no channels). */
#ifndef JOBEXEC_H
#define JOBEXEC_H
/* Returns the output length (>= 0), or -ST_BADOP for an op this build does not carry, -ST_FAILED if the kernel
 * failed. Compression ops exist only when built with JOB_COMPRESS (the T425 node). */
long job_exec(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);
#endif
