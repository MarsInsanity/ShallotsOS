#!/bin/sh
# Check the bundled OnionOS-format theme: valid config, every skin image the
# OnionOS layout uses at its expected size, the fallback font and its licence,
# and (when Pillow is installed) that the committed artwork matches the
# generator.
set -eu
cd "$(dirname "$0")/.."

THEME=assets/onion/Themes/ShallotsOS
[ -f assets/onion/fonts/Exo-2-Bold-Italic.ttf ]
grep -q "SIL OPEN FONT LICENSE Version 1.1" assets/onion/fonts/Exo-2-Bold-Italic.OFL.txt

python3 - "$THEME" <<'EOF'
import json
import struct
import sys
from pathlib import Path

theme = Path(sys.argv[1])
config = json.loads((theme / "config.json").read_text())
for key in ("title", "hint", "list", "grid", "batteryPercentage"):
    assert key in config, f"config.json: missing {key}"
for key in ("grid1x4", "grid3x4", "color", "selectedcolor"):
    assert key in config["grid"], f"config.json grid: missing {key}"


def png_size(path):
    data = path.read_bytes()[:24]
    assert data[:8] == b"\x89PNG\r\n\x1a\n", f"{path}: not a PNG"
    return struct.unpack(">II", data[16:24])


expected = {
    "background": (640, 480), "bg-title": (640, 60), "tips-bar-bg": (640, 60),
    "bg-list-s": (640, 60), "bg-list-l": (640, 90), "div-line-h": (640, 4),
    "bg-game-item-n": (154, 170), "bg-game-item-f": (154, 170),
    "extra/gs-top-bar": (640, 40), "extra/gs-bottom-bar": (640, 40),
}
for key in ("recent", "favorite", "game", "app", "setting", "retroarch"):
    for state in "nf":
        expected[f"ic-{key}-{state}"] = (156, 278)
required = [
    "miyoo-topbar", "icon-A-54", "icon-B-54", "icon-left-arrow-24",
    "icon-right-arrow-24", "extra/toggle-on", "extra/toggle-off", "dot-a",
    "dot-n", "power-0%-icon", "power-20%-icon", "power-50%-icon",
    "power-80%-icon", "power-full-icon", "ic-power-charge-100%",
    "preview-bg", "ic-favorite-mark", "icon-folder",
]
for name in list(expected) + required:
    path = theme / "skin" / f"{name}.png"
    assert path.is_file(), f"missing skin/{name}.png"
    size = png_size(path)
    if name in expected:
        assert size == expected[name], f"skin/{name}.png is {size}, want {expected[name]}"
assert png_size(theme / "preview.png") == (480, 360)
assert (theme / "sound/change.wav").read_bytes()[:4] == b"RIFF"
print("theme assets ok")
EOF

if python3 -c "import PIL" 2>/dev/null; then
    TMP=$(mktemp -d)
    trap 'rm -rf "$TMP"' EXIT INT TERM
    python3 tools/onion_theme/make_shallotsos_theme.py "$TMP/theme" >/dev/null
    (cd "$TMP/theme" && find . -type f | sort | xargs sha256sum) > "$TMP/generated.sum"
    (cd "$THEME" && find . -type f ! -name preview.png | sort | xargs sha256sum) > "$TMP/committed.sum"
    diff -u "$TMP/committed.sum" "$TMP/generated.sum"
    echo "theme matches its generator"
else
    echo "Pillow not installed: skipped the generator comparison"
fi
echo "OnionOS theme test passed"
