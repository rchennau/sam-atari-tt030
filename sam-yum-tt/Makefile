# sam-yum-tt — build the proof-of-technology probe into build/.  Run from this directory.
#   make host   host check of the SHA-256 core (compare with sha256sum)
#   make tt     68030 client  build/shabench.ttp
#   make t4     T425 server   build/shaserv.btl  (INMOS icc under t4; built in a short /tmp dir, see docs/BUILD.txt)
ROOT  := $(abspath ..)
CROSS := $(ROOT)/tools/cross-mint/usr/bin/m68k-atari-mint-gcc
T4DIR := /tmp/samyum-t4

all: host tt t4

host: build/sha256test
build/sha256test: src/sha256test.c src/sha256.c src/sha256.h
	cc -O2 -Wall -o $@ src/sha256test.c src/sha256.c

tt: build/shabench.ttp
build/shabench.ttp: src/shabench.c src/sha256.c src/sha256.h
	$(CROSS) -m68020-60 -O2 -Wall -o $@ src/shabench.c src/sha256.c

t4: build/shaserv.btl
build/shaserv.btl: src/shaserv.c src/sha256.c src/sha256.h
	rm -rf $(T4DIR) && mkdir -p $(T4DIR) && cp src/shaserv.c src/sha256.[ch] $(T4DIR)/
	printf 'shaserv.t4h\nsha256.t4h\n#include startup.lnk\n' > $(T4DIR)/shaserv.lnk
	cd $(T4DIR) && . $(ROOT)/tools/d72uni-env.sh && export ISEARCH="$$PWD/ $$D72UNI/libs/" && \
	  icc shaserv.c -t4 -o shaserv.t4h && icc sha256.c -t4 -o sha256.t4h && \
	  ilink -f shaserv.lnk -t4 -h -o shaserv.c4h && icollect shaserv.c4h -t -o shaserv.b4h
	cp $(T4DIR)/shaserv.b4h $@

clean:
	rm -f build/sha256test build/shabench.ttp build/shaserv.btl

.PHONY: all host tt t4 clean
