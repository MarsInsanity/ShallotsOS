# Making ShallotsOS apps

A ShallotsOS app is a folder in `App/` on the SD card with a shell script
in it. It shows up in the **Apps** tab next to the built-in apps. The layout
is the same as SpruceOS's, so simple Spruce script apps often work as they
are (Spruce apps that include ARM programs do not: this console is MIPS).

```
App/
  My Tool/
    config.json
    launch.sh
    icon.png        (optional)
```

`config.json`:

```json
{
    "label": "My Tool",
    "icon": "icon.png",
    "launch": "launch.sh",
    "description": "What the app does"
}
```

- `label`: the name in the Apps tab (the folder name if missing).
- `icon`: a PNG shown next to the name; any size, it is scaled.
  Default `icon.png`.
- `launch`: the script to run. Default `launch.sh`.
- `"hidden": true` hides the app.
- Paths are relative to the app's folder. SpruceOS's absolute
  `/mnt/SDCARD/...` paths are understood too.

The script runs with `/bin/sh` (BusyBox), **from its own folder**, as root.
`$SHALLOTS_APP_DIR` is that folder. The SD card is `/mnt/sdcard`. When the
script ends, you are back in the Apps tab.

## Screens: `shallots_ui`

The menu has closed while your script runs, so the script draws its screens
with `shallots_ui` (it is on `PATH`). Screens use the active theme's colours,
background and font, and stay on screen until the next one.

| Command | What it does | Result |
|---|---|---|
| `shallots_ui message [--title T] [--wait] "Text"` | Text in the middle; `--wait` waits for A or B | `--wait`: exit 0 for A, 1 for B |
| `shallots_ui progress [--title T] 40 "Copying..."` | Progress bar at 40% | returns at once |
| `shallots_ui confirm [--title T] [--yes L] [--no L] "Delete?"` | Two buttons, "No" selected first | exit 0 = yes, 1 = no or B |
| `shallots_ui menu [--title T] [--index] [--selected N] "One" "Two" ...` | A list | prints the chosen item (or its number, from 0, with `--index`); exit 1 on B |
| `shallots_ui keyboard [--title T] [--initial "text"]` | On-screen keyboard (A type, Y delete, X shift, START done) | prints the text; exit 1 on B |
| `shallots_ui notify [--seconds N] "Done!"` | A toast over the current screen for N seconds (2) | returns after it |
| `shallots_ui clear` | Hands the screen back (done for you when the app ends) | |

Exit status 3 means there is no usable screen (rotated panels such as
SF3500-class consoles are not supported yet); 2 means wrong arguments.

### Example

```sh
#!/bin/sh
choice=$(shallots_ui menu --title "Clean up" "Delete screenshots" "Cancel") || exit 0
[ "$choice" = "Cancel" ] && exit 0
shallots_ui confirm --title "Clean up" "Delete all screenshots?" || exit 0
n=$(ls /mnt/sdcard/screenshots 2>/dev/null | wc -l)
rm -f /mnt/sdcard/screenshots/*
shallots_ui message --wait "Deleted $n screenshots."
```

A complete app is **Save Backup** (`App/Save Backup/launch.sh`): menus,
confirmations and a progress bar while it copies files.

## Good to know

- **Scripts run as root and can change anything on the card.** Only add
  apps you trust, and be careful with `rm`.
- **Memory is tight** (about 43 MB for everything). `/tmp` is RAM on this
  console: keep only small files there and delete them when done. Put big
  files on the SD card.
- Output (`echo`) is not shown on screen; it goes to `/tmp/treefrog_ui.log`
  (copied to `cubegm/logs/last_standalone.log`). Use `shallots_ui` for
  anything the player should see.
- Programs for this console must be built for MIPS (little-endian
  MIPS32r2, hard-float): see the SF3000 toolchain the ShallotsOS build uses.
- With the USB Debug Shell (Settings > System) you can run and test your
  script from a PC while the console runs.
