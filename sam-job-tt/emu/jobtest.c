/* jobtest — NFR-2 correctness lane (tt030-transputer-job-queue): run N pseudo-random jobs per op through job_exec
 * and print one digest line per op. Built twice from the same sources — host gcc (the reference: the 68030 builds
 * these sources with gcc, and the TT showed 68030 == T425 for deflate/bzip2) and INMOS icc -t4 run under the t4
 * emulator — and the two outputs must be identical (emu/run_emu_tests.sh). Same pattern as sam-yum-tt compt4test. */
#include <stdio.h>
#include <stdlib.h>
#include "jobwire.h"
#include "jobexec.h"
#include "bzlib.h"
#include "edsign.h"
#ifndef JOBS
#define JOBS 1000
#endif
#define MAXIN 16384L
#ifndef CJOBS
#define CJOBS 50                 /* crypto ops are slow under t4 (Ed25519 sign + verify per job) */
#endif
static unsigned long seed = 2463534242UL;
static unsigned long rnd(void) { seed ^= seed << 13; seed &= 0xFFFFFFFFUL; seed ^= seed >> 17; seed ^= seed << 5; seed &= 0xFFFFFFFFUL; return seed; }
static void fill(unsigned char *b, long n, int kind)
{
    long i;
    for (i = 0; i < n; i++)
        b[i] = kind == 0 ? (unsigned char)rnd()                                     /* incompressible */
             : kind == 1 ? (unsigned char)("the quick brown fox jumps over a lazy dog\n"[(i * 7 + i / 97) % 42])
             : (unsigned char)((i / (1 + rnd() % 64)) & 0xff);                        /* runs */
}
/* Known answers: a logic bug shared by both builds cannot show up in the host-vs-t4 diff, so check results
 * against fixed vectors and round trips too (mutation check 2026-10-13: a shared SHA bug passed the diff). */
static int kat(void)
{
    static const unsigned char abc[32] = { 0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
                                           0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad };
    static const unsigned char empty[32] = { 0xe3,0xb0,0xc4,0x42,0x98,0xfc,0x1c,0x14,0x9a,0xfb,0xf4,0xc8,0x99,0x6f,0xb9,0x24,
                                             0x27,0xae,0x41,0xe4,0x64,0x9b,0x93,0x4c,0xa4,0x95,0x99,0x1b,0x78,0x52,0xb8,0x55 };
    unsigned char d[32]; int i, bad = 0;
    if (job_exec('S', 0, (const unsigned char *)"abc", 3, d, 32) != 32) bad++; else for (i = 0; i < 32; i++) if (d[i] != abc[i]) { bad++; break; }
    if (job_exec('S', 0, (const unsigned char *)"", 0, d, 32) != 32) bad++; else for (i = 0; i < 32; i++) if (d[i] != empty[i]) { bad++; break; }
    printf("kat sha256 %s\n", bad ? "FAIL" : "ok");
    return bad;
}
static int hexb(unsigned char *o, const char *h) { int i = 0; while (h[0] && h[1]) { unsigned v; sscanf(h, "%2x", &v); o[i++] = (unsigned char)v; h += 2; } return i; }
static int same(const unsigned char *a, const unsigned char *b, int n) { int i; for (i = 0; i < n; i++) if (a[i] != b[i]) return 0; return 1; }
/* RFC 7748 5.2, RFC 8032 7.1 test 1, RFC 8439 2.4.2 (ciphertext computed independently with Python cryptography) */
static int kat_crypto(void)
{
    static unsigned char in[256], out[256], exp[256];
    static const char *pt = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
    int bad = 0, i, n;
    hexb(in, "a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c");
    hexb(exp, "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552");
    if (job_exec('X', 0, in, 64, out, 32) != 32 || !same(out, exp, 32)) bad |= 1;
    hexb(in, "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
    hexb(in + 64, "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    if (job_exec('V', 0, in, 96, out, 1) != 1 || out[0] != 1) bad |= 2;
    in[5] ^= 1;
    if (job_exec('V', 0, in, 96, out, 1) != 1 || out[0] != 0) bad |= 4;
    for (i = 0; i < 32; i++) in[i] = (unsigned char)i;
    hexb(in + 32, "000000000000004a00000000"); in[44] = 1; in[45] = in[46] = in[47] = 0;
    for (n = 0; pt[n]; n++) in[48 + n] = (unsigned char)pt[n];
    hexb(exp, "6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0bf91b65c5524733ab8f593dabcd62b3571639d624e65152ab8f530c359f0861d807ca0dbf500d6a6156a38e088a22b65e52bc514d16ccf806818ce91ab77937365af90bbf74a35be6b40b8eedf2785e42874d");
    if (job_exec('C', 0, in, 48 + n, out, 256) != n || !same(out, exp, n)) bad |= 8;
    printf("kat crypto %s\n", bad ? "FAIL" : "ok");
    if (bad) printf("kat crypto bits %d\n", bad);
    return bad;
}
int main(void)
{
    static const char ops[] = "ESzgrbiXVC";
    static unsigned char in[MAXIN + 8], out[MAXIN * 2 + 2048], tmp[MAXIN * 2 + 2048];
    int k, j;
    kat();
    kat_crypto();
    for (k = 0; ops[k]; k++) {
        unsigned long dig = 1, fails = 0;
        int jobs = (ops[k] == 'X' || ops[k] == 'V') ? CJOBS : JOBS;
        for (j = 0; j < jobs; j++) {
            long n = j < 3 ? (long)j : (long)(rnd() % MAXIN), olen, i;
            int level = 1 + (int)(rnd() % 9), op = ops[k];
            fill(in, n, (int)(rnd() % 3));
            if (op == 'X') n = 64;
            if (op == 'C' && n < 48) n = 48;
            if (op == 'V') {                            /* sign a random message, corrupt every 2nd job: verdict must follow */
                unsigned char sec[32], pub[32]; long m = (long)(rnd() % 512);
                for (i = 0; i < 32; i++) sec[i] = (unsigned char)rnd();
                fill(tmp, m, 0);
                edsign_sec_to_pub(pub, sec);
                edsign_sign(in, pub, sec, tmp, (size_t)m);
                for (i = 0; i < 32; i++) in[64 + i] = pub[i];
                for (i = 0; i < m; i++) in[96 + i] = tmp[i];
                n = 96 + m;
                if (j & 1) in[(long)(rnd() % n)] ^= (unsigned char)(1 + rnd() % 255);
                olen = job_exec('V', 0, in, n, out, (long)sizeof out);
                if (olen != 1 || out[0] != ((j & 1) ? 0 : 1)) fails++;
                dig = (dig * 31 + (unsigned long)(olen == 1 ? out[0] : 9)) & 0xFFFFFFFFUL;
                continue;
            }
            if (op == 'b') level = 1;                   /* bzip2 above -1 does not fit the T425 heap (Phase 2) */
            if (op == 'i') {                            /* round trip: raw deflate ('r') then inflate ('i') */
                /* 'r' writes type (1) || raw stream; placed at tmp+3, the 4-byte size then overwrites tmp[0..3],
                 * giving size (4, LE) || raw stream = the 'i' payload */
                long r = job_exec('r', level, in, n, tmp + 3, (long)sizeof tmp - 3);
                if (r < 1) { fails++; continue; }
                tmp[0] = (unsigned char)n; tmp[1] = (unsigned char)(n >> 8); tmp[2] = (unsigned char)(n >> 16); tmp[3] = (unsigned char)(n >> 24);
                olen = job_exec('i', 0, tmp, r + 3, out, (long)sizeof out);
                if (olen != n) { fails++; continue; }
                for (i = 0; i < n; i++) if (out[i] != in[i]) break;
                if (i < n) { fails++; continue; }
            } else
                olen = job_exec(op, level, in, n, out, (long)sizeof out);
            if (op == 'b' && olen > 0) {                /* bzip2 round trip */
                unsigned int dl = (unsigned int)sizeof tmp; long q;
                if (BZ2_bzBuffToBuffDecompress((char *)tmp, &dl, (char *)out, (unsigned int)olen, 0, 0) != BZ_OK || (long)dl != n) { fails++; continue; }
                for (q = 0; q < n; q++) if (tmp[q] != in[q]) break;
                if (q < n) { fails++; continue; }
            }
            if (olen < 0) { fails++; dig = dig * 31 + (unsigned long)(-olen); continue; }
            dig = (dig * 31 + (unsigned long)olen) & 0xFFFFFFFFUL;
            for (i = 0; i < olen; i++) dig = (dig * 31 + out[i]) & 0xFFFFFFFFUL;
        }
        printf("op %c jobs %d failed %lu digest %08lx\n", ops[k], jobs, fails, dig);
    }
    return 0;
}
