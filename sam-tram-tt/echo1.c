/* echo1 (Slot 1): echo dest-1 messages; forward dest-2 messages to the 2nd transputer and relay its reply. */
#include <misc.h>
#include <channel.h>
static char buf[16384];
int main(void)
{
  Channel *fr = (Channel *)get_param(1), *tr = (Channel *)get_param(2);
  Channel *t2 = (Channel *)get_param(3), *f2 = (Channel *)get_param(4);
  int hdr[2];
  for (;;) {
    ChanIn(fr, hdr, 8);
    if (hdr[0] == 0) { ChanOut(t2, hdr, 8); return 0; }
    ChanIn(fr, buf, hdr[0]);
    if (hdr[1] == 2) { ChanOut(t2, hdr, 8); ChanOut(t2, buf, hdr[0]); ChanIn(f2, buf, hdr[0]); }
    ChanOut(tr, buf, hdr[0]);
  }
}
