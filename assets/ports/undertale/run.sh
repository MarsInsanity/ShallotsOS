#!/bin/sh
set -eu

PORT_DIR=/mnt/sdcard/roms/ports/undertale
DATA_DIR=
[ -f "$PORT_DIR/.data/game.unx" ] && DATA_DIR=.data/
if ! /mnt/sdcard/cubegm/port_check.sh \
    undertale \
    https://github.com/tzubertowski/TreeFrogUI_PortsCollection/blob/main/docs/setup/undertale.md \
    /mnt/sdcard/frogui/port-help/undertale.png \
    "$PORT_DIR" 'Start Undertale.butterscotch' \
    "${DATA_DIR}game.unx" "${DATA_DIR}ATLAS.BIN" "${DATA_DIR}ATLAS.RGBA4444"; then exit 0; fi
cd "$PORT_DIR"
rm -f /mnt/sdcard/cubegm/port_error.txt
exec /mnt/sdcard/cubegm/picoarch \
    /mnt/sdcard/cubegm/cores/butterscotch_libretro.so \
    "$PORT_DIR/Start Undertale.butterscotch"
