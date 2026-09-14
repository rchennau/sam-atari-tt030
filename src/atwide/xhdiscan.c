/* NOT WORKING YET (2026-09-13): lists no targets even for the working BlueSCSI SCSI disks, so the XHDI
 * call itself is wrong — most likely gcc promoting the 16-bit opcode/major/minor arguments to 32 bits
 * where XHDI expects words. Fix the binding before trusting any "no IDE target" result.
 *
 * xhdiscan — ask the running disk driver (XHDI) which targets exist, to see whether the ATW800/2's
 * IDE/SD slot is visible (kanban 5d91d23b, item A). XHDI major numbers: 0-7 ACSI, 8-15 SCSI,
 * 16-17 IDE; also prints Drvmap(). Read-only. XHDI calls are made in supervisor mode (Supexec);
 * opcode 1 = XHInqTarget(major, minor, &blocksize, &flags, productname). A first build used opcode 12
 * by mistake and listed nothing — not even the working SCSI disks.
 */
#include <mint/osbind.h>
#include <stdio.h>

#define C_XHDI 0x58484449L                        /* 'XHDI' */

typedef long (*xhdi_fn)(unsigned short, ...);

static long *cookie_jar(void)
{
    return (long *)Setexc(0x5A0 / 4, (void (*)())-1L);
}

static xhdi_fn xhdi;

static long find_xhdi(void)
{
    long *p = cookie_jar();
    for (; p && p[0]; p += 2)
        if (p[0] == C_XHDI)
            xhdi = (xhdi_fn)p[1];
    return 0;
}

static long scan(void)
{
    unsigned short major, minor;
    printf("XHDI version %04lx\n", xhdi(0) & 0xffff);
    for (major = 0; major < 24; major++)
        for (minor = 0; minor < 2; minor++) {
            unsigned long blksz = 0, flags = 0;
            char name[40] = "";
            long r = xhdi(1, major, minor, &blksz, &flags, name);       /* XHInqTarget */
            if (r == 0)
                printf("target %2u.%u: block %lu flags %08lx '%s'%s\n", major, minor, blksz, flags, name,
                       major >= 16 ? "  <- IDE" : "");
        }
    return 0;
}

int main(void)
{
    Supexec(find_xhdi);
    printf("Drvmap: %08lx\n", Drvmap());
    if (!xhdi) {
        printf("no XHDI cookie\n");
        return 1;
    }
    Supexec(scan);
    return 0;
}
