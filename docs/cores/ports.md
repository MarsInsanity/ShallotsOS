# TreeFrogUI Ports Collection

Ports are optional and released separately from TreeFrogUI. Install them with
the [TreeFrogUI Installer](https://github.com/tzubertowski/TreeFrogUI-installer/releases)
using its **Install ports** option, or download the latest archive from the
[Ports Collection release page](https://github.com/tzubertowski/TreeFrogUI_PortsCollection/releases/latest)
and extract it to the SD card root.

Each port installs its launcher at `roms/ports/<name>.sh` and keeps its
executable or Libretro core, metadata, and artwork in `roms/ports/<port-id>/`.
The device shows the launcher's friendly name under **Games → Ports**. Ports
that need files from an existing game installation use TreeFrogUI's shared
missing-files screen, which links to the port's setup guide.

See the [Ports Collection README](https://github.com/tzubertowski/TreeFrogUI_PortsCollection#how-to-install)
for manual installation and instructions for adding ports.
