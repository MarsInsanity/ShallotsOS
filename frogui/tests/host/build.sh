#!/bin/sh
# Build the FrogUI core and render harness for the host (x86/ARM Linux) to
# check layouts without a device. Needs gcc, g++ and FreeType headers.
#   tests/host/build.sh [OUT_DIR]   -> OUT_DIR/render_harness
set -eu
cd "$(dirname "$0")/../.."
OUT=${1:-out/host}
mkdir -p "$OUT"
INC="-I. -Icommon $(pkg-config --cflags freetype2) -Iexternal/harfbuzz/src \
     -Iexternal/SheenBidi -Iexternal/SheenBidi/Headers -Iexternal/SheenBidi/Source"
CFLAGS="${CFLAGS:--O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer} -Wall -D__LIBRETRO__"
if [ ! -f "$OUT/harfbuzz.o" ]; then
    g++ $CFLAGS $INC -fno-exceptions -fno-rtti -DHB_MINI -DHB_LEAN -DHB_NO_CDECL \
        -DHB_NO_SETLOCALE -DHB_NO_OT_FONT -DHB_NO_FALLBACK_SHAPE -DHAVE_FREETYPE \
        -c external/harfbuzz/src/harfbuzz.cc -o "$OUT/harfbuzz.o"
fi
if [ ! -f "$OUT/sheenbidi.o" ]; then
    gcc $CFLAGS $INC -DSB_CONFIG_UNITY -w -c external/SheenBidi/Source/SheenBidi.c \
        -o "$OUT/sheenbidi.o"
fi
SRCS="frogui_libretro.c render.c font.c recent_games.c theme.c onion_theme.c gamelist.c
      favorites.c banner.c backlight.c input.c core_override.c ext_filter.c
      common/i18n.c tests/host/render_harness.c"
gcc $CFLAGS $INC $SRCS "$OUT/harfbuzz.o" "$OUT/sheenbidi.o" \
    $(pkg-config --libs freetype2) -lstdc++ -lm -Wl,--wrap=open -o "$OUT/render_harness"
echo "$OUT/render_harness"
