#!/bin/sh
set -eu

[ "$#" -ge 5 ] || exit 2
slug=$1
guide_url=$2
qr_path=$3
port_dir=$4
shift 4
error_file=/mnt/sdcard/cubegm/port_error.txt

missing=
if [ ! -d "$port_dir" ]; then
    missing="roms/ports/$slug/ (port data folder)"
else
    for file do
        if [ ! -f "$port_dir/$file" ]; then
            missing="${missing}${missing:+
}$file"
        fi
    done
fi

[ -n "$missing" ] || exit 0
tmp="$error_file.tmp"
{
    printf '%s\n%s\n%s\n' "$slug" "$guide_url" "$qr_path"
    printf '%s\n' "$missing"
} > "$tmp"
mv -f "$tmp" "$error_file"
sync
exit 1
