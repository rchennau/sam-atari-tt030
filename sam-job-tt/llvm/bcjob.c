/* bcjob — GNU bc 1.05 (op 'c') in the resident job image, T425 node only (tt030-ttjob-ports FR-3).
 * bcserv was written for one request per boot. Here the image stays up, so before every job bc's globals go
 * back to their boot values: build_llvm.sh links bc's objects into one relocatable and marks its .data/.bss with
 * bc_{data,bss}_{start,end}; .data is snapshotted on the first job and restored, .bss zeroed, on each one after.
 * (Renamed sections are placed after the image's .bss, where the heap starts: bc's globals overlapped the heap.) bcshim's
 * BS_TRACK frees whatever bc left on the heap at the end of each run. */
#include <stdlib.h>
#include <string.h>

extern char bc_data_start[], bc_data_end[], bc_bss_start[], bc_bss_end[];
long bc_kernel(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap);

long bc_job(int op, int level, const unsigned char *in, long n, unsigned char *out, long cap)
{
    static char *snap;
    long d = bc_data_end - bc_data_start;
    if (!snap) {
        if (!(snap = malloc(d > 0 ? d : 1)))
            return -1;
        memcpy(snap, bc_data_start, d);
    } else
        memcpy(bc_data_start, snap, d);
    memset(bc_bss_start, 0, bc_bss_end - bc_bss_start);
    return bc_kernel(op, level, in, n, out, cap);
}
