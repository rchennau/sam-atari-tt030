/* atwres — print the live screen mode as "W H PLANES" from the Line-A variables (V_X_MAX, V_Y_MAX, VPLANES), so a
 * remote check can confirm which XVDI mode booted (PMS tt030-atw800-video-modes FR-3 check 2). Read-only, in Supexec. */
#include <mint/osbind.h>
#include <mint/linea.h>
#include <stdio.h>

static short w, h, planes;

static long rd(void)
{
    w = V_X_MAX; h = V_Y_MAX; planes = VPLANES;
    return 0;
}

int main(void)
{
    linea0();
    Supexec(rd);
    printf("%d %d %d\n", w, h, planes);
    return 0;
}
