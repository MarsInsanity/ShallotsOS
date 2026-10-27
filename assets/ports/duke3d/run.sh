#!/bin/sh
set -eu

PORT_DIR=/mnt/sdcard/roms/ports/duke3d
if ! /mnt/sdcard/cubegm/port_check.sh \
    duke3d \
    https://github.com/tzubertowski/TreeFrogUI_PortsCollection/blob/main/docs/setup/duke3d.md \
    /mnt/sdcard/frogui/port-help/duke3d.png \
    "$PORT_DIR" DUKE3D.GRP DUKE.RTS; then exit 0; fi
cd "$PORT_DIR"
rm -f /mnt/sdcard/cubegm/port_error.txt
exec /mnt/sdcard/cubegm/picoarch \
    /mnt/sdcard/cubegm/cores/duke3d_libretro.so \
    "$PORT_DIR/Start Duke Nukem 3D.duke3d"
