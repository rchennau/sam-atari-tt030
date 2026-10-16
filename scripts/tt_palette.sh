#!/bin/bash
# tt_palette.sh — switch the TT's 256-colour look (PMS tt030-atw-display-tuning FR-3).
#   tt_palette.sh show            which look C:\AUTO\DEFAULT.PAL holds (md5 match: normal|amber|custom|none)
#   tt_palette.sh LOOK            install LOOK (normal|amber) as the boot palette and load it now
# normal removes DEFAULT.PAL (xVDI then uses its built-in palette) and reloads the stock colours live.
# Installs upload DEFAULT.TMP over Modem 2 (tt_zput.sh, SHA-256 verified), check 1536 bytes on the TT, then rename,
# so a broken transfer never becomes the boot palette. Everything runs over serial: it works with an unreadable
# screen. Needs /c/atwpal.ttp on the TT (src/atwbg/atwpal.c). Only 256-colour modes read DEFAULT.PAL.
. "$(dirname "$0")/tt_log.sh"
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
tts() { python3 "$here/tt_serial.py" --timeout 120 "$1"; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
md5of() { python3 "$here/atw_palette.py" "$1" "$tmp/$1.pal" && md5sum < "$tmp/$1.pal" | cut -d' ' -f1; }

case ${1:-show} in
show)
    have=$(tts 'md5sum /c/auto/default.pal 2>/dev/null | cut -d" " -f1' | tr -d '\r\n ')
    look=custom
    [ -z "$have" ] && look=none
    for l in normal amber; do [ "$have" = "$(md5of $l)" ] && look=$l; done
    echo "tt_palette: $look${have:+ ($have)}" ;;
normal)
    python3 "$here/atw_palette.py" normal "$tmp/n.pal"
    bash "$here/tt_zput.sh" "$tmp/n.pal" /c LIVE.PAL
    tts 'rm -f /c/auto/default.pal; /c/atwpal.ttp -l /c/live.pal'
    echo "tt_palette: normal (DEFAULT.PAL removed, stock colours loaded)" ;;
amber)
    python3 "$here/atw_palette.py" amber "$tmp/a.pal"
    [ "$(stat -c %s "$tmp/a.pal")" = 1536 ] || { echo "tt_palette: built palette is not 1536 bytes" >&2; exit 1; }
    bash "$here/tt_zput.sh" "$tmp/a.pal" /c/auto DEFAULT.TMP
    tts '[ $(wc -c < /c/auto/default.tmp) = 1536 ] && mv /c/auto/default.tmp /c/auto/default.pal && /c/atwpal.ttp -l /c/auto/default.pal' \
        || { tts 'rm -f /c/auto/default.tmp' > /dev/null; echo "tt_palette: install failed; DEFAULT.PAL unchanged" >&2; exit 1; }
    echo "tt_palette: amber installed and loaded" ;;
*)  echo "usage: tt_palette.sh show|normal|amber" >&2; exit 1 ;;
esac
