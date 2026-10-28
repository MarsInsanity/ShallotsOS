#!/bin/sh
# TreeFrogUI offline updater. This file is copied to /tmp by zhijack.sh before
# execution, so the installed copy can safely replace itself during an update.

SDROOT=${TFUPDATE_ROOT:-/mnt/sdcard}
USBROOT=${TFUPDATE_USB_ROOT:-/media/hdd}
PACKAGE="$SDROOT/update.zip"
# USB-host mode is mounted by mdev before the launcher runs. Prefer an update
# on the external drive, while retaining the SD-root fallback for normal use.
[ -f "$USBROOT/update.zip" ] && PACKAGE="$USBROOT/update.zip"
WORK_DIR="$SDROOT/.treefrog-update"
STAGE="$WORK_DIR/staging"
LOG="$SDROOT/update.log"
DEVICE=${1:-}

case "$DEVICE" in
    r36sx|r36hd|sf3000|sf3000hd_nohdmi|sf3500|sf3000hd|sf3100|gb350) ;;
    *) exit 0 ;;
esac

[ -f "$PACKAGE" ] || exit 0

log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S' 2>/dev/null) $*" >> "$LOG"
}

# ShallotsOS: percent done (0-100) for the "Updating" screen zhijack.sh shows
# (shallots_osd --splash). Unpacking 0-40, checking 40-50, installing 50-98.
PROGRESS=${TFUPDATE_PROGRESS:-/tmp/tfupdate.progress}
progress() {
    echo "$1" > "$PROGRESS.tmp" 2>/dev/null && mv -f "$PROGRESS.tmp" "$PROGRESS" 2>/dev/null
}

fail() {
    log "ERROR: $*"
    rm -rf "$STAGE"
    sync
    exit 1
}

extract_package() {
    if command -v unzip >/dev/null 2>&1; then
        unzip -o "$PACKAGE" -d "$STAGE"
    elif [ -x /bin/busybox ]; then
        /bin/busybox unzip -o "$PACKAGE" -d "$STAGE"
    else
        return 127
    fi
}

install_tree() {
    SOURCE=$1
    # Optional: report progress from $2 to $3 percent while copying.
    P_FROM=${2:-}
    P_TO=${3:-}
    [ -d "$SOURCE" ] || return 0

    find "$SOURCE" -type d > "$STAGE/directories.list" || return 1
    while IFS= read -r DIR; do
        REL=${DIR#"$SOURCE"/}
        [ "$DIR" = "$SOURCE" ] && continue
        mkdir -p "$SDROOT/$REL" || return 1
    done < "$STAGE/directories.list"

    find "$SOURCE" -type f > "$STAGE/files.list" || return 1
    P_TOTAL=$(wc -l < "$STAGE/files.list")
    P_N=0
    while IFS= read -r SRC; do
        P_N=$((P_N + 1))
        if [ -n "$P_FROM" ] && [ "$P_TOTAL" -gt 0 ] && [ $((P_N % 20)) = 0 ]; then
            progress $((P_FROM + (P_TO - P_FROM) * P_N / P_TOTAL))
        fi
        REL=${SRC#"$SOURCE"/}
        [ "$REL" = delete.txt ] && continue
        DST="$SDROOT/$REL"
        TMP="$DST.tfu-new.$$"
        mkdir -p "$(dirname "$DST")" || return 1
        rm -f "$TMP"
        cp "$SRC" "$TMP" || return 1
        mv -f "$TMP" "$DST" || return 1
    done < "$STAGE/files.list"
}

# A few older archives could leave root documentation with different casing.
# Remove an old variant only when the verified payload is about to replace it.
# Migration idea credited to MartStartIV (PR #75).
remove_replaced_root_doc_variants() {
    for SRC in "$BUNDLE/payload"/*.md; do
        [ -f "$SRC" ] || continue
        SOURCE_NAME=${SRC##*/}
        SOURCE_FOLDED=$(printf '%s' "$SOURCE_NAME" | tr '[:upper:]' '[:lower:]')
        for EXISTING in "$SDROOT"/*; do
            [ -f "$EXISTING" ] || continue
            EXISTING_NAME=${EXISTING##*/}
            [ "$EXISTING_NAME" = "$SOURCE_NAME" ] && continue
            EXISTING_FOLDED=$(printf '%s' "$EXISTING_NAME" | tr '[:upper:]' '[:lower:]')
            [ "$EXISTING_FOLDED" = "$SOURCE_FOLDED" ] || continue
            rm -f "$EXISTING" || return 1
        done
    done
}

mkdir -p "$WORK_DIR" || exit 1
rm -rf "$STAGE"
mkdir -p "$STAGE" || fail "cannot create staging directory"
log "Applying $(basename "$PACKAGE") for $DEVICE from $(dirname "$PACKAGE")"
progress 0

# Unpack in the background and count the files written so far against the
# archive's file count (its central directory: quick to list).
X_TOTAL=$( (unzip -l "$PACKAGE" 2>/dev/null || /bin/busybox unzip -l "$PACKAGE" 2>/dev/null) | awk 'END { print $2 }')
case "$X_TOTAL" in ''|*[!0-9]*) X_TOTAL=0 ;; esac
X_OUT="$WORK_DIR/extract.out"
extract_package > "$X_OUT" 2>&1 &
X_PID=$!
while kill -0 "$X_PID" 2>/dev/null; do
    if [ "$X_TOTAL" -gt 0 ]; then
        X_N=$(grep -c 'inflating:\|extracting:' "$X_OUT" 2>/dev/null)
        [ -n "$X_N" ] && progress $((40 * X_N / X_TOTAL))
    fi
    sleep 1
done
wait "$X_PID"
X_RC=$?
cat "$X_OUT" >> "$LOG" 2>/dev/null
rm -f "$X_OUT"
[ "$X_RC" = 0 ] || fail "archive extraction failed; package kept for retry"
progress 40

BUNDLE="$STAGE/treefrog-update"
[ -f "$BUNDLE/manifest.txt" ] || fail "manifest.txt missing"
[ -f "$BUNDLE/SHA256SUMS" ] || fail "SHA256SUMS missing"
[ "$(sed -n 's/^format=//p' "$BUNDLE/manifest.txt")" = 1 ] \
    || fail "unsupported update format"
[ -d "$BUNDLE/payload" ] || fail "universal payload missing"
[ -d "$BUNDLE/device/$DEVICE" ] || fail "payload does not support $DEVICE"

(cd "$BUNDLE" && sha256sum -c SHA256SUMS) >> "$LOG" 2>&1 \
    || fail "checksum verification failed; package kept for retry"
progress 50

VERSION=$(sed -n 's/^version=//p' "$BUNDLE/manifest.txt")
BASE_VERSION=$(sed -n 's/^base_version=//p' "$BUNDLE/manifest.txt")
BASE_MAJOR=$(sed -n 's/^base_major=//p' "$BUNDLE/manifest.txt")
case "$VERSION" in
    ''|*[!A-Za-z0-9._-]*) fail "invalid update version" ;;
esac
case "$BASE_VERSION" in
    *[!A-Za-z0-9._-]*) fail "invalid base version" ;;
esac
case "$BASE_MAJOR" in
    ''|*[!0-9]*) [ -z "$BASE_MAJOR" ] || fail "invalid base major" ;;
esac
INSTALLED_VERSION=$(cat "$SDROOT/cubegm/version.txt" 2>/dev/null)
if [ "$INSTALLED_VERSION" = "$VERSION" ]; then
    log "Reinstalling TreeFrogUI $VERSION"
else
    if [ -n "$BASE_VERSION" ] && [ "$BASE_VERSION" != unknown ]; then
        [ "$INSTALLED_VERSION" = "$BASE_VERSION" ] \
            || fail "requires $BASE_VERSION, installed version is ${INSTALLED_VERSION:-unknown}"
    fi
    if [ -n "$BASE_MAJOR" ]; then
        case "$INSTALLED_VERSION" in
            ''|unknown)
                # Early major-line releases did not write version.txt. The signed
                # base_major is the best compatibility check available for them.
                ;;
            v"$BASE_MAJOR".*) ;;
            *) fail "requires major v$BASE_MAJOR, installed version is ${INSTALLED_VERSION:-unknown}" ;;
        esac
    fi
fi

# Configs are intentionally authoritative: releases may add options, migrate
# formats, or fix incompatible defaults. Keep one pre-update copy for recovery
# without preserving stale values over the new release.
BACKUP="$WORK_DIR/backup-$VERSION"
for REL in \
    frogui/settings.txt \
    frogui/keymap.txt \
    picoarch.cfg \
    cubegm/.pcsx4all/pcsx4all.cfg \
    cubegm/cores/.pcsx4all/pcsx4all.cfg
do
    if [ -f "$BUNDLE/payload/$REL" ] && \
       [ -f "$SDROOT/$REL" ] && [ ! -f "$BACKUP/$REL" ]; then
        mkdir -p "$BACKUP/$(dirname "$REL")" || fail "cannot create config backup"
        cp "$SDROOT/$REL" "$BACKUP/$REL" || fail "cannot back up $REL"
    fi
done

# ROMs, BIOS, saves, histories and media are never deleted. Release configs are
# authoritative and replace installed configs after the backup above. Device
# files are applied last so the launcher is the final boot-critical change.
remove_replaced_root_doc_variants || fail "cannot migrate root documentation names"
install_tree "$BUNDLE/payload" 50 97 || fail "installing universal payload failed"
install_tree "$BUNDLE/device/$DEVICE" || fail "installing device payload failed"

delete_list() {
    LIST=$1
    [ -f "$LIST" ] || return 0
    while IFS= read -r REL; do
        [ -n "$REL" ] || continue
        case "$REL" in
            /*|../*|*/../*|*/..|roms/*|PS/*|GB/*|GBA/*|GBC/*|FC/*|MD/*|SFC/*|\
            cubegm/bios/*|cubegm/saves/*|cubegm/Update/*|.treefrog-update/*|\
            screenshots/*|saves/*|bios/*|update.zip|update.log)
                return 1 ;;
        esac
        rm -f "$SDROOT/$REL" || return 1
    done < "$LIST"
}

delete_list "$BUNDLE/delete.txt" || fail "unsafe or failed common deletion"
delete_list "$BUNDLE/device/$DEVICE/delete.txt" || fail "unsafe or failed device deletion"

echo "$VERSION" > "$SDROOT/cubegm/version.txt.tfu-new.$$" \
    || fail "cannot write installed-version marker"
mv -f "$SDROOT/cubegm/version.txt.tfu-new.$$" \
      "$SDROOT/cubegm/version.txt" \
    || fail "cannot install version marker"
sync

# Deletion is the commit point: failed/interrupted updates retain the package
# and converge by applying it again on the next successful boot.
rm -f "$PACKAGE" || fail "update installed but package deletion failed"
progress 100
rm -rf "$STAGE"
sync
log "SUCCESS: TreeFrogUI $VERSION installed; package removed"
exit 10
