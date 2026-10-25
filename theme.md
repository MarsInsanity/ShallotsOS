# Customising TreeFrogUI

TreeFrogUI supports colour themes, custom fonts, one global wallpaper, per-system backgrounds and game thumbnails.

ShallotsOS adds **OnionOS themes**: the OnionOS look and layout, and support for
the theme format used by OnionOS on the Miyoo Mini.

---

## OnionOS themes

With an OnionOS theme selected, the whole interface uses the OnionOS layout:

- **Home screen**: Recents, Favorites, Games, Apps and Settings tiles with page
  dots, like OnionOS's main menu. Left/Right chooses a tile and A opens it.
- **Games**: the console grid, four systems across and two down.
- **Game lists, Favorites, Apps and Settings**: OnionOS lists with the title bar,
  the selection highlight, toggles and arrows, and the A/B hint bar with a
  position counter. Box art appears on the right of game lists.
- **Recents**: the list of recently played games.
- **Show Recents on Home** (Settings → Library): turn it off to drop the
  Recents tile, like Onion's "Show recents" tweak. Some themes, such as BMO
  Noire, are drawn for a home screen without it.
- **GameSwitcher** (FN in the menu, or hold FN in a game; START in the menu
  on consoles without FN): laid out like
  OnionOS's. The last screenshot of each game fills the screen, the header
  shows play time and the battery, and the game name sits on a dark bar with
  arrows. Left/Right changes game and A resumes it. Y switches between the
  normal and minimal views, and holding Y goes fullscreen. X removes the game
  from history, SELECT changes the header (title, game time, or game and total
  time), and B or START goes back to the main menu. In a game, Settings →
  Library → **In-game Menu Keys** picks how the menu and the GameSwitcher
  open: FN (tap / hold), hold SELECT / hold START, or SELECT+START (tap /
  hold). Only the chosen keys work; FN stays free for system shortcuts in
  the other two. In those two, SELECT and START only reach the game once it
  is clear they aren't a menu press (a quick tap is passed on when released),
  and in the hold layout SELECT+START held for a second also opens the menu.
  Only the R36SX family has an FN button: on the SF3000, SF3500 and GB350
  the FN choice is skipped and SELECT+START is the default.

B at the top of Games, Favorites, Recents, Apps or Settings returns to the home
screen. Search (X), the core and extension picker (SELECT) and favouriting (Y)
work as they do in the other styles.

ShallotsOS ships with the **ShallotsOS** theme selected. To change it, open
Settings → Appearance → **OnionOS Theme** and press Left/Right. Choose **off**
to return to the TreeFrogUI layouts and colour themes.

### Installing OnionOS themes

1. Download a theme, for example from the
   [OnionOS theme catalogue](https://github.com/OnionUI/Themes) (`release/` holds
   one `.zip` per theme).
2. Copy the `.zip`, or the unzipped theme folder, into **`Themes/`** at the root of
   the SD card. This is the same place as on a Miyoo Mini.
3. Select the theme under Settings → Appearance → **OnionOS Theme**. A `.zip` is
   unpacked the first time you select it; the zip itself is left in place.

A theme folder looks like this:

```text
Themes/Silky by DiMo/
├── config.json      ← name, fonts, sizes and colours
├── preview.png
├── skin/            ← background.png, bg-title.png, bg-list-s.png, ic-game-f.png …
├── icons/           ← optional console icons: gba.png, fc.png … (sel/ for selected)
│   └── app/         ← optional app icons
├── sound/change.wav ← optional navigation sound
└── font.ttf         ← optional font named in config.json
```

### What is supported

| OnionOS theme feature | Status |
|---|---|
| `config.json`: `title`, `hint`, `list`, `grid` (`grid1x4`, `grid3x4`), `currentpage`, `total` fonts, sizes and colours | Yes, with OnionOS's fallback rules |
| `config.json`: `batteryPercentage` (`visible`, `size`, `color`, `offsetX`, `offsetY`, `textAlign` / `onleft`, `fixed`) | Yes |
| `config.json`: `hideLabels` (`icons`, `hints`) and the older `hideIconTitle` | Yes |
| Main menu tiles `ic-*-n.png` / `ic-*-f.png`, page dots `dot-a` / `dot-n`, logo `miyoo-topbar.png` | Yes |
| `background`, `bg-title`, `tips-bar-bg`, `bg-list-s`, `bg-list-l`, `div-line-h`, `preview-bg` | Yes |
| `icon-A-54`, `icon-B-54`, arrows, `extra/toggle-on` / `extra/toggle-off` | Yes |
| Battery icons `power-*-icon`, `ic-power-charge-100%` | Yes (see the note below) |
| Console grid cards `bg-game-item-n` / `bg-game-item-f` | Yes |
| GameSwitcher bars `extra/gs-top-bar`, `extra/gs-bottom-bar`, arrows `extra/arrowLeft` / `extra/arrowRight`, legend `extra/gs-legend` | Yes. As in OnionOS, a theme without the bars gets the normal header and footer, and a custom bottom bar is drawn as-is (its button legend is part of the image). |
| Console icons `icons/<name>.png` and `icons/sel/<name>.png` | Yes. OnionOS icon names such as `fc`, `sfc`, `md` and `ps` are matched to the ShallotsOS ROM folders. Systems without a theme icon use the selected **Icon Pack**. |
| App icons `icons/app/<name>.png` | Yes, for the apps ShallotsOS has |
| `sound/change.wav` | Yes, when **Menu Sounds** is on |
| Theme `.zip` files | Yes |
| Profile overrides in `Saves/CurrentProfile/theme/` | Yes |
| `sound/bgm.mp3`, pop-up menus, Wi-Fi and key-tester images | Not used: ShallotsOS has no matching screen or hardware |

Themes are designed for 640×480, the native resolution of the R36SX, R36 HD and
GB350, so they appear exactly as designed on those devices. On 854×480 panels
(SF3000 family) the full-width bars and background stretch to the panel and the
tiles and grid stay centred. The GameSwitcher bars widen through a plain part of
the bar so their logo and button legend keep their shape.

**Fonts.** OnionOS themes often name fonts that OnionOS keeps in
`/mnt/SDCARD/miyoo/app/`. ShallotsOS looks for the font in the theme folder, then
in `miyoo/app/` on the SD card, then in `frogui/onion/fonts/`. If you have
OnionOS's fonts, copy them into `miyoo/app/` on the card. Otherwise
`wqy-microhei.ttc` uses the bundled WenQuanYi Zen Hei, the Helvetica Neue fonts
use DejaVu Sans, and anything else falls back to Exo 2 Bold Italic, which is
OnionOS's own fallback.

**Battery.** The theme's battery icons replace the device's own battery icon
when Settings → Appearance → **Stock Battery Indicator** is off.

**Missing images.** If a theme leaves an image out, the ShallotsOS theme's image is
used, the same way OnionOS falls back to its stock skin. These spare images live
in `frogui/onion/`.

**Settings that do not apply.** With an OnionOS theme selected, Theme, Style,
Background Theme Pack, Background Images, Background Dim, Wallpaper and Center
Text are greyed out: the OnionOS theme sets those itself.

### The ShallotsOS theme

The default theme is a regular OnionOS theme in `Themes/ShallotsOS/`, so it also
works on a Miyoo Mini running OnionOS. Its artwork is generated by
`tools/onion_theme/make_shallotsos_theme.py`; its `preview.png` is a screenshot of
the home screen taken with the FrogUI host renderer (`frogui/tests/host/`).

---

## Box art, game covers and thumbnails

TreeFrogUI can show box art, cover images or gameplay thumbnails beside the selected game. The Recents game switcher prefers box art; when none is available, it uses the latest gameplay screenshot instead.

### Recommended: get box art automatically in three steps

Use **[Mini Scraper for CFW](https://github.com/tzubertowski/mini-scraper-cfw/releases)** to download box art without finding, resizing or renaming every image yourself. Ready-to-run desktop ZIPs are available for Windows x64, Linux x64 and macOS ARM64; the app needs no account and includes everything it needs.

1. Download the latest desktop ZIP for your computer, extract it and run Mini Scraper.
2. Choose the SD card or ROM folder, confirm the detected **TreeFrogUI** format, and leave artwork source option 1 selected: **Automatic (Libretro Thumbnails) · no login**.
3. Press **Add artwork**.

Mini Scraper matches covers to the ROM filenames and writes each image beside its ROM using the filename TreeFrogUI expects. Existing artwork is preserved unless you explicitly choose to replace it.

### Add box art manually

Place each cover image in a hidden `.res/` folder next to the ROM. Match the ROM filename without its extension:

```text
roms/GBA/Advance Wars.gba
roms/GBA/.res/Advance Wars.png
```

Name the image after the ROM filename without its extension, then use a supported image extension: `.png`, `.jpg`, `.jpeg` or `.bmp`.

PNG, JPG, JPEG and BMP images can be any resolution. Images larger than the 250×200 preview area are downscaled automatically with their aspect ratio preserved. PNG transparency is supported. Keep files reasonably small because they are decoded while browsing.

Older headerless `.rgb565` thumbnail sets also work at these fixed sizes: `64×64`, `128×128`, `160×160`, `200×200`, `250×200`, and `200×250`.

In-game savestate and last-screen images are captured automatically and need no manual setup.

---

## Appearance settings

Open **`>> Settings`** at the bottom of the main system list. Use Up/Down to select an option and Left/Right to change it. Press A or B to close Settings and save.

| Setting | What it changes |
|---|---|
| **Theme** | Interface colours |
| **Background Theme Pack** | Selects a folder of themed background artwork |
| **Style** | Chooses Vertical, Horizontal or System View |
| **Icon Pack** | Chooses System View artwork independently from backgrounds |
| **Friendly System Names** | Expands folder codes such as `ps` and `gba` |
| **Font** | Menu typeface |
| **Brightness** | Screen brightness |
| **Animations** | Background crossfades and other UI motion |
| **Battery Colour Mode** | Uses a green, blue or red battery status light |
| **Background Images** | Enables wallpapers and per-system backgrounds |
| **Wallpaper** | Selects one image for every TreeFrogUI screen |
| **Wallpaper Fit** | Controls how the selected wallpaper fills the display |
| **Hide Extensions** | Hides file extensions in game lists |
| **Hide Empty Folders** | Hides system folders with no games |

Settings are stored in `/mnt/sdcard/frogui/settings.txt`.

### Background theme packs

Theme packs live in a folder under `/mnt/sdcard/frogui/theme-packs/`. Select one in
Settings → Appearance → **Background Theme Pack**. Packs use the same filenames
as per-system backgrounds: `main.jpg`, `settings.jpg`, `recents.jpg`,
`favourites.jpg`, or a ROM folder name such as `gba.jpg`. The shipped
`Art_Book_Next` pack preserves the original artwork. The test packs include
per-system artwork adapted from **Elementerial**, **Iconic** and
**PlayStation-X**.

`Art_Book_NextUI` is the updated NextUI resource set adapted from
[Leviathanium's ArtBookNextUI theme](https://github.com/Leviathanium/NextUI-Themes/tree/main/Catalog/Themes/ArtBookNextUI.theme).
It is center-cropped to the supported 854×480 display size and keeps the
original pack available for comparison.

`Nao_Black` is another adapted background set from
[Leviathanium's Nao-Black theme](https://github.com/Leviathanium/NextUI-Themes/tree/main/Catalog/Themes/Nao-Black.theme).

The supplied packs include explicit artwork aliases for the short TreeFrogUI
folder names used on the device (for example `a26`, `m2k`, `pcesgx` and
`ps1r`), so these systems do not fall back to the generic main image.

### Which background is used

| Background Images | Wallpaper | Result |
|---|---|---|
| Off | Any value | Plain background using the selected theme |
| On | None | Per-system and per-screen backgrounds |
| On | An image | That wallpaper on every screen |

A selected wallpaper overrides per-system art. Turning **Background Images** off only hides the images; it does not forget your wallpaper choice.

---

## System list styles

**Vertical** is the standard text list. **Horizontal** is the animated
left/right system carousel. **System View** is a four-column, two-row console
grid inspired by OnionOS. It uses the selected theme colours and L1/R1 for the
top-level tabs. Its background follows the highlighted
system and uses `main.*` as the fallback. Use the D-pad to move between systems;
pages follow the selection automatically.

System View icons live in `/mnt/sdcard/frogui/system-icons/` and are named after
ROM folders, for example `gba.png` and `ps1.png`. Custom folders without an icon
fall back to initials. Extra sets live in
`/mnt/sdcard/frogui/icon-packs/<pack>/`; select them with **Icon Pack** without
changing the colour theme or background pack. Transparent colour PNGs are
supported. The supplied collection credits every Onion pack and creator in
`frogui/icon-packs/README.md`.

### Horizontal system picker

Set Settings → Appearance → **Style** to **Horizontal**. Choose colours
separately with **Theme**; the carousel uses the selected theme.

The main system screen changes to a left/right carousel. System names slide with the selection, wrap at both ends, and the background crossfades to the selected system. Game lists, Recents, Favourites and Settings keep the standard vertical layout.

The carousel uses the same per-system background files described below. It also supports `recents.*`, `favourites.*`, `settings.*` and the `main.*` fallback. Turn **Animations** off for instant movement and background changes.

**Friendly System Names** works in every style. Turn it on to show names such
as **PlayStation** and **Game Boy Advance** instead of the actual `ps` and `gba`
folder names.

---

## File cache

**File Cache** is off by default. Turn it on only if repeated library scans are
slow. **Rebuild File Cache** clears old entries, enables the cache and rebuilds
the main library index immediately. Individual system folders refresh when
opened.

---

## Global wallpaper

1. Copy an image to `/mnt/sdcard/frogui/wallpapers/`.
2. Restart TreeFrogUI so it rescans the folder.
3. Open Settings → Appearance → **Wallpaper** and select the filename.
4. Set **Wallpaper Fit** to suit the image.

Supported formats are `.png`, `.jpg`, `.jpeg`, and `.bmp`. Extensions are matched without regard to case. TreeFrogUI lists up to 63 custom wallpapers.

Any image size works. For the least scaling, use the native panel size:

- **640×480** for R36SX, R36 HD and GB350.
- **854×480** for SF3000, SF3000 HD, SF3100 and SF3500.

### Wallpaper fit modes

| Mode | Result |
|---|---|
| **Fill** | Preserves aspect ratio, covers the display and crops the excess |
| **Fit** | Preserves aspect ratio and shows the whole image; unused space uses the theme colour |
| **Stretch** | Fills the display without cropping; may distort the image |
| **Center** | Keeps the original pixel size, centred; large images are cropped |
| **Tile** | Repeats the image at its original size |

Use **None** under Wallpaper to return to per-system backgrounds.

### Wallpaper troubleshooting

- **The image is not listed:** check that it is in `frogui/wallpapers/`, uses a supported format, and restart TreeFrogUI.
- **The image is selected but not visible:** turn **Background Images** on.
- **Text is hard to read:** use a darker image, change the theme, or disable background images.

---

## Per-system backgrounds

Per-system backgrounds change with the current screen or ROM folder. They are used when **Background Images** is on and **Wallpaper** is set to **None**.

Place the files directly in `/mnt/sdcard/frogui/`:

| Screen or folder | Filename |
|---|---|
| Main system list and fallback | `main.png` |
| Recent games | `recents.png` |
| Favourites | `favourites.png` |
| Settings | `settings.png` |
| A ROM folder | The folder name plus `.png`, for example `GBA.png` or `FC.png` |

The base filename must exactly match the ROM folder name, including case. Use a lower-case `.png`, `.jpg`, `.jpeg`, or `.bmp` extension.

Per-system images stretch to the full panel. Use **640×480** on R36SX, R36 HD and GB350, or **854×480** on the SF3000 family, to avoid distortion. If a screen has no matching image, TreeFrogUI falls back to `main.*`.

Dark, low-contrast images work best because menu text is drawn over them.

---

## Built-in colour themes

TreeFrogUI's built-in themes include:

| Theme | Style |
|---|---|
| `MinUI Style` | Minimal dark |
| `Emerald` | Dark green |
| `Orange` | High-contrast orange |
| `Golden` | Gold and yellow |
| `Rose` | Grey and rose |
| `Purple` | Deep purple |
| `Prosty's Pink` | Bright pink and white |
| `Green` | Natural green |
| `Red` | Retro red |
| `Commodore 64` | Blue and lavender |
| `Game Boy` | Four-shade green |
| `NES` | Off-white and grey |
| `Amber CRT` | Amber on black |
| `Green CRT` | Green on black |
| `DOS` | Blue with yellow headers |
| `Famicom` | Maroon and gold |
| `SNES` | Light grey and purple |
| `Matrix` | Green and black |
| `Sajnaps Green` | Dark arcade green |
| `Q_ta's Light Wii` | Light grey and blue |
| `Q_ta's Dark Wii` | Dark grey and blue |
| `Desoxyn's Purple` | Neon purple on black |
| `Ocean` | Deep blue and cyan |
| `Sunset` | Purple and orange |
| `Mono Dark` | Dark grey and white |
| `Nord` | Cool arctic blue |
| `Dracula` | Dark purple and pink |
| `Gruvbox` | Warm sand and yellow |
| `Tokyo Night` | Blue, purple and cyan |
| `Solarized Dark` | Dark teal and yellow |

The Allium palette ports are `alium_default`, `alium_alliumboy`, `alium_blue`,
`alium_boxart`, `alium_brown`, `alium_bubblegum`, `alium_eva_01`, `alium_min`,
`alium_pastel`, `alium_snes`, `alium_spruceos`, and `alium_synthwave`.
Select **alium_Nunito** under Font for Allium's default **Nunito Bold** face.
Turn Background Images off to see the plain colors. These ports adapt the
source RGB palette to RGB565; wallpapers, transparency, and layouts are not
included. Credits and licenses ship in `frogui/fonts/alium_README.md` and
`frogui/fonts/alium_Nunito-OFL.txt`.

---

## Custom fonts

Copy a `.ttf` or `.otf` file to `/mnt/sdcard/frogui/fonts/`, restart TreeFrogUI, then select it under Settings → Appearance → **Font**.

TreeFrogUI also scans `/mnt/sdcard/cubegm/fonts/`. If neither font folder contains a usable file, the embedded GamePocket and Monogram fonts remain available as a fallback. The release ships with BPreplayBold as the default.
