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
# ShallotsOS fixes on top, applied in this order (each patch's context
# assumes the ones before it):
#   gpsp-sram-library-string.patch  games whose SRAM library string is
#       SRAM_F_V (e.g. Hamtaro: Ham-Ham Heartbreak), and SRAM writes of 0xAA
#       at 0x5555 mistaken for a flash command
#   gpsp-hblank-timing.patch  HBlank at 1008 cycles into the line, lasting
#       224 (hardware timing, as in mGBA); Hamtaro stalled after "Yep-P"
#   gpsp-trace-hblank.patch  temporary diagnostic trace, only active when
#       /mnt/sdcard/gpsp_trace.txt exists
set -euo pipefail

[ $# -eq 3 ] || { sed -n '6p' "$0" >&2; exit 2; }
SRC=$(realpath "$1")
SDK=$(realpath "$2")
OUT=$(realpath -m "$3")
PATCH=$(realpath "$(dirname "$0")/../patches/gpsp-upstream-sf3000.patch")
PATCH_DIR=$(realpath "$(dirname "$0")/../patches")

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
for p in gpsp-sram-library-string gpsp-hblank-timing gpsp-trace-hblank; do
    git apply --check "$PATCH_DIR/$p.patch" 2>/dev/null && git apply "$PATCH_DIR/$p.patch"
done
grep -q 'backup_type_sram_known' gba_memory.c || { echo "ERROR: SRAM patch not applied" >&2; exit 1; }
grep -q 'video_count += 224;' main.c || { echo "ERROR: HBlank timing patch not applied" >&2; exit 1; }
grep -q 'shallots_trace_dump' main.c || { echo "ERROR: trace patch not applied" >&2; exit 1; }

make clean >/dev/null 2>&1 || true
make platform=sf3000 CC="$WRAP/gpsp-gcc" CXX="$WRAP/gpsp-g++" \
    AR="${MIPS}ar" RANLIB="${MIPS}ranlib" -j"$(nproc)" || true
rm -f gpsp_libretro.so
make platform=sf3000 CC="$WRAP/gpsp-g++" CXX="$WRAP/gpsp-g++" \
    AR="${MIPS}ar" RANLIB="${MIPS}ranlib" \
    LDFLAGS="$LDFLAGS_C -lstdc++" gpsp_libretro.so
"${MIPS}strip" -o "$OUT" gpsp_libretro.so
file "$OUT"
