/* rootc (T425): print the T425's own figures and the Slot-1 TRAM's report. */
#include <stdio.h>
#include <misc.h>
#include <channel.h>
#include "charcommon.h"

static void size(const char *who, int c)
{
  static const char *mb[NSIZE] = { "1", "2", "4", "8", "16", "32" };
  printf("%s RAM: ", who);
  if (c < 0) printf("link failure\n");
  else if (c >= 300) printf("more than 32 MB?\n");
  else if (c >= 200) printf("no RAM at +%s MB (ends below that)\n", mb[c - 200]);
  else printf("%s MB (address decode wraps)\n", mb[c - 100]);
}

int main(void)
{
  Channel *from = (Channel *)get_param(3);
  int r[4], my = looptime();
  printf("T425  lddevid=%08x loop=%d ticks\n", devid(), my);
  ChanIn(from, r, sizeof r);
  printf("Slot1 lddevid=%08x loop=%d ticks (clock ~ %d MHz if same cycles/iteration as the 40 MHz T425)\n",
         r[0], r[1], r[1] ? (40 * my + r[1] / 2) / r[1] : 0);
  size("Slot1", r[2]);
  size("Next ", r[3]);
  exit_terminate(0);
  return 0;
}
