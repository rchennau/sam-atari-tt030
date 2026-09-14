/* atwprobe — check which ATW800/2 address map applies on the TT (kanban 5d91d23b, item A).
 * Programmer's Manual TT column: FPGA info (32-byte build string) 0xFEDFF200, CPLD version 0xFEFFFAD8.
 * The vendor TMANDEL readme instead puts the TT's C011/CPLD block at 0xFAFFFAC0 (-> version 0xFAFFFAD8).
 * All reads; the 0xFA... read runs last, alone, since an unmapped address bus-errors this probe.
 */
#include <mint/osbind.h>
#include <stdio.h>

static unsigned char info[33], v1, v2;
static long rd_info(void) { int i; for (i = 0; i < 32; i++) info[i] = ((volatile unsigned char *)0xFEDFF200UL)[i]; return 0; }
static long rd_v1(void) { v1 = *(volatile unsigned char *)0xFEFFFAD8UL; return 0; }
static long rd_v2(void) { v2 = *(volatile unsigned char *)0xFAFFFAD8UL; return 0; }

int main(void)
{
    int i;
    Supexec(rd_info);
    printf("FPGA info @0xFEDFF200: \"");
    for (i = 0; i < 32; i++) putchar(info[i] >= 32 && info[i] < 127 ? info[i] : '.');
    printf("\"\nhex:");
    for (i = 0; i < 32; i++) printf(" %02x", info[i]);
    printf("\n");
    Supexec(rd_v1);
    printf("CPLD @0xFEFFFAD8 = 0x%02x (version %d)\n", v1, v1 & 7);
    fflush(stdout);
    Supexec(rd_v2);
    printf("CPLD @0xFAFFFAD8 = 0x%02x (version %d)\n", v2, v2 & 7);
    return 0;
}
