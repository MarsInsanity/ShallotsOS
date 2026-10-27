#!/bin/sh
# Mount a USB mass-storage disk on the console and launch FrogUI against it.
# This is the host/OTG counterpart to usb_mtp.sh.  The internal SD remains
# mounted (the executable and FrogUI core live there); only the external disk
# is mounted at USB_MOUNTPOINT.

set -u

SYS_ROOT=${TF_USB_SYS_ROOT:-/sys}
ROLE_PATH=${TF_USB_ROLE_PATH:-$SYS_ROOT/devices/platform/soc/18844000.usb/musb-hdrc.0.auto/mode}
MOUNTPOINT=${TF_USB_HOST_MOUNTPOINT:-/mnt/usb}
MODULE_DIR=${TF_USB_MODULE_DIR:-/mnt/sdcard/cubegm/modules/$(uname -r)}
BUILTIN_MODULES=${TF_USB_BUILTIN_MODULES:-$MODULE_DIR/modules.builtin}
PICOARCH=${TF_USB_PICOARCH:-/mnt/sdcard/cubegm/picoarch}
FROGUI_CORE=${TF_USB_FROGUI_CORE:-/mnt/sdcard/cubegm/cores/frogui_libretro.so}
LOG=${TF_USB_HOST_LOG:-/mnt/sdcard/USB_HOST_ERROR.log}
DEVICE=${TF_USB_HOST_DEVICE:-}
mounted=0
original_role=

log() { printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S' 2>/dev/null || echo time-unknown)" "$*" >> "$LOG"; }
fail() { log "ERROR $*"; printf 'USB host error: %s\n' "$*" >&2; exit 1; }

find_role() {
    [ -e "$ROLE_PATH" ] && return 0
    ROLE_PATH=$(find "$SYS_ROOT/devices" -path '*musb-hdrc.0.auto/mode' -print -quit 2>/dev/null || true)
    [ -n "$ROLE_PATH" ] && [ -e "$ROLE_PATH" ]
}

load_module() {
    name=$1
    modprobe "$name" >>"$LOG" 2>&1 || true
    grep -q "^${name} " /proc/modules 2>/dev/null && return 0
    # Stock SF3000 firmware has the host/storage stack built into the kernel;
    # modules.builtin is the authoritative indication in that case.
    case "$name" in
        usb_storage) builtin_path='drivers/usb/storage/usb-storage.ko' ;;
        scsi_mod) builtin_path='drivers/scsi/scsi_mod.ko' ;;
        sd_mod) builtin_path='drivers/scsi/sd_mod.ko' ;;
        fat) builtin_path='fs/fat/fat.ko' ;;
        vfat) builtin_path='fs/fat/vfat.ko' ;;
        nls_iso8859_1) builtin_path='fs/nls/nls_iso8859-1.ko' ;;
        nls_cp437) builtin_path='fs/nls/nls_cp437.ko' ;;
        *) builtin_path="$(printf '%s' "$name" | tr '_' '-')".ko ;;
    esac
    if [ -f "$BUILTIN_MODULES" ] && grep -Fq "$builtin_path" "$BUILTIN_MODULES"; then return 0; fi
    module_file="$MODULE_DIR/$name.ko"
    [ -f "$module_file" ] || module_file="$MODULE_DIR/usb-storage.ko"
    [ -f "$module_file" ] || return 1
    insmod "$module_file" >>"$LOG" 2>&1 || true
    grep -q "^${name} " /proc/modules 2>/dev/null
}

load_host_stack() {
    # Dependency order matters on the stock 4.4 kernel.  musb is normally
    # built/loaded by the board init, but loading it here is harmless when it
    # is available as a module.  The SD/SCSI modules are required by USB MSC.
    [ -f "$BUILTIN_MODULES" ] || BUILTIN_MODULES="/lib/modules/$(uname -r)/modules.builtin"
    load_module scsi_mod || log "scsi_mod not reported loaded (may be built-in)"
    load_module sd_mod || log "sd_mod not reported loaded (may be built-in)"
    load_module usb_storage || load_module usb-storage || log "usb-storage not reported loaded (may be built-in)"
    load_module fat || log "fat not reported loaded (may be built-in)"
    load_module vfat || log "vfat not reported loaded (may be built-in)"
    load_module nls_cp437 || true
    load_module nls_iso8859_1 || true
    return 0
}

discover_device() {
    [ -n "$DEVICE" ] && { [ -b "$DEVICE" ] || return 1; printf '%s\n' "$DEVICE"; return 0; }
    # Ignore the console's mmc devices.  Prefer partitions; whole-disk FAT
    # media is accepted as a fallback for small USB sticks.
    for n in 1 2 3 4; do
        for d in /dev/sd?"$n"; do [ -b "$d" ] && { printf '%s\n' "$d"; return 0; }; done
    done
    for d in /dev/sd?; do [ -b "$d" ] && { printf '%s\n' "$d"; return 0; }; done
    return 1
}

cleanup() {
    rc=$?
    trap - EXIT HUP INT TERM
    if [ "$mounted" -eq 1 ]; then sync; umount "$MOUNTPOINT" >>"$LOG" 2>&1 || rc=1; fi
    if [ -n "$original_role" ] && [ -w "$ROLE_PATH" ]; then
        printf '%s\n' "$original_role" > "$ROLE_PATH" 2>>"$LOG" || rc=1
    fi
    exit "$rc"
}

run_host() {
    [ "$(id -u)" = 0 ] || fail "must run as root"
    mkdir -p "$(dirname "$LOG")" "$MOUNTPOINT" 2>/dev/null || fail "cannot prepare paths"
    : > "$LOG" 2>/dev/null || fail "cannot create log"
    trap cleanup EXIT HUP INT TERM
    find_role || fail "MUSB role control not found"
    original_role=$(cat "$ROLE_PATH" 2>/dev/null || true)
    load_host_stack || fail "USB storage/filesystem modules unavailable"
    if ! printf 'host\n' > "$ROLE_PATH" 2>>"$LOG"; then
        printf 'a_host\n' > "$ROLE_PATH" 2>>"$LOG" || fail "cannot switch USB to host role"
    fi
    log "HOST_ROLE_READY role=$(cat "$ROLE_PATH" 2>/dev/null || echo unknown)"
    n=0
    while [ "$n" -lt 30 ]; do
        DEVICE=$(discover_device 2>/dev/null || true)
        [ -n "$DEVICE" ] && break
        sleep 1; n=$((n + 1))
    done
    [ -n "$DEVICE" ] || fail "no USB storage device detected"
    log "DEVICE $DEVICE"
    mount -t vfat -o rw,utf8,flush "$DEVICE" "$MOUNTPOINT" >>"$LOG" 2>&1 ||
        fail "could not mount $DEVICE (FAT32 is currently supported)"
    mounted=1
    log "READY mount=$MOUNTPOINT device=$DEVICE"
    [ -x "$PICOARCH" ] || fail "picoarch missing: $PICOARCH"
    [ -f "$FROGUI_CORE" ] || fail "FrogUI core missing: $FROGUI_CORE"
    export FROGUI_ROMS_PATH=${TF_USB_HOST_ROMS_PATH:-$MOUNTPOINT/roms}
    [ -d "$FROGUI_ROMS_PATH" ] || mkdir -p "$FROGUI_ROMS_PATH" 2>>"$LOG" || fail "cannot create $FROGUI_ROMS_PATH"
    printf 'USB storage mounted. FrogUI is browsing %s. Press B to exit.\n' "$FROGUI_ROMS_PATH"
    cd / || fail "cannot change working directory"
    "$PICOARCH" "$FROGUI_CORE" "$FROGUI_CORE" >>"$LOG" 2>&1
}

case "${1:-run}" in run|start) run_host;; *) echo "Usage: $0 {run|start}" >&2; exit 2;; esac
