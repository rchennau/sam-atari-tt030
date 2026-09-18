#!/usr/bin/env python3
"""Split a TT ROM image into four byte lanes, or join four dumps back.

The TT's four ROM chips sit one per byte lane of the 68030's 32-bit bus, so chip N holds bytes
N, N+4, N+8 ...  The lane order is ASSUMED here and must be proven on the real machine: dump the
original chips, `join` them, and compare with a known image (emulator/roms/tos306uk.img). If the
compare fails, the lanes are in another order -- try the permutations before burning anything.

  interleave.py split image.img outprefix   -> outprefix.chip0 .. .chip3
  interleave.py join  outprefix joined.img
"""
import sys

def split(img, prefix):
    d = open(img, 'rb').read()
    if len(d) % 4:
        sys.exit('image size %d is not a multiple of 4' % len(d))
    for n in range(4):
        open('%s.chip%d' % (prefix, n), 'wb').write(d[n::4])
        print('%s.chip%d  %d bytes' % (prefix, n, len(d) // 4))

def join(prefix, out):
    lanes = [open('%s.chip%d' % (prefix, n), 'rb').read() for n in range(4)]
    if len({len(x) for x in lanes}) != 1:
        sys.exit('dumps differ in size: %s' % [len(x) for x in lanes])
    d = bytearray(len(lanes[0]) * 4)
    for n, lane in enumerate(lanes):
        d[n::4] = lane
    open(out, 'wb').write(d)
    print('%s  %d bytes' % (out, len(d)))

if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    (split if sys.argv[1] == 'split' else join)(sys.argv[2], sys.argv[3])
