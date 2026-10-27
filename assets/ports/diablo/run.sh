#!/bin/sh
set -eu

PORT_DIR=/mnt/sdcard/roms/ports/diablo
if ! /mnt/sdcard/cubegm/port_check.sh \
    diablo \
    https://github.com/tzubertowski/TreeFrogUI_PortsCollection/blob/main/docs/setup/diablo.md \
    /mnt/sdcard/frogui/port-help/diablo.png \
    "$PORT_DIR" DIABDAT.MPQ devilutionx.mpq; then exit 0; fi
cd "$PORT_DIR"
rm -f /mnt/sdcard/cubegm/port_error.txt
export HOME="$PORT_DIR"
export LC_ALL=C LANG=C
export SDL_NOMOUSE=1 SDL_MOUSEDEV=/dev/null SDL_MOUSEDRV=none
export LD_LIBRARY_PATH=/mnt/sdcard/cubegm/lib:/lib:/usr/lib
exec /mnt/sdcard/cubegm/devilutionx --verbose --diablo -n
