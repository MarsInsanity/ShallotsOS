#!/bin/bash
# Build the USB serial (CDC ACM) gadget modules for the stock 4.4.186-release
# kernel: u_serial.ko and usb_f_acm.ko, for ShallotsOS's USB debug shell.
#
#   scripts/build_usb_serial_modules.sh <toolchain bin dir> <output dir>
#
# Same recipe as TreeFrogUI's usb_f_mass_storage.ko (apps/usb_mode/README.md):
# pristine kernel.org 4.4.186, rt305x_defconfig and the ABI settings the LGPT
# R36SX OTG port recovered from the stock kernel. The stock kernel has no
# MODVERSIONS, so vermagic is the only load-time check: it must match the
# mass-storage module's exactly.
set -euo pipefail

[ $# -eq 2 ] || { sed -n '5p' "$0" >&2; exit 2; }
TC_BIN=$(realpath "$1")
OUT=$(realpath -m "$2")
KVER=4.4.186
REF_KO="$(dirname "$0")/../apps/usb_mode/modules/$KVER-release/usb_f_mass_storage.ko"
REF_KO=$(realpath "$REF_KO")
WORK=${WORK_DIR:-$(mktemp -d)}
CROSS=mips-mti-linux-gnu-
export PATH="$TC_BIN:$PATH"
command -v "${CROSS}gcc" >/dev/null || { echo "ERROR: no ${CROSS}gcc in $TC_BIN" >&2; exit 1; }

cd "$WORK"
if [ ! -d "linux-$KVER" ]; then
    [ -f "linux-$KVER.tar.xz" ] || curl -fsSL --retry 3 -o "linux-$KVER.tar.xz" \
        "https://cdn.kernel.org/pub/linux/kernel/v4.x/linux-$KVER.tar.xz"
    tar -xJf "linux-$KVER.tar.xz"
fi
cd "linux-$KVER"

# Old kernel host tools on a current host compiler: dtc's duplicate yylloc.
KMAKE=(make ARCH=mips CROSS_COMPILE="$CROSS" HOSTCFLAGS=-fcommon -j"$(nproc)")
"${KMAKE[@]}" rt305x_defconfig >/dev/null
scripts/config \
    --set-str LOCALVERSION "-release" \
    -d LOCALVERSION_AUTO \
    -e PREEMPT -d PREEMPT_NONE -d PREEMPT_VOLUNTARY \
    -d DEBUG_PREEMPT -d SMP -d MODVERSIONS -d MODULE_UNLOAD \
    -e USB_SUPPORT -e USB_GADGET \
    -m USB_LIBCOMPOSITE -m USB_CONFIGFS \
    -e USB_CONFIGFS_MASS_STORAGE -m USB_F_MASS_STORAGE \
    -e USB_CONFIGFS_ACM -m USB_U_SERIAL -m USB_F_ACM
"${KMAKE[@]}" olddefconfig >/dev/null
for opt in USB_CONFIGFS_ACM=y USB_U_SERIAL=m USB_F_ACM=m PREEMPT=y SMP MODVERSIONS MODULE_UNLOAD; do
    case "$opt" in
        *=*) grep -qx "CONFIG_$opt" .config || { echo "ERROR: .config lacks CONFIG_$opt" >&2; exit 1; } ;;
        *) ! grep -q "^CONFIG_$opt=" .config || { echo "ERROR: CONFIG_$opt must be off" >&2; exit 1; } ;;
    esac
done
"${KMAKE[@]}" modules_prepare >/dev/null
"${KMAKE[@]}" M=drivers/usb/gadget/function modules

# .modinfo fields, read without the host's modinfo (a MIPS object).
mod_field() { strings -a "$2" | awk -v k="$1=" 'index($0, k) == 1 && !d { print substr($0, length(k) + 1); d = 1 }'; }
mkdir -p "$OUT"
want=$(mod_field vermagic "$REF_KO")
for ko in u_serial usb_f_acm; do
    f="drivers/usb/gadget/function/$ko.ko"
    "${CROSS}strip" --strip-debug -o "$OUT/$ko.ko" "$f"
    got=$(mod_field vermagic "$OUT/$ko.ko")
    [ "$got" = "$want" ] || { echo "ERROR: $ko vermagic '$got' != stock '$want'" >&2; exit 1; }
    echo "$ko.ko: depends=$(mod_field depends "$OUT/$ko.ko") vermagic=$got"
done
sha256sum "$OUT"/*.ko
