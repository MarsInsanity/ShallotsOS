# OpenBOR

TreeFrogUI includes the OpenBOR 3.0 build 7142 engine as a Libretro core. It
does not include a game or mod. OpenBOR content is distributed separately as
PAK files; place each legally obtained game/mod PAK in the OpenBOR system
folder. You can keep multiple PAKs there.

Copy the file to:

```text
roms/openbor/<game name>.pak
```

Open **Games → OpenBor** and select a PAK. OpenBOR stores its regular saves,
configuration, logs, and screenshots in the frontend's save directory.
Core startup/load diagnostics are in
`picoarch/openbor/openbor/logs/OpenBorCoreLog.txt` on the SD card. Libretro save
states are not supported.

Controls: D-pad moves, **B** attacks, **A** jumps, **Y/X** use attacks 2/3,
**L** uses attack 4, **R** uses special, **Start** starts, and **Select** exits.

The core is based on the legacy OpenBOR source and retains its upstream license.
The OpenBOR executable and user-provided game data are separate; no game data
is included in TreeFrogUI.
