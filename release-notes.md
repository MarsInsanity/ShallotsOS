# TreeFrogUI v1.8.0_q prerelease

## Changes since v1.8.0_p

- **FrogUI:** Restored the mainline core, including working **Games → Ports** browsing and launchers, friendly names for Java Games (`j2me`), Picante8 (`picante8`), and Ports (`ports`), and the animated selected-item pill. The `zsnes` folder selects Snes9x-Z and reuses SNES artwork and background. Startup stays black while enabled background art loads; disabling backgrounds shows the theme color immediately.
- **FrogUI performance and memory:** Restored the fast three-frame background and carousel transitions, Recents startup default, and idle-deferred banner loading. Font files are memory-mapped instead of reserving three 24 MiB buffers, reducing the built core's static memory by about 75 MiB. All twelve built-in locale files now contain the same translation keys.
- **PicoArch:** Retains the corrected submenu pill alignment and smoother, slower option-label scrolling from v1.8.0_p.

## Other changes included since the v1.0.15 stable base

- Added Snes9x-Z as a slower, compatibility-oriented alternative SNES core, plus Picante8, experimental Duke Nukem 3D, ClassiCube, Minecraft PE, Captain Claw, Diablo 1, Java Games, Nintendo DS and experimental QPSX support. Added OpenLara and VVVVVV under **Games → Ports**, with setup guides for user-supplied game data; ports have a separate release collection.
- Added support profiles and install guidance for R36SX/R36 HD variants, SF3000, SF3000 HD, SF3000HD without HDMI, SF3100, SF3500 and GB350.
- Added FrogUI menu-frame screenshots, Nunito, improved translations and Arabic text rendering, shared volume and battery indicators, and PICO-8 system alternatives.
- Improved game handoff, PicoArch audio startup and underrun handling, Java saves and game exit, ebook progress and page turning, and offline update recovery.

For a fresh installation, use the full archive and matching `install_first/<device>/` files. To update an existing installation, copy `update.zip` to the SD-card root and reboot. Back up your card first.
