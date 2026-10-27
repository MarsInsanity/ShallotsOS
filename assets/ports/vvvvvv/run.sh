#!/bin/sh
set -eu

PORT_DIR=/mnt/sdcard/roms/ports/vvvvvv
if ! /mnt/sdcard/cubegm/port_check.sh \
    vvvvvv \
    https://github.com/tzubertowski/TreeFrogUI_PortsCollection/blob/main/docs/setup/vvvvvv.md \
    /mnt/sdcard/frogui/port-help/vvvvvv.png \
    "$PORT_DIR" data.zip; then exit 0; fi
rm -f /mnt/sdcard/cubegm/port_error.txt
exec /mnt/sdcard/cubegm/picoarch \
    /mnt/sdcard/cubegm/cores/vvvvvv_libretro.so \
    "$PORT_DIR/data.zip"
