# ShallotsOS checklist

The running list of what is done, in progress and planned. It is updated
whenever work starts or finishes.

Repositories and branches:

- **ShallotsOS** (FrogUI menu, themes, overlay build):
  `MarsInsanity/ShallotsOS`, branch `claude/treefrogui-onionos-themes-5391yd`
- **ShallotsOS_picoarch** (emulator frontend, in-game menu):
  `MarsInsanity/ShallotsOS_picoarch`, branch `claude/gameswitcher-fn-hold`

Device: R36SX v2.7 on the R36SX v2.6 firmware (`install_first/r36sx/`).

## Test builds (standing rule)

Every time a new build is ready to test, wait for the GitHub Actions run to
pass and give the direct link to it. The artifact is at the bottom of the run
page.

- One download: "Build ShallotsOS overlay" in ShallotsOS, artifact
  `ShallotsOS-overlay` → copy its contents to the SD card root. It holds the
  FrogUI menu, themes, and `picoarch` / `picoarch_hi` built from the latest
  ShallotsOS_picoarch (branch `claude/gameswitcher-fn-hold`).
- It runs on every ShallotsOS push to `claude/**` that touches FrogUI, the
  theme assets or the workflow. After a picoarch-only change, start it with
  "Run workflow" (it has a `picoarch_ref` input for another branch).
- ShallotsOS_picoarch keeps its own "Build picoarch" check for picoarch pushes.

Latest test build:

- https://github.com/MarsInsanity/ShallotsOS/actions/runs/40149721145
  (ShallotsOS `46576a4`, picoarch `b4de93f`)

## User to-do (device tests)

1. Install the latest test build (link above).
2. Create an empty `log.txt` at the SD card root (turns on logging).
3. Boot once (writes the hardware report for the front LEDs).
4. Settings > System > Disable Sleep: off, then restart.
5. Kirby: tap power to sleep, wait 30 s, wake, play a bit. Note whether the
   battery icon or lag come back.
6. Optional: battery % before and after a 30-minute sleep.
7. Low battery (when convenient): play until 5%; it should save and shut
   down, then resume on boot.
8. Try In-game Menu Keys (all three) and the new boot logo.
9. Send `logs/picoarch.log`.

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
- [x] ShallotsOS boot logo replaces the TreeFrogUI one (both panel formats,
      from `tools/onion_theme/make_boot_logo.py`); the overlay ships the
      640x480 one
- [x] GitHub Actions overlay build (artifact `ShallotsOS-overlay`), which
      also builds the latest picoarch so one download has everything

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
- [x] GitHub Actions build of `picoarch` and `picoarch_hi` (artifact `picoarch`), passing
- [x] Build fix: `g_switcher_enabled` used before its definition
- [x] Options submenus and Save/Load State lists use the theme's highlight
      colour, like the main menu (the white bar hid the selected item)

### Settings layout

- [x] Volume moved next to Brightness in General (was under System)

### In-game keys setting

- [x] Settings > Library > In-game Menu Keys: **FN** (tap = menu, hold =
      GameSwitcher), **Hold SEL/START** (hold SELECT = menu, hold START =
      GameSwitcher, taps still reach the game), or **SEL+START/L1**
      (SELECT+START = menu, SELECT+L1 = GameSwitcher). FN and SELECT+START
      keep working in every mode

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

### Save on power-off, resume on boot (picoarch)

- [x] Holding power for 1 s in a game saves to hidden slot 10, flushes
      battery saves and writes `picoarch/resume_on_boot.txt`. The hardware
      cuts power about 4 s in. Reads the power pin (GPIO 14 at 0x18800000,
      the pin `powergpio` drives), read-only, and only when it reads as an
      idle input.
- [x] The next boot resumes that game from slot 10, even with Quick Resume
      off. The marker is removed first, so a broken game can't boot-loop.
- [x] Letting go of power during the firmware's power-off screen keeps the
      save (first try dropped it: the shutdown takes a few seconds). It is
      only dropped if the game still runs 10 s after release, or is left
      normally. SIGTERM from the shutdown also saves. No screen flash.
- [x] Confirmed working on the device (power-off save and boot resume)
- [x] Settings > Gameplay > **Resume After Power-Off** (default on) turns it
      off; Quick Resume stays as the fallback for unexpected shutdowns and
      for the SF3000's power switch
- [x] Low battery: at 5% or less, not charging, for 30 s, save like a
      power-off. Momentary-button consoles then shut down via
      `cubegm/shutdown.sh`; SF3000 re-saves every 2 minutes while low
- [ ] Needs a device test: low-battery save and shutdown

### Sleep (picoarch)

- [x] Waking from sleep re-applies full CPU speed (games lagged on the stock
      frequency policy), hides the stock battery overlay again in games, and
      drops audio queued around the sleep. Also re-checks the CPU policy every
      few seconds
- [x] Logs power presses and each sleep's frame gap vs. kernel-suspend time
      (`logs/picoarch.log` when `log.txt` exists) to tell real suspend from a
      paused game
- [ ] Needs a device test with Disable Sleep off; it stays on by default
      (upstream's choice) until sleep is confirmed solid
- [ ] Front LEDs stay on during sleep: find their control from the boot
      hardware report (LED/PWM/GPIO/device tree in `logs/picoarch.log`),
      then switch them off for sleep (and maybe add an LED setting)
- [ ] Decide on power-off after a long sleep (Miyoo Mini-style) once the log
      shows whether sleep is a real suspend (light) or a paused game (drains)

### File cache

- [x] The file cache refreshes itself after ROMs are added or removed:
      once per boot, FrogUI compares the names in `roms/` and each system
      folder with the last boot and clears stale listings (and remembered
      missing artwork). It doesn't rely on folder dates, which Windows
      doesn't update on FAT cards.
- [x] Also re-checks after USB mode, which returns to FrogUI without a reboot

## In progress

- (nothing right now; waiting on device test results)

## Next

- [ ] Test on the device: FN tap/hold, FN+volume, suspend/resume, fake08
      Auto-Load, GameSwitcher looks and keys, Options highlight, new ROMs
      appearing, power-off save and boot resume
- [ ] Waiting on the user: mockups for media apps and play tracking

## Ideas offered, not started

- [ ] GameSwitcher hints and theme picker previews
- [ ] Search tile on the home screen
- [ ] Boot screen
- [ ] Themed in-game menu (closer to Onion's)
- [ ] Expert/RetroArch tile
- [ ] Photos, videos and music apps, and a nicer play-activity app

## Install (from Windows)

1. Download the `ShallotsOS-overlay` artifact from the latest ShallotsOS
   Actions run (link above) and unzip it.
2. Copy its contents to the SD card root, replacing files when asked.
