#!/bin/sh
# Builds stockprobe.so with the SF3000 toolchain (the same one picoarch uses).
set -e
cd "$(dirname "$0")"
TC="${TC:-$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot/opt/ext-toolchain/bin/mips-mti-linux-gnu-}"
SYSROOT="${SYSROOT:-$HOME/sf3000-work/sf3000toolchain/mipsel-buildroot-linux-gnu_sdk-buildroot/mipsel-buildroot-linux-gnu/sysroot}"
CC_FLAGS="${CC_FLAGS:--mips32r2 -march=mips32r2 -mtune=74kc -mfp32 -mhard-float -EL -fPIC}"
mkdir -p build
"${TC}gcc" $CC_FLAGS --sysroot="$SYSROOT" -O2 -Wall -fvisibility=hidden -shared \
	-o build/stockprobe.so stockprobe.c -ldl -Wl,-soname,stockprobe.so -s
echo "built build/stockprobe.so"
