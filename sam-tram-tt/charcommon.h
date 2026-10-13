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

static int scratch[4];
/* result: S index found (0..NSIZE-1) as positive code: 100+i = wrapped at size i, 200+i = no RAM at size i,
   300 = RAM beyond the largest size, -1 = link failure */
static int size_self(void)
{
  int i;
  volatile int *b = (volatile int *)scratch;
  for (i = 0; i < NSIZE; i++) {
    volatile int *p = (volatile int *)((char *)scratch + (0x100000 << i));
    int save = *p, got;
    *b = 0x11111111; *p = 0x22222222; got = *p;
    if (*b == 0x22222222) { *b = 0; return 100 + i; }
    *p = save;
    if (got != 0x22222222) return 200 + i;
  }
  return 300;
}
