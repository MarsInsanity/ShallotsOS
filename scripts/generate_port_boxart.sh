#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TEMPLATE="$ROOT/assets/ports/floppy-template.png"

if [ "$#" -ne 2 ] || [ ! -f "$1" ] || [ ! -f "$TEMPLATE" ]; then
    echo "usage: $0 <artwork.png> <output.png>" >&2
    exit 2
fi
if command -v magick >/dev/null 2>&1; then
    IM=magick
elif command -v convert >/dev/null 2>&1; then
    IM=convert
else
    echo "ImageMagick is required" >&2
    exit 1
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

# Keep one exact 500x500 floppy frame; only the artwork in its label window varies.
"$IM" -size 500x500 xc:none \
    -fill white -draw 'roundrectangle 76,177 424,425 12,12' "$tmp/mask.png"
"$IM" -size 500x500 xc:none \
    \( "$1" -resize '348x248^' -gravity center -extent 348x248 +repage \) \
    -gravity northwest -geometry +76+177 -composite \
    "$tmp/mask.png" -compose DstIn -composite "$tmp/art.png"
"$IM" "$TEMPLATE" "$tmp/art.png" -compose over -composite "$2"
