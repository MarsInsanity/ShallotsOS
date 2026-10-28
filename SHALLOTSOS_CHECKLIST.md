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
  ShallotsOS_picoarch (branch `claude/gameswitcher-fn-hold`) and the
  HCSNES cores from MarsInsanity/HCSNES (`main`; after an HCSNES-only
  change, start it with "Run workflow", which has an `hcsnes_ref` input).
- It runs on every ShallotsOS push to `claude/**` that touches FrogUI, the
  theme assets or the workflow. After a picoarch-only change, start it with
  "Run workflow" (it has a `picoarch_ref` input for another branch).
- ShallotsOS_picoarch keeps its own "Build picoarch" check for picoarch pushes.

Latest test build:

- https://github.com/MarsInsanity/ShallotsOS/actions/runs/40475842078
  (ShallotsOS `8740e53`, picoarch `9ed8e9a`, Books reader `7e6c60e`, PCSX4ALL `75967a2`, HCSNES 0.4 `84ce531`)

## User to-do (device tests)

Build: https://github.com/MarsInsanity/ShallotsOS/actions/runs/40475842078
Before: back up the SD card. For feel tests remove `log.txt` and the
stockprobe (uninstall_stockprobe.bat); put `log.txt` back only when a test
asks for logs.
Confirmed earlier: folder move, NES save states, Resume, GameSwitcher,
Recents, Favourites, Y menu, box art, FN mode passes SELECT+START, Load >
Auto preview, boot-script poll fix (other processes 25% -> 8% of the CPU),
OnionOS search keyboard, ShallotsOS volume indicator (first build), PS1 4:3
aspect, quick launch/back, Themes app, picoarch and PCSX4ALL About screens,
PCSX4ALL buttons, EPUBs open, no stock bar in Music.
Merge (v1.8.0_q) on device: menu works as before at a first look (M1);
no big visible changes (M2); Picante8 is the PICO-8 default (M3).
Snes9x-Z and the other new cores arrive with the full package.
Results so far (timed frameskip build): Yoshi's Island on Snes9x 2005 much
better, crackle only at scene changes (intro, cutscene end, map); Neo Geo
(Magical Drop 3) full speed and quicker to open; GBA fine on gpSP (default),
mGBA unplayable; Crash: HCPS1 still crackles and feels sluggish, PCSX4ALL
(default) very playable but not full speed.

A. New in this build: merged with TreeFrogUI v1.8.0_q (FrogUI e3a5b94,
   picoarch f77a0d5). Your card's TreeFrogUI base files stay as they are;
   the new launcher script (zhijack.sh) only comes with a full install.
M1. Menu basics after the merge: OnionOS home, Games/Favorites/Recents/Apps/
    Settings, theme switch, search (X), Y menu, launch a game and back,
    GameSwitcher. Anything that looks different from the last build?
M2. New from TreeFrogUI: screenshot shortcut in the menu; selection pill
    animation in picoarch's menus (Y in a game > Options: the highlight
    slides; long option names keep scrolling slowly by themselves).
M3. New systems in the Y menu / Games: Snes9x-Z as a SNES emulator,
    Picante8 for PICO-8 (now default), PocketStation (pokket), OpenBOR;
    the cores themselves come from a TreeFrogUI update, not this overlay.
M4. Ports: a roms/ports folder (TreeFrogUI Ports Collection) browses as
    one Ports system; a port with files missing shows what is missing.
0. Driver check: done. The R36SX card's own driver.so is TreeFrogUI's
   driver_r36sx.so byte for byte, so an R36SX on the v2.6 firmware can
   supply its own driver for a full ShallotsOS install.
1. Power off (hold power) from the menu and from a game: the stock power-off
   screen shows. Now watches both of cubevol's fb1 mappings and counts a
   grid of points. If it still doesn't show: with log.txt, send
   logs/shallots_osd.log after a power-off. Also plug in USB once.
2. Volume: hold volume up/down in a game and in the Music app: the popup /
   the Music slider follows step by step. Volume a few steps down, then
   switch screens: no false "no sound".
3. HCSNES 0.4 (Timed, cap 3 now default) on Yoshi's Island: lows/spikes vs
   Snes9x 2005. Optional: a 60 s Profiling On run (HCSNES PROFILING.md).
4. Aspect: SNES/NES/Genesis on Native fill the screen (4:3); 8:7 and 16:9
   change the shape.
5. Books (new reader, cubegm/ebook, from MarsInsanity/ShallotsOS_ebook_reader):
   - pages no longer stretched vertically; text sharp;
   - footer: chapter, page, percent; SELECT menu in the theme's colours;
   - button hints bar when a book opens (START shows/hides it);
   - DOWN/UP next/previous chapter; X contents list;
   - Y bookmark on/off (ribbon top right); menu > Bookmarks, X deletes;
   - change Text size: the same text stays on screen; position kept after
     quitting (SELECT+START) and reopening.
   If the overlay has no cubegm/ebook, the reader build failed: tell me.
B. SNES feel (no log.txt, no probe)
6. Stock Snes9x 2005 is now better than the old ShallotsOS SNES (minimal
   crackle, visible frameskip). Found: Filter = Nearest is slower than
   Bilinear (also Kirby's Adventure on NES): Nearest scales on the CPU,
   Bilinear on the display hardware. Use Bilinear until that is fixed.
C. Not yet confirmed
7. Games tab: FN (GameSwitcher) in a game list, B, then Games: the systems
   list again, on that system. SELECT in the Games list does nothing.
8. FPS counter (PCSX4ALL, and ReARMed/HCPS1 in picoarch): Crash Bandicoot
    with Show FPS on: "FPS: 30.0 (60)" no longer squeezed sideways.
9. GB/GBC and PS1 saves and states after the folder move; Activity Tracker
    play time.
10. Videos, Images apps; low-battery save at 5% when convenient.
D. Later
11. Scraping: names and art on all systems; first open of a big folder.
12. Kirby lag: if it happens, 15 s play, menu and back, 15 s more; send
    logs/picoarch.log (with log.txt).
13. Activity Tracker mockup (from you).

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
      Names from gamelist.xml (on by default) in lists and the
      GameSwitcher; relative and foreign absolute paths; entities/CDATA;
      parsed once per file change, 6 LRU lists in memory (6,000 games /
      6.7 MB: 14 ms on the host); scraper folders (media, Imgs...) hidden
      from game lists; artwork miss cache keyed on the gamelists too.
      Recents list names and `<desc>` not yet
- [x] Recalbox folder structure (`frogui/systems.c`, 65 systems with
      Recalbox names, full names and emulators, default first), FrogUI and
      OnionOS icons and names for them, Games list sorted by display name
- [x] First-boot move from TreeFrogUI's folders (`frogui/migrate.c`):
      prompt (A move, B power off and ask again); renames only, never
      deletes; splits mixed folders by extension (.gbc out of gb, sega into
      megadrive / mastersystem / gamegear); saves, save states, screenshots,
      cheats and art follow each game; Recents, Favourites, play time,
      emulator choices and resume files rewritten; alternate-emulator
      folders become per-game choices (nesq = QuickNES); name clashes stay
      put; empty TreeFrogUI folders removed. Tested on a host card copy
- [x] Y menu (OnionOS / SpruceOS style): Search, Emulator (Left/Right),
      Apply to This game / Whole system, Add/Remove Favourite; Y on a system
      sets its emulator; launch uses game choice, then system choice, then
      the default; standalone emulators allowed as choices; Y hint now
      "Y-MENU"; override list raised to 2048 entries
- [x] Y menu fix: on a game it opens on "This game" (it opened on "Whole
      system" when the system had a choice, so a game change overwrote the
      system's); "Default (<system's current emulator>)" for a game
- [x] Skraper step-by-step guide: docs/scraping.md
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
- [ ] Device test: all of the above; Books uses ShallotsOS's own reader now
      (MarsInsanity/ShallotsOS_ebook_reader, built into the overlay)

Full ShallotsOS install (no TreeFrogUI install first):

- [x] `scripts/shallots_package.sh` builds `ShallotsOS_<version>.zip` on the
      public TreeFrogUI v1.8.0_q full ZIP (sha256 checked): stock drivers and
      `picoarch.cfg` removed, overlay on top, every
      `install_first/<device>/cubegm/zhijack.sh` from ShallotsOS's template,
      the ShallotsOS boot logo for 640x480 consoles, a minimal
      `frogui/settings.txt` (disable_sleep=on, filter=bilinear),
      `INSTALL-ShallotsOS.md`
- [x] `update.zip` in TreeFrogUI's updater format: never carries
      `setting.xml`, a stock driver, `frogui/settings.txt`, `keymap.txt`,
      `picoarch.cfg` or PCSX4ALL's cfg, so an update keeps them (tested with
      `tfupdate.sh` on a simulated card)
- [x] `cubegm/shallots_setup` makes `driver_<device>.so` on the first boot
      from the console's own driver (card `driver.so` or the SF3500-class
      decrypted copy); R36SX also gets `driver_r36sx27.so`; existing files
      are never replaced; log `cubegm/logs/shallots_setup.log`
- [x] Overlay workflow uploads `ShallotsOS-full` and `ShallotsOS-update`
      artifacts (no release is published)
- [x] First package build: run 40479764817 (ShallotsOS f554af6), artifacts
      `ShallotsOS-full`, `ShallotsOS-update`
- [x] Device test on the R36SX: full install works
- [ ] Device test: update.zip over a full install
- [x] ShallotsOS defaults: Animations off, ShallotsOS volume indicator on,
      stock battery indicator off, names from gamelist.xml on (only when
      `frogui/settings.txt` has no saved value)
      Build: run 40545889351 (ShallotsOS 534ac46)

USB debug shell (live debugging over USB-C):

- [x] `u_serial.ko` / `usb_f_acm.ko` built in CI for the stock
      4.4.186-release kernel (TreeFrogUI's mass-storage recipe; vermagic
      checked against its module)
- [x] `cubegm/usb_debug.sh`: CDC ACM gadget (a COM port on Windows) with a
      root shell; `logs` helper; log `cubegm/logs/usb_debug.log`
- [x] Settings > System > USB Debug Shell (off by default), starts at once and
      at boot; USB mode stops it first
- [x] Build: run 40570141467 (ShallotsOS 7d95fb8); both modules built,
      vermagic `4.4.186-release preempt MIPS32_R2 32BIT` (matches stock)
- [ ] Device test: modules load, COM port appears, PuTTY shell works while a
      game runs; MTP still works after it

GBA: gpSP (latest):

- [x] TreeFrogUI's `gpsp_libretro.so` is upstream libretro/gpsp `2db57b1`
      (4 June 2026), 23 commits behind (I/O read semantics and VRAM-execution
      fixes with MIPS dynarec changes, EEPROM detection, sound mixing)
- [x] The overlay builds current upstream gpSP (`gpsp_ref` input, default
      master) as `gpsp_latest_libretro.so`, "gpSP (latest)" in Y > Emulator
      for GBA, next to the pinned gpSP; saves are shared (same ROM folder)
- [x] Build: run 40590128968 (ShallotsOS bb9a7a7, gpSP upstream 5819380)
- [ ] Device test: speed vs gpSP; Hamtaro: Ham-Ham Heartbreak and GTA
      Advance (symptoms on the old gpSP still to be described)

Upstream watch:

- [x] `.github/workflows/upstream-watch.yml` + `scripts/upstream_watch.py`:
      monthly report of new commits in `upstream-watch.txt` projects
      (monthly tier: TreeFrogUI, gpSP, PCSX-ReARMed, PicoDrive, GPGX, Geolith,
      snes9x-Z, PICO-8 cores, Ardens, DSperate; quarterly tier in Jan/Apr/Jul/
      Oct), flagging MIPS-code changes and fix/speed titles; run summary,
      artifact, and a comment on an "Upstream watch" issue if Issues are on
- [x] First run: 40590707277 (all 27 projects read, no errors)
- [ ] The monthly schedule starts once the workflow is on `main` (GitHub runs
      schedules from the default branch); until then it runs on pushes to it

Box art from scrapers:

- [x] When the gamelist's `<image>` / `<thumbnail>` file is missing (e.g. a
      deleted `media/mix`), the menu now also looks for `<rom name>.png/.jpg`
      in `media/images`, `media/box2dfront`, `media/boxart`, `media/covers`,
      `media/thumbnails`, `media/screenshots`, `media/mix`; these folders also
      refresh the missing-art cache
- [ ] Device test: scraped `media/images` show with `media/mix` deleted

Update screen:

- [x] "Updating ShallotsOS" screen while update.zip installs: theme
      background, Exo 2 text (`tools/make_update_splash.py`, rendered in the
      overlay build as `cubegm/update_splash.bmp`), live progress bar and
      percentage drawn by `shallots_osd --splash` on fb1; `tfupdate.sh` writes
      `/tmp/tfupdate.progress` (unpacking 0-40, checking 40-50, installing
      50-97, done 100); zhijack starts it before the updater and stops it
      after; skipped on rotated panels (SF3500-class)
- [ ] Device test: shows from the update *after* the one that installs it
      (the installed launcher starts it)


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
      - [x] Performance defaults (picoarch overrides): Frameskip **Auto**
        (skips drawing only while a game falls behind) for snes9x2005 /
        _plus, snes9x2002, gpsp, picodrive, PCE Fast and PCSX ReARMed;
        PCSX ReARMed also threaded SPU on, dithering off (as PCSX4ALL).
        The SF3000 audio ring sits 50-75% full, so "underrun likely" is now
        below 25% (it was 50%, which would have skipped frames in games
        running at speed). PCE Fast overrides never applied (wrong core
        name), fixed. Lag report counts skipped frames
      - Left as is: PCSX4ALL (TreeFrog already ships fast, clean defaults:
        auto frameskip, no dither, reverb and interpolation off, accurate
        cycle rate); Sega CD default stays Genesis Plus GX (TreeFrog's
        choice; PicoDrive is the faster pick in the Y menu)
      - [ ] DS (DSperate): its SF3000 launcher is TreeFrog's own package,
        not in the public repo. DSperate has `frameskip` /
        `frameskip_mode = adaptive` in its INI (`~/.config/dsperate/`, per
        game in `games/`). Need the card's `cubegm/dsperate/run_sf3000.sh`
        to know where its config lives before shipping a default
      - [x] HCSNES (MarsInsanity/HCSNES, another agent's modified
        Snes9x 2005: direct-threaded Super FX, bounded frameskip) added as
        SNES emulators "HCSNES Fast" (old APU) and "HCSNES Plus" (Blargg
        APU), last in the Y menu list; the overlay workflow builds them
        with the SF3000 GCC via its tools/build.py. Its own frameskip
        default is Auto, max 1 in a row. Systems can now list 8 emulators
      - [x] Reviewed HCSNES 0.1 vs snes9x2005 a79dfe9: only Super FX
        dispatch, capped frameskip and the RTC fix differ. Sent the user a
        brief for its developer (in-core profiling to cores.log, build
        flags / PGO, GSU locals, PPU and threading, tests)
      - [ ] HCSNES PS1 core: add the same way when it exists
- [x] User test (October 2026): box art shows; GBC had no icon: the
      shipped settings.txt started on the Pixel by Jeltron pack, and no
      TreeFrogUI pack has gbc.png. Now the pack's GB icon, then
      system-icons; OnionOS themes without GBC art use their GB icon. New
      cards start on Default (Cosy); existing cards change it in Settings >
      Appearance > Icon Pack
- [x] User test: PCSX4ALL has no picoarch menu / GameSwitcher → PS1
      default is now pcsx_rearmed (log: ~10 ms emu/frame, audio healthy);
      PCSX4ALL stays selectable. SNES: Yoshi's Island ~16-18 ms emu per
      frame (budget 16.7) on Snes9x 2005 Plus and HCSNES alike → Auto skips
      ~1/3 of frames and audio runs dry (crackle); 0.5 s stalls were the
      SRAM autosave (fsync + sync on the game thread) → now a background
      thread. Logs no longer fsync; cores' DEBUG lines dropped (mGBA logged
      every DMA, ~40 ms/frame). FN mode no longer opens the menu on
      SELECT+START. cpufreq is not readable on this kernel (cpu= empty in
      the lag lines). DSperate: launcher sets HOME=cubegm/dsperate/data and
      runs --no-audio; FrogUI appends [emu] frameskip = 2 / adaptive to its
      dsperate.ini unless frameskip is already set
- [x] PCSX4ALL (MarsInsanity/ShallotsOS_pcsx4all, branch
      claude/shallotsos-menu-integration): the repo (like TreeFrog's) holds
      only TreeFrog's 27 changed files; base is gameblabla/pcsx4all
      21084ac (UPSTREAM), build_sf3000.sh fetches and overlays it. Menu
      rewritten to match picoarch exactly (verified in a host render),
      multi-size glyph cache, theme font / Exo 2 / GamePocket caps as
      picoarch, frontend.c pages drawn through sf3000_list_draw. Keys,
      switcher (.switcher.sav + .resume, scr.bmp, /tmp/shallotsos_from_game),
      power-off (pin 14 on R36SX, SIGTERM) + resume_on_boot.txt, sleep.
      picoarch's boot resume execs a standalone directly. Overlay builds
      and ships cubegm/pcsx4all (pcsx4all_ref input). PS1 default back to
      PCSX4ALL
- [x] User test: PCSX4ALL's save toast and FPS counter looked nothing like
      picoarch's → toasts now on fb1 exactly as picoarch's fb1_toast (dark
      card centred above the bottom edge, 22 px menu font, 2 s, deferred
      until the menu closes); FPS = picoarch's HUD card "FPS: x (y)" top
      left of the game frame (was the bitmap stats line); Show FPS added
      to Audio and video
- [x] User test (October 2026): FN mode passes SELECT+START to the game
      (confirmed); Load > Auto preview fixed (confirmed). SNES log:
      Snes9x 2005 Plus froze ~520 ms every few seconds while HCSNES Fast
      did not: upstream snes9x2005 Auto frameskip may skip 30 frames in a
      row (HCSNES caps at 1) -> picoarch now clears "behind" after 2
      skipped frames for every core. Heavy scenes still ~16-18 ms emu per
      frame (budget 16.7): crackle needs a faster core (HCSNES work).
      Games tab after GameSwitcher showed a system's games as the root:
      save_games_tab_entries cached the folder list; now only the root is
      cached and the tab returns on that system. TreeFrogUI SELECT core
      picker (and its extension-filter editor) removed; Y menu instead.
      Themes app (Apps > Themes, preview.png full screen). PCSX4ALL face
      buttons swapped vs a PS1 pad (TreeFrog assumed A = south; A is east
      per picoarch's calibrated keymap): Cross->B, Circle->A, config v2
      migration. Music volume bar: fb1 is one page (virtual 480/480,
      resize refused), so page flipping is impossible; next idea is the
      player HUD on fb0 with fb1 blanked during audio-only playback
- [x] Stock SNES cores: TreeFrog's R36HD stock backup
      (tzubertowski/H.OS_stock_backup, stock-backups-v1; the R36SX Google
      Drive backups are blocked here) has the stock rkgame cores in
      cubegm/cores: libemu_sfc.so (Snes9x 2005 v1.36 b94a804, MIPS32, Codescape
      GCC 6.3), libemu_snes9x.so (Snes9x), libemu_snes9x2010.so. Plain
      libretro cores (25-27 retro_* exports, only libc/libm/libstdc++),
      not encrypted, but each runs check_encrypty: mmaps /dev/mem at the
      eFuse block (0x1882E000) and SPI flash (0x1F000000), a genuine-hardware
      check (passes on the real console). libemu_sfc also exports
      SetFrameSkip/Speed (rkgame-only API, unknown semantics) and has no
      snes9x_2005_* options. config.xml lists Snes9x first for .sfc/.smc,
      filelist.xml puts some games on Snes9x 2010. Added all three to the
      SNES Y menu (STOCK() paths, labels "Stock ..."); never libemu_md.so
      (the boot hijack). SH_MAX_CORES 10
- [x] User request: Options > About page (the Y menu cuts long emulator
      names): picoarch e_menu_about (core_lib_name/version, core file,
      game, av_info video/audio, frontend); PCSX4ALL sh_menu_about (CPU
      recompiler/interpreter, disc ID, BIOS/HLE). picoarch option lists now
      cut long values with "..."
- [x] Stock SNES test (user + log): stock Snes9x 2005 draws exactly
      every other frame (fps 30.0, skipped 150 per 5 s, even in light
      scenes: fixed frameskip 1 via its SetFrameSkip) with the old APU;
      emu ~13-15 ms, audio queue 2000-3600 (healthy) vs 0-600 on ours. That
      even 30 fps is most likely the stock firmware's "smooth" Yoshi's
      Island. Stock Snes9x / Snes9x 2010: ~40 ms per frame, removed from
      the Y menu. Recommendation: HCSNES Fast, Frameskip = Fixed, Maximum
      Consecutive Skips = 1 (same behaviour, old APU, no 30-frame runs)
- [x] Search matches and shows gamelist.xml names (when that setting is on),
      each result labelled from its own folder's gamelist; scraper art
      folders (media, Imgs...) are skipped while searching
      - PS2: not possible on this hardware (told the user)

## Ideas offered, not started

- **ShallotsOS Hub** (later; a PC companion app for consoles without WiFi):
  Windows `.exe` built by Actions, talking to the console over USB (MTP) or a
  card reader. Tabs: Ports (TreeFrogUI_PortsCollection releases, already
  MIPS; setup guides for ports needing game data), Free Games (Spruce Game
  Nursery titles that run on our emulators: C64, ZX, Arduboy, GB/GBC/GBA,
  NES, SNES, Freedoom/Doom shareware, Cave Story; only with a license that
  allows it, downloaded from the author or Spruce, never re-hosted), PICO-8
  cart browser (Lexaloffle BBS, into `roms/pico8/` with gamelist.xml),
  public-domain eBooks; later themes, save backup, one-click updates (needs
  published releases). Spruce's own ports are ARM builds and EasyRPG has no
  core yet. Needs a repo such as `MarsInsanity/ShallotsOS_Hub`.

- [ ] Later (user, October 2026): replace the stock firmware's parts and
      start from a mostly blank slate. Agreed order: (1) read-only
      hardware/boot-chain inventory app; (2) own input/system daemon
      replacing cubevol (shift-register keys key-clk/key-do, GPIO volume/
      FN/power, persistentmem volume/backlight, ADC battery, OSD, sleep);
      (3) zhijack.sh stops rkgame/cubevol and runs only ShallotsOS (still
      SD-only, reversible); (4) drop driver.so (rkgame decrypts it) and
      the stock audio path for our own /dev/dis and AUDDEC/I2SO ioctls;
      (5) optional custom rootfs in flash, only with UART recovery and a
      full flash backup. Kernel, HCRTOS firmware and bootloader stay stock
      (request the GPL kernel source). Not now: polish and performance
      first


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
