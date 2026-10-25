# ShallotsOS checklist

The running list of what is done, in progress and planned. It is updated
whenever work starts or finishes.

Repositories and branches:

- **ShallotsOS** (FrogUI menu, themes, overlay build):
  `MarsInsanity/ShallotsOS`, branch `claude/treefrogui-onionos-themes-5391yd`
- **ShallotsOS_picoarch** (emulator frontend, in-game menu):
  `MarsInsanity/ShallotsOS_picoarch`, branch `claude/gameswitcher-fn-hold`

Device: R36SX v2.7 on the R36SX v2.6 firmware (`install_first/r36sx/`).

## Done

### OnionOS look and themes (ShallotsOS)

- [x] Vendor FrogUI into the repository
- [x] OnionOS theme loader: `config.json`, `skin/`, `icons/`, `icons/sel/`,
      `icons/app/`, `sound/change.wav`, theme folders and `.zip` files in
      `Themes/`, profile overrides, fallback skin
- [x] Theme fonts (FreeType + HarfBuzz), with fallbacks for Onion's font names
- [x] OnionOS screens: home tiles, console grid, lists, header and footer,
      battery, previews, favourite marks, folder icons
- [x] OnionOS-style settings (A toggles, B leaves); theme picker in
      Settings > Appearance > OnionOS Theme
- [x] Default "ShallotsOS" theme and its generator script, plus the Exo 2 font
- [x] Theme asset test (`tests/test_onion_theme.sh`) and docs
- [x] Classic (non-Onion) UI left pixel-identical to upstream
- [x] GitHub Actions overlay build (artifact `ShallotsOS-overlay`)

### GameSwitcher behaviour

- [x] GameSwitcher is separate from Recents; FN in the main menu opens and
      closes it (OnionOS theme with Game Switcher on)
- [x] Leaving a game returns to the GameSwitcher
- [x] In game: tap FN = emulator menu, hold FN = suspend to GameSwitcher
- [x] Holding FN saves to a hidden slot 10. Launching the game resumes from
      it once. Leaving through the menu discards it. The menu still shows
      slots 0-7, and Auto-Save uses slot 9.
- [x] FN shortcuts are safe: an FN press that is used with another button,
      or that changes volume or brightness (FN+volume), opens neither the
      menu nor the GameSwitcher (in game and in the main menu)

### Emulator frontend (ShallotsOS_picoarch)

- [x] Auto-Save/Auto-Load fixed for cores that load the game late (fake08):
      the state is restored a few frames in
- [x] GitHub Actions build of `picoarch` and `picoarch_hi` (artifact `picoarch`)
- [x] Build fix: `g_switcher_enabled` used before its definition
- [x] Options submenus and Save/Load State lists use the theme's highlight
      colour, like the main menu (the white bar hid the selected item)

### GameSwitcher look (matches OnionOS's gs_render.h)

- [x] Full-screen capture, header with play time ("2h 3m / 8h 53m") and
      battery, 60 px game-name bar at 75% black with arrows
- [x] Theme `extra/gs-top-bar` / `extra/gs-bottom-bar` replace the header and
      footer (bottom bar drawn as-is); without them, the normal header and
      footer. Arrows `extra/arrowLeft/Right`; `extra/gs-legend` shown for 5 s
- [x] Y: normal/minimal view, hold Y: fullscreen; X: remove from history
      (with prompt); SELECT: cycle header; START or B: main menu. The view
      and header choice are saved
- [x] ShallotsOS theme: logo in the top bar and button legend in the bottom
      bar, like Onion's default theme; bars widen cleanly on 854 px panels

### File cache

- [x] The file cache refreshes itself after ROMs are added or removed:
      once per boot, FrogUI compares the names in `roms/` and each system
      folder with the last boot and clears stale listings (and remembered
      missing artwork). It doesn't rely on folder dates, which Windows
      doesn't update on FAT cards.

## In progress

- [ ] Confirm the picoarch CI build passes and produces the artifact
- [ ] Save on shutdown and resume on boot (OnionOS-style): powering off in a
      game suspends it, and the next boot resumes it
- [ ] Check that the file cache also refreshes after USB mode (the card can
      change while the console is on)

## Next

- [ ] Test on the device: FN tap/hold, FN+volume, suspend/resume, fake08
      Auto-Load, GameSwitcher looks, Options highlight, new ROMs appearing
- [ ] Waiting on the user: mockups for media apps and play tracking

## Ideas offered, not started

- [ ] GameSwitcher hints and theme picker previews
- [ ] Search tile on the home screen
- [ ] Boot screen
- [ ] Themed in-game menu (closer to Onion's)
- [ ] Expert/RetroArch tile
- [ ] Photos, videos and music apps, and a nicer play-activity app

## Install (from Windows)

1. Download the `ShallotsOS-overlay` artifact from the ShallotsOS Actions
   run and copy its contents to the SD card root.
2. Download the `picoarch` artifact from the ShallotsOS_picoarch Actions run
   and copy `picoarch` and `picoarch_hi` into `cubegm/` on the card.
