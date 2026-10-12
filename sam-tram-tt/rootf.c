/* rootf (T425, built -ta: software floating point): same sum for reference, then print Slot-1's FPU answer. */
#include <stdio.h>
#include <misc.h>
#include <channel.h>
#include <process.h>
int main(void)
{
  Channel *from = (Channel *)get_param(3);
  volatile double x = 0.0; int i, t0, t, r[3];
  printf("FPU test: sum 1/i^2, i=1..100000 (expect 1644924 = pi^2/6 x 1e6)\n");
  t0 = ProcTime();
  for (i = 1; i <= 100000; i++) x += 1.0 / ((double)i * (double)i);
  t = ProcTimeMinus(ProcTime(), t0);
  printf("T425  (software fp): %d in %d ticks\n", (int)(x * 1000000.0), t);
  ChanIn(from, r, sizeof r);
  printf("Slot1 (tramfpu: -t8 FPU code; tramfps: -ta control): %d in %d ticks\n", r[0], r[2]);
  exit_terminate(0);
  return 0;
}
