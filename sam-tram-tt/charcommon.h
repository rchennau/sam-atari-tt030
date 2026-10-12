/* shared by rootc.c (T425) and tramc.c (Slot-1 TRAM): device id, loop timing, RAM sizing. */
#include <process.h>
#define NSIZE 6                       /* candidate sizes 1,2,4,8,16,32 MB */
static int devid(void) { int id; __asm { ldc 0; lddevid; st id; } return id; }
/* unsigned: transputer signed add is overflow-checked and halts on error. ticks (low priority, 64 us) for a fixed integer loop: same code on both CPUs, so the ratio ~ clock ratio */
static int looptime(void)
{
  volatile unsigned x = 0; int i, t0 = ProcTime();
  for (i = 0; i < 2000000; i++) x += i;
  return ProcTimeMinus(ProcTime(), t0);
}
