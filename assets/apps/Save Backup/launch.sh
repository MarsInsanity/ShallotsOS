#!/bin/sh
# Save Backup: a ShallotsOS script app (App/<name>/launch.sh). It copies the
# game saves to Backups/<date>/ on the SD card and can put a backup back.
# It is also the example for docs/apps.md: every screen comes from
# shallots_ui, which the launcher puts on PATH.
#
# Saved: picoarch/ (in-game saves and save states of the libretro
# emulators, without picoarch/.unzipped, which only holds unpacked ROMs),
# saves/ (J2ME) and PCSX4ALL's cubegm/.pcsx4all and cubegm/cores/.pcsx4all.

SD=${SHALLOTS_SD:-/mnt/sdcard}
BACKUPS=$SD/Backups
TITLE="Save Backup"
ITEMS="picoarch saves cubegm/.pcsx4all cubegm/cores/.pcsx4all"

# shallots_ui exits 3 when it has no usable screen (rotated panels).
ui() {
    shallots_ui "$@"
    rc=$?
    [ "$rc" = 3 ] && exit 1
    return "$rc"
}

# Files to copy from $1 (relative to the card) into the list file $2.
list_files() {
    for item in $ITEMS; do
        [ -e "$1/$item" ] || continue
        (cd "$1" && find "$item" -type f ! -path 'picoarch/.unzipped/*') >> "$2"
    done
}

# Copies the files in list $3 from $1 to $2, with a progress screen.
copy_files() {
    total=$(wc -l < "$3")
    n=0
    while IFS= read -r f; do
        mkdir -p "$2/${f%/*}" && cp -p "$1/$f" "$2/$f" || return 1
        n=$((n + 1))
        if [ $((n % 10)) = 0 ] || [ "$n" = "$total" ]; then
            ui progress --title "$TITLE" $((n * 100 / total)) "$n of $total files"
        fi
    done < "$3"
    sync
}

backup() {
    name=$(date +%Y-%m-%d_%H%M 2>/dev/null)
    [ -n "$name" ] || name=backup
    while [ -e "$BACKUPS/$name" ]; do name="${name}_2"; done
    list=${TMPDIR:-/tmp}/save_backup.list
    : > "$list"
    list_files "$SD" "$list"
    if [ ! -s "$list" ]; then
        rm -f "$list"
        ui message --title "$TITLE" --wait "There are no saves to back up yet."
        return
    fi
    ui confirm --title "$TITLE" --yes "Back up" --no "Cancel" \
        "Copy $(wc -l < "$list") save files to Backups/$name on the SD card?" || { rm -f "$list"; return; }
    ui progress --title "$TITLE" 0 "Starting..."
    if copy_files "$SD" "$BACKUPS/$name" "$list"; then
        ui message --title "$TITLE" --wait "Saved to Backups/$name."
    else
        ui message --title "$TITLE" --wait "The backup could not be completed. Is the SD card full?"
    fi
    rm -f "$list"
}

restore() {
    choice=$(choose_backup "Restore which backup?") || return
    ui confirm --title "$TITLE" --yes "Restore" --no "Cancel" \
        "Copy the saves in $choice back? Saves with the same name on the card are replaced." || return
    list=${TMPDIR:-/tmp}/save_backup.list
    : > "$list"
    list_files "$BACKUPS/$choice" "$list"
    if copy_files "$BACKUPS/$choice" "$SD" "$list"; then
        ui message --title "$TITLE" --wait "Restored $choice."
    else
        ui message --title "$TITLE" --wait "The restore could not be completed."
    fi
    rm -f "$list"
}

delete() {
    choice=$(choose_backup "Delete which backup?") || return
    ui confirm --title "$TITLE" --yes "Delete" --no "Cancel" "Delete the backup $choice?" || return
    case "$choice" in ""|*/*|.|..) return ;; esac   # only a folder directly in Backups/
    rm -rf "$BACKUPS/$choice" && sync
    ui notify "Deleted $choice"
}

# A menu of the backups, newest first; prints the chosen name.
choose_backup() {
    prompt=$1
    names=$(ls -1r "$BACKUPS" 2>/dev/null)
    if [ -z "$names" ]; then
        ui message --title "$TITLE" --wait "There are no backups yet."
        return 1
    fi
    old_ifs=$IFS
    IFS='
'
    # shellcheck disable=SC2086
    set -- $names
    IFS=$old_ifs
    shallots_ui menu --title "$prompt" "$@"
}

while :; do
    action=$(ui menu --title "$TITLE" --index "Back up saves now" "Restore a backup" "Delete a backup") || exit 0
    case "$action" in
        0) backup ;;
        1) restore ;;
        2) delete ;;
    esac
done
