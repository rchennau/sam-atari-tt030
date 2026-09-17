#!/bin/bash
# FR-8 headless boot capture. usage: boot.sh <name> <rom> [hatari args...]
# Screenshots at fixed wall-clock seconds (fast-forward off => ~real time).
W=$(dirname "$(readlink -f "$0")")/../emulator/fr8; mkdir -p "$W/shots"; name=$1 rom=$2; shift 2
out=$W/shots/$name; rm -rf "$out"; mkdir -p "$out"
exec 3<> <(:); Xvfb -displayfd 3 -screen 0 1280x1024x24 2>/dev/null & X=$!
read -r -u 3 dn; export DISPLAY=:$dn
flatpak run --user org.tuxfamily.hatari --configfile "$W/../hatari.cfg" --tos "$rom" \
  --machine tt --cpulevel 3 --fpu 68882 --memsize 4 --ttram 64 --fast-forward off \
  --sound off --statusbar off --log-file "$out/hatari.log" "$@" >"$out/stdout.log" 2>&1 & H=$!
for t in $(seq -w 2 2 150); do sleep 2; scrot -o "$out/t$t.png"; done
kill $H; sleep 1; kill $X
