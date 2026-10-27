#!/bin/sh
set -eu

PORT_DIR=/mnt/sdcard/roms/ports/openlara
if ! /mnt/sdcard/cubegm/port_check.sh \
    openlara \
    https://github.com/tzubertowski/TreeFrogUI_PortsCollection/blob/main/docs/setup/openlara.md \
    /mnt/sdcard/frogui/port-help/openlara.png \
    "$PORT_DIR" 'Start OpenLara.gba' LEVEL1.PHD; then exit 0; fi
cd "$PORT_DIR"
rm -f /mnt/sdcard/cubegm/port_error.txt
exec /mnt/sdcard/cubegm/picoarch \
    /mnt/sdcard/cubegm/cores/openlara_libretro.so \
    "$PORT_DIR/Start OpenLara.gba"
