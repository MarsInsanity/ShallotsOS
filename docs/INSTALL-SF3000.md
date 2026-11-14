# Installing ShallotsOS on an SF3000

ShallotsOS is a custom menu for the SF3000 with an OnionOS-style look. It
runs from the SD card on top of the console's own system, which it never
changes: if anything goes wrong, putting the original card contents back
returns the console to how it was.

**This is a test build, and ShallotsOS has not been tried on an SF3000
yet.** Your notes and photos are what will make it work well there.

---

## What you need

- The SF3000 and its microSD card (or another card, 8 GB or larger).
- A computer with a microSD card reader.
- **7-Zip** (free, 7-zip.org) to open the `.7z` and `.zip` files.
- **The ShallotsOS package**: `ShallotsOS_dev-2bc1497.zip`, which you got
  together with this manual.
- **The SF3000 stock backup**, for the right model (next section).
- About 30 minutes.

## 1. Which SF3000 is it?

There are three SF3000 models, and each needs its own files:

| Model | How to tell | Stock backup | Install folder |
|---|---|---|---|
| **SF3000** | No HDMI port | [SF3000_sdcard.7z](https://github.com/Q-ta-s/q-ta-s.github.io/releases/download/sf3000/SF3000_sdcard.7z) | `install_first/sf3000` |
| **SF3000 HD** | Has an **HDMI port** | "SF3000 HD / v1.1" backup on [Q-ta-s's releases page](https://github.com/Q-ta-s/q-ta-s.github.io/releases) | `install_first/sf3000hd` |
| **SF3000 HD without HDMI** | Sold as "HD", but has **no HDMI port** | [SF3000HD_without_HDMI_stock.7z](https://github.com/tzubertowski/H.OS_stock_backup/releases/download/stock-backups-v1/SF3000HD_without_HDMI_stock.7z) | `install_first/sf3000hd_nohdmi` |

If you can't tell which it is, start with plain **SF3000**. Some SF3000s
are built like the newer SF3500 inside; ShallotsOS detects those on its
own and switches to the right driver.

## 2. Back up the card first

The card belongs to your neighbour and may hold their games and saves.

1. Put the card in the computer.
2. Copy **everything** on it into a folder on the computer, for example
   `SF3000 original card`. Keep this copy until you are sure everything
   works: it is also how you undo the install.

## 3. Prepare the card

ShallotsOS must go on a **clean card set up from the stock backup**, not on
top of the factory system: installing over it causes missing sound, wrong
buttons and crashes.

1. **Format the card as FAT32.**
   - Cards of 32 GB or less: in Windows, right-click the card's drive >
     Format > File system **FAT32** > Start.
   - Larger cards: Windows doesn't offer FAT32, so use a free tool such as
     **guiformat** ("FAT32 Format", from ridgecrop.co.uk): pick the card's
     drive letter, then Start. **Double-check the drive letter**: formatting
     erases the whole drive.
2. Open the stock backup `.7z` with 7-Zip and **copy everything inside it to
   the card's root** (the top level of the card, not inside a folder). If
   the backup contains a single folder, copy what is inside that folder.

## 4. Copy ShallotsOS

1. Open `ShallotsOS_dev-2bc1497.zip` with 7-Zip. Inside is a folder named
   **`release`**.
2. Copy everything inside `release` to the card's root, **except the
   `install_first` folder**. Say yes when Windows asks to replace files.
3. Open `release/install_first/`, then the folder for your model from the
   table in step 1 (for example `sf3000`). Inside is a `cubegm` folder:
   copy it to the card's root and **replace** files when asked.
   This step is what makes the console start ShallotsOS.
4. **Recommended for this test:** create an empty file named `log.txt` at the
   card's root (in Windows: right-click > New > Text Document, name it
   `log.txt`, and make sure it isn't `log.txt.txt`). It switches on the
   detailed logs that help fix problems.

## 5. Add games

- Copy game files into `roms/<system>` on the card, for example
  `roms/gba` for Game Boy Advance, `roms/snes` for Super Nintendo,
  `roms/ps1` for PlayStation. The folders are already there.
- Some systems need BIOS files (for example PlayStation); they go in
  `cubegm/bios`. ShallotsOS includes no games or BIOS files.
- If the original card had games, you can copy its game folders back from
  your backup.

## 6. First start

1. Put the card in the console and switch it on.
2. The console's own logo appears, then ShallotsOS's menu. The **first start
   can take a little longer**: ShallotsOS prepares the display and sound
   driver from the console's own files (nothing from the stock system is
   changed).
3. Try the menu, start a few games, and open the Settings.

Buttons in a game: **SELECT + START** opens the game menu (save, load,
options, quit).

## What works differently on the SF3000

The SF3000's screen is mounted sideways inside the console, and some
ShallotsOS extras don't support that yet:

- The **volume popup** is the console's original one, not ShallotsOS's.
- During an **update**, the screen shows the original progress bar instead
  of the ShallotsOS update screen.
- **Apps that draw their own screens** (like *Save Backup* in the Apps tab)
  can't show anything yet and close straight away.
- There's **no Shutdown entry** in Apps: use the power switch.

## What to report

Please note anything that looks or works wrong, with photos if you can:
menus, game speed, sound, buttons, the screen being cut off or the wrong
way round. Then send these files from the card (with `log.txt` in place):

- `cubegm/logs/zhijack.log` (and `zhijack.log.prev` if it exists)
- `cubegm/logs/shallots_setup.log`
- `cubegm/logs/last_game.log` after a game misbehaves
- the `logs` folder at the card's root

Also say which model it is (step 1) and which install folder you used.

## If it doesn't start

- **Stuck on the console's logo, or a black screen:** switch off, take the
  card out and check that step 4.3 was done with the folder for the right
  model. Then send the log files above.
- **No sound, or the picture looks wrong:** try the install folder of
  another model (step 4.3 again), and tell us which one worked.
- **To undo everything:** format the card again and copy back your backup
  from step 2.

## Updating later

New versions come as an `update.zip`: copy it to the card's root and switch
the console on. It installs itself and restarts; settings and games are
kept.
