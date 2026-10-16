#!/bin/bash
# atw_mode.sh — choose the ATW800/2 boot mode (PMS tt030-atw800-video-modes FR-2).
#   atw_mode.sh show            current mode and its name (C:\AUTO\XVDIMENU.INF word 0 = mode - 1, 0-based)
#   atw_mode.sh set <mode>      write <mode> (1..36, docs/atw-modes.tsv) for the NEXT boot
#   atw_mode.sh restore         put back the backed-up original (mode 15 = 1024x768x16)
# XVDIMENU applies the mode at boot only (Phase 0, 2026-10-15), so reboot after set/restore (tt_reboot.sh --wait-back).
# The first run backs the card's file up to $BAK (md5 must match the Phase 0 one). Writes go over Modem 2
# (tt_zput.sh verifies SHA-256 over the line); reads use ssh. Exit 0 ok · 1 refused or failed.
. "$(dirname "$0")/tt_log.sh"
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
TSV=$here/../docs/atw-modes.tsv
INF=/c/auto/xvdimenu.inf
BAK=${ATW_MODE_BAK:-$HOME/.local/state/tt030-deploy/xvdimenu.inf.orig}
ORIG_MD5=0f552319c52225fb1efe6429603feaa2      # Phase 0 (2026-10-15): word 0 = 14 = mode 15

name() { awk -F'\t' -v m="$1" '$1 == m { print $2 }' "$TSV"; }
fetch() { ssh atari-tt "cat $INF" > "$1"; [ "$(stat -c %s "$1")" = 22 ] || { echo "atw_mode: $INF is not 22 bytes" >&2; exit 1; }; }
mode0() { echo $(( $(od -An -tu2 --endian=big -N2 "$1") + 1 )); }   # word 0 is 0-based

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fetch "$tmp/cur"
if [ ! -f "$BAK" ]; then
    [ "$(md5sum < "$tmp/cur" | cut -d' ' -f1)" = "$ORIG_MD5" ] || { echo "atw_mode: no backup and the card's file is not the original — refusing" >&2; exit 1; }
    mkdir -p "$(dirname "$BAK")"; cp "$tmp/cur" "$BAK"; echo "atw_mode: backed up to $BAK"
fi

case ${1:-show} in
show)    m=$(mode0 "$tmp/cur"); echo "mode $m: $(name "$m")" ;;
set)     m=${2:?usage: atw_mode.sh set <mode>}
         [ -n "$(name "$m")" ] || { echo "atw_mode: no mode $m in $TSV" >&2; exit 1; }
         cp "$tmp/cur" "$tmp/XVDIMENU.INF"
         w=$((m - 1)); printf "$(printf '\\x%02x\\x%02x' $((w >> 8)) $((w & 255)))" | dd of="$tmp/XVDIMENU.INF" conv=notrunc status=none
         bash "$here/tt_zput.sh" "$tmp/XVDIMENU.INF" /c/auto XVDIMENU.INF
         echo "atw_mode: next boot = mode $m: $(name "$m")" ;;
restore) bash "$here/tt_zput.sh" "$BAK" /c/auto XVDIMENU.INF
         echo "atw_mode: next boot = mode $(mode0 "$BAK"): $(name "$(mode0 "$BAK")")" ;;
*)       echo "usage: atw_mode.sh show|set <mode>|restore" >&2; exit 1 ;;
esac
