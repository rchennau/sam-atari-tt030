#!/usr/bin/env bash
# Build the FreeMiNT userland disk for BlueSCSI SCSI ID 1 (FR-7): an Atari root sector with one
# partition of ID RAW holding ext2 populated from the SpareMiNT bootstrap rootfs. No root needed.
#
#   scripts/build_hd10_ext2.sh OUT.hda [SIZE_MIB]      (default 1024)
#
# Why each choice (all measured in Hatari, 2026-09-12):
#   - partition ID RAW: CBHD 5.02 maps RAW to MiNT through XHDI; LNX and BGM were not mapped.
#   - ext2 rev 1, no optional features, 1 KiB blocks, 128-byte inodes: ext2.xfs 0.63 rejects
#     unsupported features/block sizes/inode sizes.
#   - fakeroot: files in the image are owned by root (uid 0), not by sam.
#   - e2fsck -fy + tune2fs -c 0 -i 0: otherwise MiNT warns "mounting unchecked fs".
set -euo pipefail
OUT=${1:?usage: build_hd10_ext2.sh OUT.hda [SIZE_MIB]}
SIZE_MIB=${2:-1024}
REPO=$(cd "$(dirname "$0")/.." && pwd)
TGZ=$REPO/staging/SPAREMINT/bootstrap/rootfs+rpm-3.06.tgz
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

BLOCKS=$(( (SIZE_MIB * 1024) - 1 ))          # 1 KiB blocks; leave the first KiB for the root sector
RPMS=$REPO/staging/SPAREMINT
# 7z decompresses an RPM's payload to a plain SVR4 cpio archive (no gunzip step). Unpacking the
# base set here gives MiNT a working bash/ls without running rpm on the TT; the rpm database is
# not updated, so `rpm -ivh --justdb` them on the TT later if package tracking matters.
mkdir -p "$WORK/cpio"
for r in "$RPMS"/*.m68kmint.rpm; do
  7z e -y -o"$WORK/cpio" "$r" >/dev/null
done
# One fakeroot session for extraction AND mke2fs, so every file in the image is owned by root.
fakeroot sh -c "
  mkdir -p '$WORK/root' && tar xzf '$TGZ' -C '$WORK/root'
  for c in '$WORK'/cpio/*.cpio; do (cd '$WORK/root' && cpio -idmu --quiet --no-absolute-filenames < \"\$c\" 2>/dev/null); done
  python3 '$REPO/scripts/cpio_symlinks.py' '$WORK/root' '$WORK'/cpio/*.cpio
  mkdir -p '$WORK/root/tmp' && chmod 1777 '$WORK/root/tmp'
  mke2fs -q -t ext2 -r 1 -O none -I 128 -b 1024 -L TTROOT -d '$WORK/root' '$WORK/part' $BLOCKS
"
e2fsck -fy "$WORK/part" >/dev/null || [ $? -le 1 ]     # 1 = errors corrected, which is fine here
tune2fs -c 0 -i 0 "$WORK/part" >/dev/null

python3 - "$WORK/part" "$OUT" <<'EOF'
import struct, sys
part = open(sys.argv[1], "rb").read()
nsec = len(part) // 512
root = bytearray(512)
struct.pack_into(">I", root, 0x1C2, 2 + nsec)                    # hd_siz, 512-byte sectors
struct.pack_into(">B3sII", root, 0x1C6, 0x01, b"RAW", 2, nsec)   # exists, RAW, start 2
if sum(struct.unpack(">256H", bytes(root))) & 0xFFFF == 0x1234:  # never boot from this disk
    root[0x1FE] ^= 1
with open(sys.argv[2], "wb") as f:
    f.write(root); f.write(bytes(512)); f.write(part)
print(f"{sys.argv[2]}: {2 + nsec} sectors, RAW ext2 partition at sector 2 ({nsec} sectors)")
EOF
