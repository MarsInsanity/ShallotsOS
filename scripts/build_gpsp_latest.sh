#!/bin/bash
# Build current upstream libretro/gpsp as gpsp_latest_libretro.so: a separate
# GBA emulator choice next to TreeFrogUI's gpsp_libretro.so (pinned upstream
# build), so a newer upstream can be compared without replacing it.
#
#   scripts/build_gpsp_latest.sh <gpsp source> <SF3000 SDK> <output .so>
#
# Same recipe as build_all.sh's upstream gpsp: patches/gpsp-upstream-sf3000.patch
# (platform=sf3000: MIPS dynarec, mmap'd JIT cache, PIC), C sources compiled
# with gcc, then relinked with g++ because video.cc is C++.
set -euo pipefail

[ $# -eq 3 ] || { sed -n '6p' "$0" >&2; exit 2; }
SRC=$(realpath "$1")
SDK=$(realpath "$2")
OUT=$(realpath -m "$3")
PATCH=$(realpath "$(dirname "$0")/../patches/gpsp-upstream-sf3000.patch")

MIPS="$SDK/opt/ext-toolchain/bin/mips-mti-linux-gnu-"
SYSROOT="$SDK/mipsel-buildroot-linux-gnu/sysroot"
WRAP=$(mktemp -d)
trap 'rm -rf "$WRAP"' EXIT
for t in gcc g++; do
    printf '#!/bin/sh\nexec %s%s --sysroot=%s -isystem %s/usr/include -fPIC -EL "$@"\n' \
        "$MIPS" "$t" "$SYSROOT" "$SYSROOT" > "$WRAP/gpsp-$t"
    chmod +x "$WRAP/gpsp-$t"
done
LDFLAGS_C="-mips32r2 -mhard-float -mfp32 -EL --sysroot=$SYSROOT -L$SYSROOT/usr/lib -lm -lc"

cd "$SRC"
git apply --check "$PATCH" 2>/dev/null && git apply "$PATCH"
grep -q 'platform), sf3000' Makefile || { echo "ERROR: SF3000 patch not applied" >&2; exit 1; }

make clean >/dev/null 2>&1 || true
make platform=sf3000 CC="$WRAP/gpsp-gcc" CXX="$WRAP/gpsp-g++" \
    AR="${MIPS}ar" RANLIB="${MIPS}ranlib" -j"$(nproc)" || true
rm -f gpsp_libretro.so
make platform=sf3000 CC="$WRAP/gpsp-g++" CXX="$WRAP/gpsp-g++" \
    AR="${MIPS}ar" RANLIB="${MIPS}ranlib" \
    LDFLAGS="$LDFLAGS_C -lstdc++" gpsp_libretro.so
"${MIPS}strip" -o "$OUT" gpsp_libretro.so
file "$OUT"
