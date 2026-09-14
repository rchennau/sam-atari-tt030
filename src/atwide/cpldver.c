/* cpldver — read the ATW800/2's CPLD (Absinth) firmware version: Programmer's Manual memory map,
 * "CPLD version" at 0xFEFFFAD8 on the TT (lower 3 bits). The IDE/SD slot on VME needs Absinth
 * VME firmware v1 (User's Manual). Read-only; kanban 5d91d23b (item A).
 */
#include <mint/osbind.h>
#include <stdio.h>

static unsigned char v;
static long rd(void) { v = *(volatile unsigned char *)0xFEFFFAD8UL; return 0; }

int main(void)
{
    Supexec(rd);
    printf("CPLD version register 0xFEFFFAD8 = 0x%02x, version (low 3 bits) = %d\n", v, v & 7);
    return 0;
}
