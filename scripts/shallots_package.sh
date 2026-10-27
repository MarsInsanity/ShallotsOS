#!/bin/bash
# Build the full ShallotsOS install ZIP and update.zip.
#
#   scripts/shallots_package.sh <version> <TreeFrogUI full ZIP> <overlay dir> \
#       <shallots_setup binary> <output dir>
#
# The base is TreeFrogUI's published full release (its cores, BIOS helpers,
# boot hijack, updater). ShallotsOS then:
#   - removes the stock drivers (cubegm/driver_*.so): shallots_setup makes
#     them from the console's own driver on the first boot;
#   - lays its overlay on top (menu, picoarch, PCSX4ALL, HCSNES, HCPS1,
#     Books, media player, volume indicator, themes);
#   - generates every install_first/<device>/cubegm/zhijack.sh from its own
#     hijack/zhijack.tpl.sh, keeping the device's setting.xml, hijack core
#     and (on 854x480 panels) boot logo from the base;
#   - ships its own frogui/settings.txt (only what the launcher reads) and
#     no picoarch.cfg, so ShallotsOS's built-in defaults apply.
# update.zip carries every release file except install_first and the user's
# configs (frogui/settings.txt, keymap.txt, picoarch.cfg, PCSX4ALL's cfg),
# which an update keeps. It never carries setting.xml or a stock driver.
set -euo pipefail
cd "$(dirname "$0")/.."

[ $# -eq 5 ] || { sed -n '2,4p' "$0" >&2; exit 2; }
VERSION=$1 BASE_ZIP=$2 OVERLAY=$3 SETUP_BIN=$4 OUT=$5
case "$VERSION" in
    ''|*[!A-Za-z0-9._-]*) echo "ERROR: invalid version: $VERSION" >&2; exit 1 ;;
esac
[ -f "$BASE_ZIP" ] && [ -d "$OVERLAY" ] && [ -x "$SETUP_BIN" ] || {
    echo "ERROR: missing base ZIP, overlay or shallots_setup" >&2; exit 1; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
REL="$WORK/release"
mkdir -p "$OUT"

# The device tables, from the release script TreeFrogUI maintains.
eval "$(sed -n '/^declare -A HJ=(/,/^)/p' build_release.sh)"
eval "$(sed -n '/^declare -A NOSLEEP_ADDRS=(/,/^)/p' build_release.sh)"
[ "${#HJ[@]}" -gt 0 ] || { echo "ERROR: no device table in build_release.sh" >&2; exit 1; }

# 1) TreeFrogUI's release tree.
7z x -y "$BASE_ZIP" "-o$WORK" >/dev/null
[ -d "$REL/cubegm" ] && [ -d "$REL/install_first" ] || {
    echo "ERROR: $BASE_ZIP has no release/ tree" >&2; exit 1; }
BASE_VERSION=$(cat "$REL/cubegm/version.txt" 2>/dev/null || echo unknown)

# 2) No stock drivers; TreeFrogUI's defaults out.
rm -f "$REL"/cubegm/driver_*.so "$REL/picoarch.cfg"

# 3) ShallotsOS on top. The 640x480 boot logo goes to those devices only.
LOGO_640="$OVERLAY/cubegm/xgame-logo.bmp"
(cd "$OVERLAY" && find . -type f ! -name SHALLOTSOS-README.txt ! -path ./cubegm/xgame-logo.bmp) \
    | while IFS= read -r f; do
        mkdir -p "$REL/$(dirname "$f")"
        cp -a "$OVERLAY/$f" "$REL/$f"
      done
install -m 0755 "$SETUP_BIN" "$REL/cubegm/shallots_setup"
install -m 0755 hijack/tfupdate.sh "$REL/cubegm/tfupdate.sh"
cat > "$REL/frogui/settings.txt" <<'EOF'
disable_sleep=on
filter=bilinear
EOF

# 4) The launchers, from ShallotsOS's template.
for dir in "$REL"/install_first/*/; do
    dev=$(basename "$dir")
    entry=${HJ[$dev]:-}
    [ -n "$entry" ] || { echo "ERROR: no launcher settings for $dev" >&2; exit 1; }
    [ -f "$dir/cubegm/setting.xml" ] && [ -f "$dir/cubegm/cores/libemu_md.so" ] || {
        echo "ERROR: $dev: base is missing setting.xml or the hijack core" >&2; exit 1; }
    read -r DEV PW PH ASPN ASPD ROT PRESENT DRIVER KILL <<< "$entry"
    Z="$dir/cubegm/zhijack.sh"
    sed -e "s/@DEV_LABEL@/$dev/g" -e "s/@DEV@/$DEV/g" -e "s/@PW@/$PW/g" \
        -e "s/@PH@/$PH/g" -e "s/@ASPN@/$ASPN/g" -e "s/@ASPD@/$ASPD/g" \
        -e "s/@ROT@/$ROT/g" -e "s/@PRESENT@/$PRESENT/g" -e "s/@DRIVER@/$DRIVER/g" \
        -e "s#@NOSLEEP@#${NOSLEEP_ADDRS[$dev]:-}#g" \
        hijack/zhijack.tpl.sh > "$Z"
    if [ "$KILL" = stop ]; then sed -i '/#@KILL@/d' "$Z"; else sed -i 's/ #@KILL@//' "$Z"; fi
    if [ "$dev" = r36sx ] || [ "$dev" = r36hd ]; then
        sed -i -e 's/ #@R36@//' -e '/#@HW@/d' "$Z"
    else
        sed -i -e '/#@R36@/d' -e 's/ #@HW@//' "$Z"
    fi
    if grep -o '@[A-Z_]*@' "$Z" | grep -q .; then
        echo "ERROR: $dev: unfilled placeholders in zhijack.sh" >&2; exit 1
    fi
    sh -n "$Z"
    chmod +x "$Z"
    case "$dev" in r36sx|r36hd|gb350) [ -f "$LOGO_640" ] && cp "$LOGO_640" "$dir/cubegm/xgame-logo.bmp" ;; esac
done

# 5) Docs and version.
cp docs/INSTALL-ShallotsOS.md "$REL/INSTALL-ShallotsOS.md"
printf '%s\n' "$VERSION" > "$REL/cubegm/version.txt"
cat > "$REL/SHALLOTSOS-VERSION.txt" <<EOF
ShallotsOS $VERSION, built on TreeFrogUI $BASE_VERSION ($(basename "$BASE_ZIP"))
$(sed -n '1p' "$OVERLAY/SHALLOTSOS-README.txt" 2>/dev/null)
EOF

# 6) Checks: nothing stock beyond the per-device setting.xml.
if find "$REL" -name 'driver_*.so' | grep -q .; then
    echo "ERROR: a stock driver is in the release" >&2; exit 1
fi
for need in cubegm/shallots_setup cubegm/tfupdate.sh cubegm/picoarch cubegm/cores/frogui_libretro.so \
            cubegm/shallots_osd cubegm/ebook Themes/ShallotsOS/config.json; do
    [ -f "$REL/$need" ] || { echo "ERROR: missing $need" >&2; exit 1; }
done

python3 scripts/pack_zip.py --normalize-tree "$REL"
FULL="$OUT/ShallotsOS_$VERSION.zip"
rm -f "$FULL"
python3 scripts/pack_zip.py "$(realpath "$OUT")/ShallotsOS_$VERSION.zip" "$WORK" release

# 7) update.zip (TreeFrogUI's updater format 1).
BUNDLE="$WORK/upd/treefrog-update"
mkdir -p "$BUNDLE/payload" "$BUNDLE/device"
(cd "$REL" && find . -path ./install_first -prune -o -type d -print) | while IFS= read -r d; do
    mkdir -p "$BUNDLE/payload/$d"
done
(cd "$REL" && find . -path ./install_first -prune -o -type f -print) | while IFS= read -r f; do
    case "${f#./}" in
        frogui/settings.txt|frogui/keymap.txt|picoarch.cfg|cubegm/.pcsx4all/*|cubegm/cores/.pcsx4all/*) continue ;;
    esac
    cp -a "$REL/$f" "$BUNDLE/payload/$f"
done
for dir in "$REL"/install_first/*/; do
    dev=$(basename "$dir")
    dst="$BUNDLE/device/$dev"
    mkdir -p "$dst/cubegm/cores"
    : > "$dst/delete.txt"
    cp -a "$dir/cubegm/zhijack.sh" "$dst/cubegm/"
    cp -a "$dir/cubegm/cores/libemu_md.so" "$dst/cubegm/cores/"
    [ -f "$dir/cubegm/xgame-logo.bmp" ] && cp -a "$dir/cubegm/xgame-logo.bmp" "$dst/cubegm/"
done
: > "$BUNDLE/delete.txt"
cat > "$BUNDLE/manifest.txt" <<EOF
format=1
version=$VERSION
base_version=unknown
base_archive=$(basename "$BASE_ZIP")
EOF
(cd "$BUNDLE" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)
if find "$BUNDLE" \( -name setting.xml -o -name 'driver_*.so' -o -type l \) | grep -q .; then
    echo "ERROR: update bundle has setting.xml, a stock driver or a symlink" >&2; exit 1
fi
for keep in frogui/settings.txt frogui/keymap.txt; do
    [ ! -e "$BUNDLE/payload/$keep" ] || { echo "ERROR: update would replace $keep" >&2; exit 1; }
done
rm -f "$OUT/update.zip"
python3 scripts/pack_zip.py "$(realpath "$OUT")/update.zip" "$WORK/upd" treefrog-update

ls -lh "$FULL" "$OUT/update.zip"
