# TreeFrogUI v1.9.0_b

## What's new

- **Set the clock in Settings:** DATE/TIME (EMULATION) now lets you set the date and time with the D-pad or sync from a USB drive on demand. The section is above SYSTEM and collapsed by default.
- **Adjust time at boot:** Choose a per-reboot minute bump, from 0 to 1440 minutes in 5-minute steps, to estimate time elapsed while the handheld was off.
- **Hardware limitation:** Supported handhelds do not have RTC hardware, so they cannot track real elapsed time while powered off. The saved clock and optional boot bump are an emulation; use manual setting or USB sync for an accurate time. Credit and thanks to [MartStartIV](https://github.com/MartStartIV) for the simulated RTC foundation in [PR #77](https://github.com/tzubertowski/TreeFrogUI/pull/77) and its required time file in [PR #78](https://github.com/tzubertowski/TreeFrogUI/pull/78); this release adds the configurable per-boot time bump separately.
- **Documentation:** Added a date/time guide to the README and website, including the USB file format and clock limitations.
- **EPUB reader:** Added FroggyReader as an EPUB-only alternative under Apps. It opens its book library directly and renders a 640×480 interface with higher-resolution text.
- **PDF zoom:** The standalone Ebook reader supports PDF zoom up to 10× with X/Y and D-pad panning while zoomed in.

## Contributor credit

Thanks to [Luis Gabriel da Costa Silva (@Maheshivara)](https://github.com/Maheshivara) for [Ports Collection PR #12](https://github.com/tzubertowski/TreeFrogUI_PortsCollection/pull/12), which automates README updates and checks on pushes to `main`.

Thanks to [Synaps33](https://github.com/Synaps33) for [FroggyTuxRacer](https://github.com/Synaps33/FroggyTuxRacer), the Tux Racer port added to the Ports Collection in [PR #14](https://github.com/tzubertowski/TreeFrogUI_PortsCollection/pull/14).

Thanks to [Sajnaps (Synaps33)](https://github.com/Synaps33) for creating the [FroggyReader Libretro core](https://github.com/Synaps33/FroggyReader), which powers the new EPUB reader app.

For a fresh installation, use the full archive and matching `install_first/<device>/` files. To update an existing installation, copy `update.zip` to the SD-card root and reboot.
