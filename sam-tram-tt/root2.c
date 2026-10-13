/* root2 (T425): print the 2nd transputer's report (relayed by Slot 1). */
#include <stdio.h>
#include <misc.h>
#include <channel.h>
#include "charcommon.h"
int main(void)
{
  static const char *mb[NSIZE] = { "1", "2", "4", "8", "16", "32" };
  int r[4], my = looptime();
  printf("T425 loop=%d ticks; booting 2nd transputer via Slot-1 link 1\n", my);
  ChanIn((Channel *)get_param(3), r, sizeof r);
  printf("2nd  loop=%d ticks (clock ~ %d MHz vs the 40 MHz T425, estimate)\n", r[0], r[0] ? (40 * my + r[0] / 2) / r[0] : 0);
  if (r[1] >= 100 && r[1] < 100 + NSIZE) printf("2nd  RAM: %s MB (address decode wraps)\n", mb[r[1] - 100]);
  else printf("2nd  RAM code %d\n", r[1]);
  printf("2nd  float sum: %d (expect 1644924) in %d ticks\n", r[2], r[3]);
  exit_terminate(0);
  return 0;
}
