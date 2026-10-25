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

- https://github.com/MarsInsanity/ShallotsOS/actions/runs/40221519715
  (ShallotsOS `b7aa0f0`, picoarch `fa4846f`)

## User to-do (device tests)

1. Install the latest test build (link above), replacing files when asked.
2. Load > Auto preview after leaving a game once (Auto save & load on).
3. Lag: keep `log.txt` at the card root. When the lag starts, keep playing
   ~15 s, then open the menu and go back to the game, play ~15 s more and
   send `logs/picoarch.log`.
4. Performance baseline: play Yoshi's Island ~30 s with the Bilinear
   filter, then ~30 s with Sharp (Options > Audio and video), rewind off;
   same for a PS1 game; note which PS1 core (ps1r / qpsx) and send
   `logs/picoarch.log`.
5. gamelist.xml: scrape a few systems with Skraper (Recalbox front-end),
   check box art, then turn on Settings > General > Names from gamelist.xml.
   Note how long big folders take to open the first time.
6. Music volume bar: send the `VIDEO_PLAYER:` lines from `/mnt/sdcard/log.txt`
   (the file at the card root, not the one in `logs/`).
7. Books (needs `cubegm/ebook` from TreeFrogUI), Videos and Images folders.
8. When convenient: low-battery save at 5%.

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
- [x] `background.png` turned 180° on load like Onion (Miyoo themes store it
      upside down: 50 of the 56 picture backgrounds in the official theme
      collection); fixes e.g. BMO Noire's mirrored bottom bar. The ShallotsOS
      theme's background is stored the Miyoo way too
- [x] Game Boy Color fallback console icon (Cosy GBC), and the overlay now
      ships `frogui/system-icons/` for themes without icons (e.g. Aubergine)
- [x] Consoles without FN (SF3000, SF3500, GB350): START opens and closes the
      GameSwitcher in the menu; In-game Menu Keys skips FN and defaults to
      SELECT+START (picoarch falls back the same way); the power-button watch
      runs only on the R36SX family (SF3000 has a real power switch). Checked
      at 854x480 in the host build; picoarch builds with the SF3000 toolchain
- [x] Home tiles placed like MainUI: centred on their slot at y = 230 (from
      theme previews), so large scene images (BMO Noire) line up; plus
      Settings > Library > Show Recents on Home
- [x] In-game Load: Switcher keeps the last GameSwitcher / power-off state
      (a `.resume` marker now decides resuming); Auto and Switcher are only
      offered when they hold a state
- [x] Music player: level slider checked every 20 ms and repainted every
      33 ms so cubevol's stock slider barely shows
- [x] Media apps show every folder with files (root first, then e.g.
      `roms/music` as a folder entry)
- [x] FPS / CPU HUD drawn as a TTF card like the toast
- [x] Load > Auto: the menu no longer re-saves Auto when it opens, so Auto
      is the state from when the game was last left; Auto and Switcher are
      always listed again ("Empty" when unused)
- [x] FPS / CPU HUD hidden while the in-game menu is open
- [x] Music player: the HUD moves to fb1's second page when the plane has
      one, so cubevol's slider lands on the hidden page (logged as page2=1)
- [x] In-game menu drawn at panel resolution (640x480 / 854x480) and sent
      straight to the panel like FrogUI, sizes doubled so the layout is the
      same; glyph cache raised to 64 px. Checked with a host harness that
      runs menu.c
- [x] Load > Auto preview: the final auto-save captures the game frame first
- [x] Music player: fb1 raised to two pages when its memory allows (log
      line "virtual=a/b mem=.. page2=.. resized=..")
- [x] Auto preview: capture falls back to the last good menu capture when
      the frame comes out black (logged as "DBG capture: ...")
- [x] Lag report in logs/picoarch.log every 5 s of play ("DBG lag: fps,
      frame max, present avg/max, audio queue, SDL queue, CPU governor/freq")
      to find the intermittent input lag that the menu clears
- [x] gamelist.xml / miyoogamelist.xml (`frogui/gamelist.c`): `<image>` /
      `<thumbnail>` as box art first, `<name>` with Settings > General >
      Names from gamelist.xml (off by default) in lists and the
      GameSwitcher; relative and foreign absolute paths; entities/CDATA;
      parsed once per file change, 6 LRU lists in memory (6,000 games /
      6.7 MB: 14 ms on the host); scraper folders (media, Imgs...) hidden
      from game lists; artwork miss cache keyed on the gamelists too.
      Recents list names and `<desc>` not yet
- [x] Draft of every component's open-source license in README.md
      ("License & Attribution" tables) and LICENSE.md section 4; awaiting the
      user's review of the open points
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

- [x] Settings > Library > In-game Menu Keys picks one layout (the others
      are off): **FN** (tap = menu, hold = GameSwitcher; SELECT+START also
      opens the menu), **Hold SEL/START** (hold SELECT = menu, hold START =
      GameSwitcher, taps reach the game), **SEL+START** (tap = menu, hold =
      GameSwitcher). SELECT+L1 stays the screenshot shortcut
- [x] Hold and SEL+START layouts hold SELECT/START back from the game until
      decided: taps are replayed on release, combos pass on, menu presses
      never reach the game (first test: Kirby lost its power-up and the menu
      wouldn't open). Right-stick drift (arrives as A/B/X/Y) no longer turns
      a hold into a combo. Hold layout: SELECT+START held 1 s = menu (way out)
- [x] Hold layout: SELECT+L1/R1/L2/R2 (screenshot, FF, load, save) keep
      SELECT from the game (Kirby lost its power-up on SELECT+R2)
- [x] Device test: all three layouts work; FN+volume doesn't trigger FN

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

- [x] Device log: the firmware's sleep never suspends the kernel ("suspended
      0 ms"), it only pauses the game, so it drains the battery like the
      Miyoo Mini's; waking leaves the stock battery icon up and the game slow
      (audio underruns). No LED device exists (`/sys/class/leds` empty, no
      LED device-tree node), so the front lights can't be switched by software
- [x] **ShallotsOS Sleep** (Settings > System, the old "Disable Sleep" key,
      default on): a short power press pauses, mutes, turns the backlight off
      and slows the CPU; the next short press wakes and restores exactly that.
      After 5 minutes asleep it saves the game (Resume After Power-Off) and
      powers off, which also stops the LEDs. Works in games and the menu
- [x] **Sleep Power-Off** (Settings > System): 10 s, 30 s, 1, 2, 5 (default),
      10, 15 or 30 minutes; greyed out when ShallotsOS Sleep is off
- [x] Firmware-sleep wake restore (CPU, OSD, audio) kept for when ShallotsOS
      Sleep is off
- [x] Device test: ShallotsOS Sleep works (no battery icon, no lag on wake)
- [x] Front LEDs (blue, red, green between SELECT and START): not software
      controllable. Extended report maps every output pin: bank 0 pin 6 =
      `lcd-reset-pin`, 8 = `key-clk`, 9 = `key-do` (button matrix), 19 =
      `speaker-output`; pin 14 = `power-button` (confirms the power watch).
      No LED or PWM is in use for them, so they're on the power/charging
      hardware. Sleep Power-Off is the way to turn them off
- [x] Post-boot message after an automatic power-off ("Powered off after 5
      min asleep" / "Powered off: battery low"): the resumed game shows it
      with ", game resumed", otherwise the menu does (for about 4 s)
- [x] OnionOS-layout toasts drawn above the hint bar (they were cut off)
- [x] In-game messages (power-off note, low battery) drawn as a toast on the
      OSD plane at panel resolution (the HUD text was scaled and cut off)
- [ ] Device test: in-game toast after a sleep power-off

### File cache

- [x] The file cache refreshes itself after ROMs are added or removed:
      once per boot, FrogUI compares the names in `roms/` and each system
      folder with the last boot and clears stale listings (and remembered
      missing artwork). It doesn't rely on folder dates, which Windows
      doesn't update on FAT cards.
- [x] Also re-checks after USB mode, which returns to FrogUI without a reboot

## In progress

User's to-do list (October 2026):

- [x] Emulator menu laid out like Miyoo's: game behind, dark left panel,
      full-width highlight bar in the theme colour, A/B hint bar; theme font
      (Exo 2) in mixed case with an OnionOS theme
- [x] Options pages and Player/Emulator controls drawn in the same style
      (values on the right with arrows, scrolling with a counter), with a
      status bar (game name, room for the battery icon)
- [x] Slot/value arrows pointed inwards; fixed
- [x] Device test: menu look, theme font (Exo 2, mixed case), status bar,
      save/load slots and the toast all confirmed
- [x] 30 user save slots plus **Auto** (Auto-Save, `.st9`) and **Switcher**
      (GameSwitcher / power-off, `.st10`) in Load; Save offers 1-30 only.
      Slots 1-8 keep `.st0`-`.st7`, 9-30 are `.st11`-`.st32` (no migration).
      Resuming or loading Auto/Switcher no longer makes them the SELECT+R2
      save slot
- [ ] Device test: 30 slots, Auto and Switcher in Load
- [ ] Theme font now loads (fallback to ShallotsOS's Exo 2 added). Check the
      `DBG menu: font ...` log line: confirms whether other themes' fonts
      come through skin.txt or only the fallback is used
- [x] Save/Load slots switched with Left/Right in the menu row (SpruceOS
      style), background shows the slot's screenshot or "Empty"; A saves or
      loads and returns to the game with a toast
- [x] Music/video player draws its own volume/brightness slider (it owns the
      OSD plane and repainted over cubevol's, cutting it off); the overlay
      now builds and ships `cubegm/video_player`
- [x] Media folders at the card root: `Music/`, `Videos/`, `Books/`,
      `Images/` (also `Media/...`, `Audio`, `Movies`, `Ebooks`, `Photos`,
      `Pictures`), then the old `roms/...` ones. Created at boot when missing
      and the player is installed; included in the new-files check
- [x] Books app (the MuPDF reader, `cubegm/ebook`, from TreeFrogUI) listed
      as "Books" in Apps
- [ ] Device test: all of the above; Books needs `cubegm/ebook` on the card


## Next

- [ ] Test on the device: FN tap/hold, FN+volume, suspend/resume, fake08
      Auto-Load, GameSwitcher looks and keys, Options highlight, new ROMs
      appearing, power-off save and boot resume
- [ ] Waiting on the user: mockups for media apps and play tracking
- [ ] Emulator performance (user priority, next big task; asked again
      October 2026, PS1 a priority too):
      - [x] Measuring: the DBG lag report (every 5 s) now has run time per
        frame, the present's share, emulation time (emu~), the scale
        filter and whether rewind is on
      - [ ] Baseline logs from the user: Yoshi's Island and a PS1 game,
        each with Bilinear and with Sharp, rewind off
      - Yoshi's Island (SNES, SuperFX2) is slow on ShallotsOS but fine on
        stock. Measure FPS on both; check the Sharp filter's software
        scaling cost (the user's games run with Sharp); compare
        snes9x2005_plus / snes9x2002 / snes9x2010 and the stock emulator;
        review core build flags, frameskip, CPU clock and present path
        using the DBG lag report
      - Same pass for other heavy systems (PS1 via ps1r / qpsx, GBA, etc.)
      - PS2: not possible on this hardware (told the user)

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
