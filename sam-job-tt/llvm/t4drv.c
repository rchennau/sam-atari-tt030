/* t4 harness: node 0 plays ttjobd. Boots jobllvm0.btl (SOLO) on node 1 over link 3 (-> node 1 link 0, its host
 * link) with llvm's boot framing, waits for RDY0, sends one 4-byte echo job, prints the reply. */
#include <channel.h>
#include <stdio.h>
#include <string.h>
#include "jobwire.h"
#define LOUT(n) ((Channel *)(0x80000000u + 4u * (unsigned)(n)))
#define LIN(n)  ((Channel *)(0x80000010u + 4u * (unsigned)(n)))
static unsigned long ad(const unsigned char *p, long n) { unsigned long a = 1, b = 0; for (long i = 0; i < n; i++) { a = (a + p[i]) % 65521; b = (b + a) % 65521; } return (b << 16) | a; }
static void p4(unsigned char *b, unsigned long v) { b[0] = v; b[1] = v >> 8; b[2] = v >> 16; b[3] = v >> 24; }
int main(void)
{
    static unsigned char img[400000], rdy[4], h[JOB_HDR], rep[LREP_HDR + 4], pay[4] = { 1, 2, 3, 4 };
    FILE *f = fopen("jobllvm0.btl", "rb");
    long sz = (long)fread(img, 1, sizeof img, f);
    fclose(f);
    int len1 = img[0];
    unsigned char *p = img + 1 + len1;
    unsigned len2 = p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24);
    ChanOut(LOUT(3), img, 1 + len1); ChanOut(LOUT(3), p, 4); ChanOut(LOUT(3), p + 4, (int)len2);
    printf("booted %ld bytes; waiting for RDY\n", sz); fflush(stdout);
    ChanIn(LIN(3), rdy, 4);
    printf("got %.4s; sending echo job\n", rdy); fflush(stdout);
    h[0] = 7; h[1] = 0; h[2] = OP_ECHO; h[3] = 0; h[4] = 0; p4(h + 5, 4); p4(h + 9, ad(pay, 4));
    ChanOut(LOUT(3), h, JOB_HDR); ChanOut(LOUT(3), pay, 4);
    ChanIn(LIN(3), rep, LREP_HDR + 4);
    printf("reply id %d status %d out %d %d %d %d node %d\n", rep[0], rep[2], rep[LREP_HDR], rep[LREP_HDR+1], rep[LREP_HDR+2], rep[LREP_HDR+3], rep[15]);
    return 0;
}
