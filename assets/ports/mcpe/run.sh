#!/bin/sh
set -eu

PORT_DIR=/mnt/sdcard/roms/ports/mcpe
if ! /mnt/sdcard/cubegm/port_check.sh \
    mcpe \
    https://github.com/tzubertowski/TreeFrogUI_PortsCollection/blob/main/docs/setup/mcpe.md \
    /mnt/sdcard/frogui/port-help/mcpe.png \
    "$PORT_DIR" 'Start Minecraft PE.mcpe' data/images/terrain.png data/fonts/minecraft.ttf; then exit 0; fi
cd "$PORT_DIR"
rm -f /mnt/sdcard/cubegm/port_error.txt
exec /mnt/sdcard/cubegm/picoarch \
    /mnt/sdcard/cubegm/cores/mcpe_libretro.so \
    "$PORT_DIR/Start Minecraft PE.mcpe"
