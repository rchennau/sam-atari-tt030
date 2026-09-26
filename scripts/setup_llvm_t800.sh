#!/usr/bin/env bash
# Set up the llvm-t800 cross toolchain (clang + lld for the T425) on fractal, no root — PMS
# tt030-llvm-t800-adoption FR-1. Extracts the pinned .debs into tools/llvm-t800 (gitignored); the drivers
# find their prefix from their own path, so /opt is not needed.
#
#   scripts/setup_llvm_t800.sh [DEB_DIR]     then:  export PATH=$PWD/tools/llvm-t800/bin:$PATH
#   scripts/setup_llvm_t800.sh --check       NFR-5 scan only: no INMOS material tracked or staged for publishing
#
# The -inmos add-on (INMOS copyright, "not for redistribution") is extracted only if present, into
# tools/llvm-t800/inmos, and nothing built for shipping may link it (plan C5).
set -euo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd)
DEST=$REPO/tools/llvm-t800
VER=0.2.0+20260926.g0d979ca.l973377d
declare -A PIN=(
  [llvm-t800_${VER}_amd64.deb]=8fa00cac30d667bdb51826cfbc6a82139c9020dd5b70bf68d1c417cd7b8e5c47
  [llvm-t800-extras_${VER}_all.deb]=776e975cf1d924754b15f40829d774d98d35a1225d56905bb236216114642503
  [llvm-t800-inmos_${VER}_all.deb]=447494272aa3db775cbebe5d0ef7ef0541fe1c9e516fd7c32b5828596c3afe24
)

# NFR-5: INMOS notices or D7205 files in anything tracked or in the published staging trees
check() {
  local hits
  hits=$(cd "$REPO" && { git ls-files -z | xargs -0 grep -lIE 'Copyright \(C\) (Inmos|INMOS)|INMOS Limited' 2>/dev/null
                         git ls-files | grep -iE '(^|/)(d7205|ictools|llvm-t800-inmos)' ; } | grep -v '^scripts/setup_llvm_t800.sh$' || true)
  if [ -n "$hits" ]; then echo "NFR-5 FAIL — INMOS material tracked:"; echo "$hits"; return 1; fi
  echo "NFR-5 OK — no INMOS material tracked"
}
if [ "${1:-}" = --check ]; then check; exit; fi

DEBS=${1:-$REPO/staging/TRANSPUTER/llvm-t800}
rm -rf "$DEST.new" && mkdir -p "$DEST.new"
for f in "${!PIN[@]}"; do
  d=$DEBS/$f
  if [ ! -f "$d" ]; then
    case $f in *-inmos_*) echo "skip $f (optional, not present)"; continue ;; esac
    echo "missing $d" >&2; exit 1
  fi
  echo "${PIN[$f]}  $d" | sha256sum -c --quiet - || { echo "SHA-256 mismatch: $d" >&2; exit 1; }
  dpkg-deb -x "$d" "$DEST.new/root"
done
rm -rf "$DEST" && mv "$DEST.new/root/opt/llvm-t800" "$DEST" && rm -rf "$DEST.new"
echo "$VER" > "$DEST/VERSION.sam"

# self-test under t4 (factory-jumper T425, as the card is set)
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
printf '#include <stdio.h>\nint main(void){ long long x = 0x123456789LL; printf("hello %%lld\\n", x*3); return 0; }\n' > "$W/h.c"
PATH=$DEST/bin:$PATH t800-cc -mcpu=t425 -mboard=atw800 -mmem=5M -Os "$W/h.c" -o "$W/h" >/dev/null
out=$(PATH=$DEST/bin:$PATH timeout 60 t4 -s4 -sm 23 -sr -ss -sb "$W/h.btl" 2>&1 || true)
if printf '%s' "$out" | grep -q 'hello 14660155035'; then
  echo "llvm-t800 $VER OK — export PATH=$DEST/bin:\$PATH"
else
  echo "self-test FAILED:"; printf '%s\n' "$out"; exit 1
fi
check
