# Installing ShallotsOS

ShallotsOS is a TreeFrogUI-based system with an OnionOS-style menu. This
package is complete: you do not need to install TreeFrogUI first.

Supported consoles: R36SX, R36HD, SF3000, SF3000 HD (no HDMI), SF3500,
SF3100 and GB350. Back up your SD card before you start.

## New install

1. Start from a clean card: format it (FAT32) and restore the stock backup
   for your console that TreeFrogUI's install guide links
   (https://github.com/tzubertowski/TreeFrogUI/blob/main/install.md, Step 1).
   A card that already runs TreeFrogUI is fine as it is.
2. Copy everything inside this ZIP's `release/` folder, except
   `install_first/`, to the root of the SD card, replacing files when asked.
3. Open `install_first/`, find the folder for your console and copy its
   `cubegm/` folder over the card's `cubegm/`, replacing files.
   This is the step that makes the console boot ShallotsOS.
4. Put the card back in the console and switch it on.

On the first boot ShallotsOS prepares the display driver for your console
from the driver that is already on its card (it copies it and, on an R36SX,
makes the v2.7 variant). Nothing from the stock firmware is included in this
ZIP. If that step fails, `cubegm/logs/shallots_setup.log` says why.

ROMs, BIOS files and saves are never part of the package; add your own.

## Updating

Copy `update.zip` to the root of the SD card and switch the console on. The
update installs itself, then the console restarts. Your settings, button map
and emulator settings are kept; `update.log` on the card records the result.

## Undoing

Copy your backup of the card's `cubegm/` folder (or the stock firmware's)
back, or install TreeFrogUI over ShallotsOS.
