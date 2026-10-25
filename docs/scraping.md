# Box art with Skraper

[Skraper](https://www.skraper.net/) downloads box art (and names) for your
whole collection from ScreenScraper. ShallotsOS's ROM folders have Recalbox's
names, so Skraper's **Recalbox** setup fits the card as it is.

Skraper's screens and labels change a little between versions; the settings
that matter are the same.

## Before you start

- Copy your games to the card first, in the right folders (`roms/nes`,
  `roms/snes`, `roms/gba`, `roms/psx`... see the
  [ROM folder table](../README.md#rom-folder-setup)).
- Make a free account on [screenscraper.fr](https://www.screenscraper.fr/).
  Skraper works without one, but it is much slower and gives up more often.
- Put the SD card in your PC (a card reader). Note its drive letter, e.g. `E:`.

## 1. Set up Skraper (first run)

1. **Front-End Selection:** click **RECALBOX**, then **NEXT**.
2. **Where your games are:** choose the local folder option (not a network
   Recalbox) and pick the **`roms`** folder on the SD card, for example
   `E:\roms`. Pick `roms` itself, not the card's root and not a system
   folder inside it.
3. **ScreenScraper account:** enter the account you made, then **NEXT** /
   **FINISH**.

Skraper then lists the systems it found on the left (Nintendo Entertainment
System, Super Nintendo, Game Boy Advance...). Empty folders are left out. A
folder it does not recognise is not listed; check its name against the ROM
folder table.

## 2. Choose what to download

Click **All Systems** at the top of the list on the left, so these settings
apply to every system, then look at the tabs on the right:

- **Gamelist:** leave the gamelist on. It writes `gamelist.xml` in each
  system folder, which ShallotsOS reads.
- **Media / Images:** one image per game is all ShallotsOS shows. Either the
  default image (a mix of box, screenshot and logo) or **Box 2D** for plain
  covers.
  - Keep Skraper's output folder and file name: images go in
    `roms/<system>/media/images/`.
  - **Size:** set the image width (or maximum size) to about **250 px**.
    ShallotsOS shows art at up to 250x200; bigger files only take up space
    and load slower.
- **Videos, manuals, wheels, marquees:** turn them off. ShallotsOS does not
  use them and they take a lot of room.

## 3. Scrape

Press the **play / scrape** button (bottom right) and wait. Large collections
take a while; Skraper can be stopped and started again, and it skips games it
already has.

When it is done, each system folder on the card has:

```text
roms/snes/gamelist.xml
roms/snes/media/images/Super Mario World (USA).png
```

## 4. On the console

1. Put the card back and start ShallotsOS. Box art shows beside the game list.
2. Optional: **Settings > General > Names from gamelist.xml** shows the
   scraped names ("Super Mario World 2: Yoshi's Island") instead of the
   file names.
3. The first time you open a big folder (thousands of arcade games) it can
   take a second while ShallotsOS reads its gamelist; after that it is
   instant.

## If something is missing

- **No art for one system:** check its folder name is a Recalbox one
  (`megadrive`, not `sega` or `MD`) and that `gamelist.xml` is in that
  folder.
- **No art for some games:** ScreenScraper did not recognise them (often
  hacks, translations or renamed files). Rename the file to its official
  name, or put a picture in `roms/<system>/Imgs/<game file name>.png`
  yourself (the game's file name without its extension).
- **Art still old after rescraping:** run **Settings > Rebuild File Cache**.
- The `media` and `Imgs` folders do not show up as games, so leave them
  where Skraper puts them.
