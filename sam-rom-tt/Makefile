# sam-rom-tt — custom TT030 firmware (alpha skeleton).  make rom | hatari | clean
ROOT  := $(abspath ..)
CC    := $(ROOT)/tools/cross-mint/usr/bin/m68k-atari-mint-gcc
OC    := $(ROOT)/tools/cross-mint/usr/bin/m68k-atari-mint-objcopy
SIZE  := 524288        # the TT decodes 0xE00000-0xE7FFFF: 4 x 128 KB chips, byte-interleaved

rom: build/rom0.img
build/rom0.img: src/rom0.S
	$(CC) -nostdlib -Wl,-Ttext=0xE00000 -o build/rom0.elf $<
	$(OC) -O binary build/rom0.elf build/rom0.raw
	python3 -c "import sys; d=open('build/rom0.raw','rb').read(); \
	  assert len(d) <= $(SIZE), len(d); open('$@','wb').write(d + b'\xff' * ($(SIZE) - len(d)))"

# Split a 512 KB image into the four byte lanes (chip N = bytes N, N+4, ...) and back.
split: build/rom0.img
	python3 tools/interleave.py split build/rom0.img build/rom0
join:
	python3 tools/interleave.py join build/rom0 build/rejoined.img

hatari: build/rom0.img
	xvfb-run -a flatpak run --user org.tuxfamily.hatari --tos build/rom0.img --machine tt --sound off --run-vbls 400

clean:
	rm -f build/rom0.elf build/rom0.raw build/rom0.img build/rom0.chip? build/rejoined.img

.PHONY: rom split join hatari clean
