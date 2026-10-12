/* rootp (T425): print what the TRAM process reports. */
#include <stdio.h>
#include <misc.h>
#include <channel.h>
int main(void)
{
  static const char *st[4] = { "link to T425", "no-accept (nothing attached)", "accepted, no reply", "transputer answering" };
  Channel *from = (Channel *)get_param(3);
  int res[8], l;
  ChanIn(from, res, sizeof res);
  printf("Slot-1 TRAM booted via icconf and reporting\n");
  for (l = 0; l < 4; l++) {
    printf("TRAM link %d: %s", l, st[res[2*l] & 3]);
    if (res[2*l] == 3) printf(" peek 80000000=%08x", res[2*l+1]);
    printf("\n");
  }
  exit_terminate(0);
  return 0;
}
