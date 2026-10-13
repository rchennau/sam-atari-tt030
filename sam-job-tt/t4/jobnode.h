/* jobnode — transputer side of jobwire, shared by the T425 dispatcher and both TRAM workers. */
#ifndef JOBNODE_H
#define JOBNODE_H
#include <channel.h>
unsigned long g4(const unsigned char *b);
void p4(unsigned char *b, unsigned long v);
/* Read the payload for header hdr from in, execute it here, write the reply to out. */
void job_run_here(Channel *in, Channel *out, unsigned char *hdr);
/* Read the payload from in, pass header + payload to fwd_out, relay the reply from fwd_in to out. */
void job_forward(Channel *in, Channel *out, unsigned char *hdr, Channel *fwd_out, Channel *fwd_in);
#endif
