#!/bin/bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK_ROOT=${WORK_ROOT:-$(dirname "$ROOT")}
TOOLCHAIN=$WORK_ROOT/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot
PREFIX=$TOOLCHAIN/opt/ext-toolchain/bin/mips-mti-linux-gnu-
SYSROOT=$TOOLCHAIN/mipsel-buildroot-linux-gnu/sysroot
WRAP=$ROOT/.toolchain
CORE=$ROOT/cores/FroggyScummvm/backends/platform/libretro
OUT=$ROOT/build/scummvm_libretro.so

for file in "${PREFIX}gcc" "${PREFIX}g++" "$CORE/Makefile"; do
    [[ -e "$file" ]] || { echo "Missing ScummVM build input: $file" >&2; exit 1; }
done

mkdir -p "$WRAP" "$ROOT/build"
FLAGS="-mfp32 -mhard-float -mlong-calls -EL --sysroot=$SYSROOT -idirafter $SYSROOT/usr/include -O3 -DNDEBUG"
cat > "$WRAP/scummvm-gcc" <<EOF
#!/bin/bash
exec ${PREFIX}gcc $FLAGS "\$@"
EOF
cat > "$WRAP/scummvm-g++" <<EOF
#!/bin/bash
exec ${PREFIX}g++ $FLAGS "\$@"
EOF
chmod +x "$WRAP/scummvm-gcc" "$WRAP/scummvm-g++"

make -C "$CORE" platform=gcw0 LITE=1 HAVE_OPENGLES2=0 FORCE_OPENGLNONE=1 \
    CC="$WRAP/scummvm-gcc" CXX="$WRAP/scummvm-g++" \
    AR="${PREFIX}ar cru" RANLIB="${PREFIX}ranlib" LD="$WRAP/scummvm-g++" \
    LDFLAGS="-shared -Wl,--no-undefined -Wl,--gc-sections -Wl,--version-script=$CORE/link.T -mhard-float -mfp32 -EL --sysroot=$SYSROOT -L$SYSROOT/usr/lib -pthread -lm -lc -lstdc++" \
    -j2

cp "$CORE/scummvm_libretro.so" "$OUT"
"${PREFIX}strip" "$OUT"
[[ -s "$OUT" ]] || { echo "ScummVM build produced an empty core" >&2; exit 1; }
echo "Built $OUT"
