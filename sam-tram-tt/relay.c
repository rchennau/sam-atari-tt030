/* relay (Slot-1 TRAM): pass the 2nd transputer's 4-word report up to the T425. */
#include <misc.h>
#include <channel.h>
int main(void)
{
  int r[4];
  ChanIn((Channel *)get_param(1), r, sizeof r);
  ChanOut((Channel *)get_param(2), r, sizeof r);
  return 0;
}
