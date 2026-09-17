# shabench — is the T425 worth using for yum's SHA-256? (tt030-rpm-pipeline, operator option 1, 2026-09-16)
# Host check:  gcc -O2 -o /tmp/t src/shabench/sha256test.c src/shabench/sha256.c; compare with sha256sum
#              (11 files incl. 0/55/56/63/64/65/119/120 bytes, 1 MiB, 3 MiB: all match, 2026-09-17)
# T425 (from a SHORT dir, ilink's ISEARCH limit — see ../x25519bench/BUILD.txt):
#   cp sha256.[ch] shaserv.c /tmp/shab; printf 'shaserv.t4h\nsha256.t4h\n#include startup.lnk\n' > shaserv.lnk
#   . tools/d72uni-env.sh; ISEARCH="$PWD/ $D72UNI/libs/"; icc shaserv.c -t4 -o shaserv.t4h; icc sha256.c ...
#   ilink -f shaserv.lnk -t4 -h -o shaserv.c4h; icollect shaserv.c4h -t -o shaserv.b4h  -> upload as SHASERV.BTL
#   sha256.c checked under t4 ("abc" + 100 KB pattern == hashlib), 2026-09-17.
# TT:   m68k-atari-mint-gcc -m68020-60 -O2 -o shabench.ttp src/shabench/shabench.c src/shabench/sha256.c
#   fpgabios.tos, then:  shabench SHASERV.BTL 512
#   Hatari (68030 only, no ATW): 137 KB/s, digest correct — emulator timing, NOT the real TT.
# Decision rule: offload only if (T425 hash incl. transfer) is clearly faster than the 68030 hash.
