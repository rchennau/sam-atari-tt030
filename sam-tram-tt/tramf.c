/* tramf (Slot-1 TRAM, built -t8 so float maths uses FPU instructions): a T805 computes the right answer
   fast; a T4 has no FPU, so the same code halts or returns garbage. Reports {result bits, ticks}. */
#include <misc.h>
#include <channel.h>
#include <process.h>
int main(void)
{
  Channel *up = (Channel *)get_param(1);
  volatile double x = 0.0; int i, t0 = ProcTime(), r[3];
  for (i = 1; i <= 100000; i++) x += 1.0 / ((double)i * (double)i);   /* -> pi^2/6 = 1.644924... */
  r[2] = ProcTimeMinus(ProcTime(), t0);
  r[0] = (int)(x * 1000000.0); r[1] = 0;
  ChanOut(up, r, sizeof r);
  return 0;
}
