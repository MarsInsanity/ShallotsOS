# Save files and save states

← [back to README](../../README.md)

## PicoArch save states

For Libretro games launched through PicoArch, save states are stored on the SD
card in:

```text
/mnt/sdcard/picoarch/<ROM-folder>/<ROM-name>.st<slot>
```

The folder is the ROM's **immediate parent folder**, and the filename is the
ROM name without its extension. For example, `roms/gb/Pokemon.gbc` uses:

```text
/mnt/sdcard/picoarch/gb/Pokemon.st0
/mnt/sdcard/picoarch/gb/Pokemon.st1
...
```

Manual save-state slots are `.st0` through `.st7`. When Auto-save/autoload is
enabled in Settings, its resume state uses slot 9 (`.st9`). Games that support
battery saves use the same folder and ROM name with a `.sav` extension. Keep
these files when backing up or moving a game library.

J2ME is an exception: its battery saves are in
`/mnt/sdcard/saves/j2me/`. Its PicoArch save states still use the normal
`picoarch/j2me/` folder.

## Ebook reading progress

The standalone **Ebook** app stores progress beside each book, in a hidden
`.positions` directory. For `roms/Ebook/Novel.pdf`, for example, progress is
in `roms/Ebook/.positions/Novel.pdf.pos`. Back up that directory with the
books.

The alternative [FroggyReader EPUB app](froggyreader.md) stores its library
settings and reading position in
`/mnt/sdcard/picoarch/epub/epub_reader/reader.cfg`. It does not support
PicoArch save states.
