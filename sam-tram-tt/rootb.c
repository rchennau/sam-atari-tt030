/* rootb (T425): link round-trip benchmark. For each size, N echo round-trips to Slot 1 (1 hop) and to the
   2nd transputer (2 hops, relayed by Slot 1). Times with the low-priority timer (64 us ticks), so many
   iterations per size; reports us per round-trip and payload MB/s (both directions counted). */
#include <stdio.h>
#include <misc.h>
#include <channel.h>
#include <process.h>
static char buf[16384];
int main(void)
{
  Channel *to = (Channel *)get_param(3), *from = (Channel *)get_param(4);
  static const int sizes[6] = { 4, 64, 256, 1024, 4096, 16384 };
  int s, d, i, n, t0, t, hdr[2];
  printf("size   hops  us/round-trip  MB/s(2-way)\n");
  for (s = 0; s < 6; s++)
    for (d = 1; d <= 2; d++) {
      n = sizes[s] <= 256 ? 2000 : sizes[s] <= 4096 ? 400 : 100;
      hdr[0] = sizes[s]; hdr[1] = d;
      t0 = ProcTime();
      for (i = 0; i < n; i++) {
        ChanOut(to, hdr, 8); ChanOut(to, buf, sizes[s]);
        ChanIn(from, buf, sizes[s]);
      }
      t = ProcTimeMinus(ProcTime(), t0);
      printf("%5d  %d     %8d       %d.%02d\n", sizes[s], d, (int)((t * 64.0) / n),
             (int)((2.0 * sizes[s] * n) / (t * 64.0)), (int)((200.0 * sizes[s] * n) / (t * 64.0)) % 100);
    }
  hdr[0] = 0; hdr[1] = 0; ChanOut(to, hdr, 8);
  exit_terminate(0);
  return 0;
}
