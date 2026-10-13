/* t2 (2nd transputer, behind Slot-1 link 1): loop ticks, own RAM size and the 1/i^2 float sum.
   Built twice: -ta (software float, control) and -t8 (FPU instructions; a T4 halts on them). */
#include <misc.h>
#include <channel.h>
#include "charcommon.h"
int main(void)
{
  Channel *up = (Channel *)get_param(1);
  volatile double x = 0.0; int i, t0, r[4];
  r[0] = looptime(); r[1] = size_self();
  t0 = ProcTime();
  for (i = 1; i <= 100000; i++) x += 1.0 / ((double)i * (double)i);
  r[3] = ProcTimeMinus(ProcTime(), t0); r[2] = (int)(x * 1000000.0);
  ChanOut(up, r, sizeof r);
  return 0;
}
