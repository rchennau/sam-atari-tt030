#!/usr/bin/env bash
# Set up the transputer cross-toolchain on fractal (FR-5): the INMOS ANSI C toolset that ships in the
# ATW800/2 basepack (d72uni, 1991: icc/ilink/icollect/oc/...) run under the t4 transputer emulator.
# No root. Installs into tools/ (gitignored) and ends with a hello-world self-test.
#
#   scripts/setup_transputer_toolchain.sh        then:  . tools/d72uni-env.sh
#
# Build for the ATW's T425 with the T4 class:
#   icc x.c -t4 -o x.t4h ; ilink -f x.lnk -t4 -h -o x.c4h ; icollect x.c4h -t -o x.b4h ; t4 -se -sb x.b4h
set -euo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd)
T=$REPO/tools
T4_COMMIT=2f1bade            # pahihu/t4 head tested 2026-09-12 (t4 V1.6a)
PACK=$REPO/staging/TRANSPUTER/atw800_2_basepack_20260227.zip
mkdir -p "$T"

# 1. t4 emulator
if [ ! -x "$T/t4/t4" ]; then
  [ -d "$T/t4" ] || git clone -q https://github.com/pahihu/t4.git "$T/t4"
  git -C "$T/t4" checkout -q "$T4_COMMIT"
  make -s -C "$T/t4" >/dev/null 2>&1
fi

# 2. INMOS toolset from the ATW basepack (nested zip)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
unzip -q "$PACK" -d "$WORK"
unzip -q "$WORK"/*/atw800_2.zip -d "$WORK/inner"
rm -rf "$T/d72uni" && mkdir -p "$T/d72uni/bin"
cp -r "$WORK/inner/atw800_2/d72uni/itools" "$WORK/inner/atw800_2/d72uni/libs" "$T/d72uni/"

# 3. environment + wrappers (each tool is a .btl the emulator boots)
cat > "$T/d72uni-env.sh" <<EOF
# INMOS ANSI C toolset (ATW800/2 d72uni) under t4 — source this file.
export D72UNI=$T/d72uni
export PATH=$T/d72uni/bin:$T/t4:\$PATH
export ISEARCH="\$D72UNI/libs/"
export IBOARDSIZE=#200000
EOF
for t in icc ilink icollect ilibr ilist imakef icconf oc occonf; do
  # shellcheck disable=SC2016  # $D72UNI and $@ must stay literal: the wrapper expands them at run time
  printf '#!/bin/sh\nexec t4 -se -sb "$D72UNI/itools/%s.btl" "$@"\n' "$t" > "$T/d72uni/bin/$t"
  chmod +x "$T/d72uni/bin/$t"
done

# 4. self-test: build and run hello world
# shellcheck disable=SC1091
. "$T/d72uni-env.sh"
cd "$WORK"
printf '#include <stdio.h>\nint main() { printf("\\nHello World\\n"); }\n' > hello.c
printf 'hello.t4h\n#include startup.lnk\n' > hello.lnk
icc hello.c -t4 -o hello.t4h >/dev/null
ilink -f hello.lnk -t4 -h -o hello.c4h >/dev/null
icollect hello.c4h -t -o hello.b4h >/dev/null
if t4 -se -sb hello.b4h | grep -q 'Hello World'; then
  echo "transputer toolchain OK — . $T/d72uni-env.sh"
else
  echo "self-test FAILED" >&2; exit 1
fi
