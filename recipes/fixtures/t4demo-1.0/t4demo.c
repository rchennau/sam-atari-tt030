/* t4demo — fixture for the T425 recipe path (tt030-t425-kernel-ports FR-3). Compresses stdin (up to
 * 64 KB) into one gzip member on the T425 when /etc/t425/enabled/t4demo exists; otherwise says why and
 * exits 3. It exists to prove build_recipe + t4call + the -t425-on switch end to end, not to be used. */
#include <stdio.h>
#include "t4call.h"

int main(void)
{
    static unsigned char in[65536], out[100000];
    long n = (long)fread(in, 1, sizeof in, stdin), len = -1;
    const char *why = "not enabled (/etc/t425/enabled/t4demo absent)";

    if (t4_enabled("t4demo"))
        len = t4call("/usr/lib/t425/t4demo.btl", 'g', 6, in, n, out, sizeof out, &why);
    if (len < 0) {
        fprintf(stderr, "t4demo: 68030 (T425 %s)\n", why);
        return 3;
    }
    fwrite(out, 1, (size_t)len, stdout);
    fprintf(stderr, "t4demo: T425, %ld bytes\n", len);
    return 0;
}
