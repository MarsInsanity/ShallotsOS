#!/bin/sh
# ShallotsOS USB debug shell: the console appears on a PC as a USB serial
# port (CDC ACM; a COM port on Windows 10/11, /dev/ttyACM0 on Linux) with a
# root shell on it, while menus and games keep running.
#
#   usb_debug.sh start | stop | status
#
# Off unless Settings > USB debug shell is on (usb_debug=on in
# frogui/settings.txt). It holds the USB port: USB mode (MTP / storage) stops
# it first, and it comes back with the next boot. Runs from a /tmp copy so it
# never keeps the SD card busy.

SD=${TF_USB_DEBUG_SD:-/mnt/sdcard}
SYS_ROOT=/sys
CONFIG_ROOT=$SYS_ROOT/kernel/config
GADGET=$CONFIG_ROOT/usb_gadget/shallots_debug
UDC_NAME=${TF_USB_UDC_NAME:-musb-hdrc.0.auto}
ROLE_PATH=$SYS_ROOT/devices/platform/soc/18844000.usb/musb-hdrc.0.auto/mode
MODULE_DIR=$SD/cubegm/modules/$(uname -r)
TMP_SELF=/tmp/shallots_usb_debug.sh
PIDFILE=/tmp/shallots_usb_debug.pid
RUN_FLAG=/tmp/shallots_usb_debug.run
SHELL_WRAP=/tmp/shallots_debug_shell
LOG=$SD/cubegm/logs/usb_debug.log
TTY=/dev/ttyGS0

log() {
    mkdir -p "${LOG%/*}" 2>/dev/null
    echo "$(date '+%Y-%m-%d %H:%M:%S' 2>/dev/null) $*" >> "$LOG" 2>/dev/null
}

# Run from /tmp: USB storage mode unmounts the SD card.
if [ "$0" != "$TMP_SELF" ] && [ "${1:-}" = start ]; then
    cp "$0" "$TMP_SELF" 2>/dev/null && chmod 755 "$TMP_SELF" && exec "$TMP_SELF" "$@"
fi

find_role_path() {
    [ -e "$ROLE_PATH" ] && return 0
    ROLE_PATH=$(find "$SYS_ROOT/devices" -path '*musb-hdrc.0.auto/mode' 2>/dev/null | head -n 1)
    [ -n "$ROLE_PATH" ] && [ -e "$ROLE_PATH" ]
}

load_module() {
    grep -q "^$1 " /proc/modules 2>/dev/null && return 0
    modprobe "$1" 2>/dev/null && return 0
    [ -f "$MODULE_DIR/$1.ko" ] && insmod "$MODULE_DIR/$1.ko" >> "$LOG" 2>&1
    grep -q "^$1 " /proc/modules 2>/dev/null
}

write_shell_wrapper() {
    cat > "$SHELL_WRAP" <<'EOF'
#!/bin/sh
cd /mnt/sdcard 2>/dev/null || cd /
export HOME=/tmp PS1='shallots:\w# ' TERM=${TERM:-vt100}
export LD_LIBRARY_PATH=/mnt/sdcard/cubegm/lib:/mnt/sdcard/cubegm/usr/lib:$LD_LIBRARY_PATH
logs() { tail -n 40 -f /mnt/sdcard/log.txt; }
echo
echo "ShallotsOS USB debug shell ($(cat /mnt/sdcard/cubegm/version.txt 2>/dev/null))"
echo "  logs                  follow log.txt (create an empty /mnt/sdcard/log.txt"
echo "                        and restart for the detailed boot/launch log)"
echo "  ls cubegm/logs        ShallotsOS logs; dmesg; top; cat /tmp/tfdevice.env"
echo "  Ctrl+C stops a command; 'exit' starts a fresh shell"
echo
exec /bin/sh -i
EOF
    chmod 755 "$SHELL_WRAP"
}

tty_node() {
    [ -c "$TTY" ] && return 0
    dev=$(cat /sys/class/tty/ttyGS0/dev 2>/dev/null) || return 1
    mknod "$TTY" c "${dev%%:*}" "${dev##*:}" 2>/dev/null
    [ -c "$TTY" ]
}

# One shell at a time on the serial port, restarted when it exits. getty or
# setsid makes the port the shell's controlling terminal, so Ctrl+C works.
serve() {
    trap 'rm -f "$RUN_FLAG"; exit 0' TERM INT
    while [ -f "$RUN_FLAG" ]; do
        if ! tty_node; then sleep 1; continue; fi
        if busybox getty --help 2>&1 | grep -q -- '-n'; then
            busybox getty -L -n -l "$SHELL_WRAP" 115200 ttyGS0 < /dev/null > /dev/null 2>&1
        elif command -v setsid > /dev/null 2>&1; then
            setsid sh -c "exec $SHELL_WRAP <> $TTY >&0 2>&1"
        else
            "$SHELL_WRAP" <> "$TTY" >&0 2>&1
        fi
        sleep 1
    done
}

running() {
    pid=$(cat "$PIDFILE" 2>/dev/null) || return 1
    [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null
}

start() {
    running && { echo "already running"; return 0; }
    [ "$(id -u)" = 0 ] || { log "ERROR: must run as root"; return 1; }
    for udc in "$CONFIG_ROOT"/usb_gadget/*/UDC; do
        [ -e "$udc" ] || continue
        [ "$udc" = "$GADGET/UDC" ] && continue
        [ -z "$(cat "$udc" 2>/dev/null)" ] || { log "USB mode is active; not starting"; return 1; }
    done
    grep -q " $CONFIG_ROOT configfs" /proc/mounts || mount -t configfs none "$CONFIG_ROOT" 2>> "$LOG"
    load_module libcomposite
    if ! load_module u_serial || ! load_module usb_f_acm; then
        log "ERROR: USB serial modules did not load (see dmesg below)"
        dmesg 2>/dev/null | tail -n 20 >> "$LOG"
        return 1
    fi
    find_role_path || { log "ERROR: USB role control not found"; return 1; }
    log "start: kernel $(uname -r), role $(cat "$ROLE_PATH" 2>/dev/null)"
    printf 'peripheral\n' > "$ROLE_PATH" 2>/dev/null || printf 'b_peripheral\n' > "$ROLE_PATH" 2>/dev/null \
        || { log "ERROR: cannot switch USB to peripheral"; return 1; }
    n=0
    while [ "$n" -lt 30 ] && [ ! -e "$SYS_ROOT/class/udc/$UDC_NAME" ]; do sleep 0.1; n=$((n + 1)); done
    [ -e "$SYS_ROOT/class/udc/$UDC_NAME" ] || { log "ERROR: UDC $UDC_NAME did not appear"; stop; return 1; }

    if [ ! -d "$GADGET/functions/acm.GS0" ]; then
        mkdir -p "$GADGET" || { log "ERROR: cannot create gadget"; stop; return 1; }
        # Linux Foundation multifunction gadget; the IAD class codes let
        # Windows' inbox usbser driver claim the ACM pair as one COM port.
        echo 0x1d6b > "$GADGET/idVendor"
        echo 0x0104 > "$GADGET/idProduct"
        echo 0x0100 > "$GADGET/bcdDevice"
        echo 0x0200 > "$GADGET/bcdUSB"
        echo 0xEF > "$GADGET/bDeviceClass"
        echo 0x02 > "$GADGET/bDeviceSubClass"
        echo 0x01 > "$GADGET/bDeviceProtocol"
        mkdir -p "$GADGET/strings/0x409" "$GADGET/configs/c.1/strings/0x409"
        echo ShallotsOS > "$GADGET/strings/0x409/manufacturer"
        echo "ShallotsOS debug shell" > "$GADGET/strings/0x409/product"
        echo "shallots-$(cat /etc/machine-id 2>/dev/null | cut -c1-12)" > "$GADGET/strings/0x409/serialnumber"
        echo "Serial console" > "$GADGET/configs/c.1/strings/0x409/configuration"
        echo 120 > "$GADGET/configs/c.1/MaxPower"
        mkdir "$GADGET/functions/acm.GS0" 2>> "$LOG" || { log "ERROR: cannot create the ACM function"; stop; return 1; }
    fi
    [ -L "$GADGET/configs/c.1/acm.GS0" ] || ln -s "$GADGET/functions/acm.GS0" "$GADGET/configs/c.1/acm.GS0"
    echo "$UDC_NAME" > "$GADGET/UDC" 2>> "$LOG" || { log "ERROR: cannot bind $UDC_NAME"; stop; return 1; }

    write_shell_wrapper
    : > "$RUN_FLAG"
    serve < /dev/null > /dev/null 2>&1 &
    echo $! > "$PIDFILE"
    log "started: serial port ready (pid $!)"
}

stop() {
    rm -f "$RUN_FLAG"
    if running; then
        kill "$(cat "$PIDFILE")" 2>/dev/null
    fi
    rm -f "$PIDFILE"
    # The shell on the port, wherever it was started from.
    for p in /proc/[0-9]*; do
        [ -r "$p/cmdline" ] || continue
        case "$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null)" in
            *"$SHELL_WRAP"*) kill "${p#/proc/}" 2>/dev/null ;;
        esac
        [ "$(readlink "$p/fd/0" 2>/dev/null)" = "$TTY" ] && kill -HUP "${p#/proc/}" 2>/dev/null
    done
    if [ -d "$GADGET" ]; then
        [ -n "$(cat "$GADGET/UDC" 2>/dev/null)" ] && echo "" > "$GADGET/UDC" 2>/dev/null
        rm -f "$GADGET/configs/c.1/acm.GS0"
        rmdir "$GADGET/configs/c.1/strings/0x409" "$GADGET/configs/c.1" \
              "$GADGET/functions/acm.GS0" "$GADGET/strings/0x409" "$GADGET" 2>/dev/null
        # Hand the port back to the normal (host) role, as USB mode does.
        find_role_path && { printf 'host\n' > "$ROLE_PATH" 2>/dev/null || printf 'b_idle\n' > "$ROLE_PATH" 2>/dev/null; }
        log "stopped"
    fi
    return 0
}

case "${1:-}" in
    start) start ;;
    stop) stop ;;
    status)
        if running; then echo "running (UDC $(cat "$GADGET/UDC" 2>/dev/null), $(cat "$SYS_ROOT/class/udc/$UDC_NAME/state" 2>/dev/null))"
        else echo stopped; fi ;;
    *) echo "Usage: $0 start|stop|status" >&2; exit 2 ;;
esac
