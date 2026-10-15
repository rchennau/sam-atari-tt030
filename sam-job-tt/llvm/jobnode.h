/* jobnode — transputer side of jobwire, shared by the T425 dispatcher and both TRAM workers. */
#ifndef JOBNODE_H
#define JOBNODE_H
#include <channel.h>
unsigned long g4(const unsigned char *b);
void p4(unsigned char *b, unsigned long v);
/* Run the job (hdr + its payload in `in`, n bytes) on this node; write reply header + output into rep.
 * Returns the total reply length (LREP_HDR + output). */
long job_reply(const unsigned char *hdr, const unsigned char *in, long n, unsigned char *rep, int node);
/* Read one request header + payload from c into hdr / buf; returns the payload length, or -1 if too long
 * (the payload is then drained and discarded). */
long job_read(Channel *c, unsigned char *hdr, unsigned char *buf);
/* Read one reply (header + output) from c into rep; returns its total length. */
long rep_read(Channel *c, unsigned char *rep);
#endif
