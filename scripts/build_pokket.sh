#!/bin/bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK_ROOT=${WORK_ROOT:-$(dirname "$ROOT")}
TOOLCHAIN=$WORK_ROOT/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot
PREFIX=$TOOLCHAIN/opt/ext-toolchain/bin/mips-mti-linux-gnu-
SYSROOT=$TOOLCHAIN/mipsel-buildroot-linux-gnu/sysroot
POKKET_REV=37c022ce46eb578de5b57d7b488ffda9e3b7d9f1
LIBRETRO_COMMON_REV=88252880232fb8488296dc0cd4c57d6fb8159b3d
FLAGS="-mips32r2 -march=mips32r2 -mtune=74kc -mfp32 -mhard-float -mlong-calls -EL --sysroot=$SYSROOT -fPIC -O3 -DNDEBUG"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/pokket.XXXXXX")
CORE=$WORK/src
LIBRETRO_COMMON=$WORK/libretro-common
trap 'rm -rf "$WORK"' EXIT

git init -q "$CORE"
git -C "$CORE" fetch --depth=1 https://github.com/mentalfoundry/Pokket.git "$POKKET_REV"
git -C "$CORE" checkout --quiet --detach FETCH_HEAD
[[ "$(git -C "$CORE" rev-parse HEAD)" == "$POKKET_REV" ]] || {
    echo "Pokket upstream changed; review and update POKKET_REV" >&2
    exit 1
}

git init -q "$LIBRETRO_COMMON"
git -C "$LIBRETRO_COMMON" fetch --depth=1 https://github.com/libretro/libretro-common.git "$LIBRETRO_COMMON_REV"
git -C "$LIBRETRO_COMMON" checkout --quiet --detach FETCH_HEAD
[[ "$(git -C "$LIBRETRO_COMMON" rev-parse HEAD)" == "$LIBRETRO_COMMON_REV" ]] || {
    echo "libretro-common revision mismatch" >&2
    exit 1
}

cmake -S "$CORE" -B "$WORK/build" -G "Unix Makefiles" \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_C_COMPILER="${PREFIX}gcc" \
    -DCMAKE_C_FLAGS="$FLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="-EL --sysroot=$SYSROOT" \
    -DCMAKE_SHARED_LINKER_FLAGS="-EL --sysroot=$SYSROOT" \
    -DCMAKE_MODULE_LINKER_FLAGS="-EL --sysroot=$SYSROOT" \
    -DFETCHCONTENT_SOURCE_DIR_LIBRETRO-COMMON="$LIBRETRO_COMMON" \
    -DPSEMU_BUILD_LIBRETRO=ON \
    -DPSEMU_BUILD_DESKTOP=OFF \
    -DPSEMU_BUILD_TESTS=OFF
cmake --build "$WORK/build" --target pokket_libretro --parallel "$(nproc)"

mkdir -p "$ROOT/build"
cp "$WORK/build/pokket_libretro.so" "$ROOT/build/pokket_libretro.so"
cp "$CORE/LICENSE" "$ROOT/build/Pokket.LICENSE"
cp "$CORE/core/LICENSE" "$ROOT/build/Pokket-core.MIT"
"${PREFIX}strip" "$ROOT/build/pokket_libretro.so"
