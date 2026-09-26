/* atwvid — read-only dump of the ATW800/2 video state from the 68030 (no T425 involved): FPGA info, VTG, ATWInfo
 * (starts "ATW8XVDI" when XVDI is healthy), CLUT shadow, first framebuffer bytes. Incident 2026-09-25. */
#include <mint/osbind.h>
#include <stdio.h>
static unsigned char b[5][64];
static const unsigned long A[5] = { 0xFEDFF200UL, 0xFEDFF800UL, 0xFEDFDC00UL, 0xFEDFDE00UL, 0xFEC00000UL };
static const char *N[5] = { "FPGA info", "VTG", "ATWInfo", "CLUT shadow", "VidMem" };
static long rd(void) { int k, i; for (k = 0; k < 5; k++) for (i = 0; i < 64; i++) b[k][i] = ((volatile unsigned char *)A[k])[i]; return 0; }
int main(void)
{
    int k, i;
    Supexec(rd);
    for (k = 0; k < 5; k++) {
        printf("%-11s %08lx:", N[k], A[k]);
        for (i = 0; i < 64; i++) printf("%s%02x", i % 16 ? " " : "\n  ", b[k][i]);
        printf("\n");
    }
    return 0;
}
