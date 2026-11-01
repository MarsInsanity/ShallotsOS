#!/bin/bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK_ROOT=${WORK_ROOT:-$(dirname "$ROOT")}
SOURCE=${FROGGYREADER_ROOT:-$WORK_ROOT/FroggyReader}
TOOLCHAIN=$WORK_ROOT/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot
PREFIX=$TOOLCHAIN/opt/ext-toolchain/bin/mips-mti-linux-gnu-
SYSROOT=$TOOLCHAIN/opt/ext-toolchain/sysroot
OUT=$ROOT/build/froggyreader_libretro.so

[[ -f "$SOURCE/src/core.c" ]] || { echo "Missing FroggyReader source: $SOURCE" >&2; exit 1; }
mkdir -p "$ROOT/build"
"${PREFIX}gcc" --sysroot="$SYSROOT" -EL -march=mips32r2 -mtune=mips32r2 \
  -mhard-float -mfp32 -O2 -fPIC -fno-strict-aliasing -std=gnu11 -D_GNU_SOURCE \
  -D_FILE_OFFSET_BITS=64 -DMINIZ_NO_ARCHIVE_WRITING_APIS -DMINIZ_NO_TIME \
  -I"$SOURCE/include" -I"$SOURCE/src" -I"$SOURCE/third_party" \
  -shared -Wl,--no-undefined -Wl,-soname,froggyreader_libretro.so \
  "$SOURCE/src/util.c" "$SOURCE/src/xml.c" "$SOURCE/src/epub.c" \
  "$SOURCE/src/css.c" "$SOURCE/src/fonts.c" "$SOURCE/src/fonts_data.c" \
  "$SOURCE/src/text.c" "$SOURCE/src/image.c" "$SOURCE/src/layout.c" \
  "$SOURCE/src/gfx.c" "$SOURCE/src/app.c" "$SOURCE/src/lang.c" \
  "$SOURCE/src/fonts_ui_data.c" "$SOURCE/src/core.c" \
  "$SOURCE/third_party/miniz.c" "$SOURCE/third_party/stb_image_impl.c" \
  -lm -o "$OUT"
"${PREFIX}strip" "$OUT"
file "$OUT"
