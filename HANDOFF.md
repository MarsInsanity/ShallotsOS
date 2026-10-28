# ShallotsOS handoff

For a Claude Code session that can talk to the handheld directly (running
on the user's PC, with the console plugged in by USB-C). Read this, then
`SHALLOTSOS_CHECKLIST.md` (the running list of what is done, in progress and
open; keep it up to date).

## The project

ShallotsOS makes TreeFrogUI (a custom frontend for HiChip MIPS handhelds)
look and feel like OnionOS, fast and polished. It runs from the SD card on
top of the stock firmware, which is never modified.

- **Device:** R36SX, v2.7 hardware booting the v2.6 firmware
  (`install_first/r36sx/`), 640x480 panel. HiChip hc16xx, one MIPS 74Kc core
  at 1188 MHz. **Linux gets only ~47.6 MB of RAM** (43 MB managed); the rest
  belongs to hcRTOS, HiChip's co-OS for display/audio/media. The root
  filesystem and `/tmp` are **ramfs**: files there are RAM that is never
  swapped or reclaimed. 128 MB swap file on the SD (`cubegm/pagefile.sys`).
  Kernel 4.4.186-release. SD card is FAT32, mounted at `/mnt/sdcard`.
- **The user** installs from Windows using GitHub Actions artifacts and tests
  on the device; they are not a developer, so explain plainly and give exact
  steps.

## Repositories and branches (push only to these)

| Repo | Branch | What |
|---|---|---|
| MarsInsanity/ShallotsOS | `claude/treefrogui-onionos-themes-5391yd` | FrogUI menu (`frogui/`), launcher (`hijack/zhijack.tpl.sh`), updater (`hijack/tfupdate.sh`), apps (`apps/`: osd, usb_debug, setup, video_player), themes, overlay + full package workflow |
| MarsInsanity/ShallotsOS_picoarch | `claude/gameswitcher-fn-hold` | emulator frontend + in-game menu (`picoarch`, `picoarch_hi`) |
| MarsInsanity/ShallotsOS_pcsx4all | `claude/shallotsos-menu-integration` | PS1 standalone emulator |
| MarsInsanity/ShallotsOS_ebook_reader | `claude/shallotsos-reader` | Books reader (MuPDF) |
| MarsInsanity/HCSNES, HCPS1, HCGBA | `main` | experimental cores by another developer (built, not ours) |

Upstream: TreeFrogUI is `tzubertowski/TreeFrogUI` (base release v1.8.0_q).
ShallotsOS's default gpSP is upstream libretro/gpsp `2db57b1` (June 2026,
from TreeFrogUI's release); "gpSP (latest)" is built from libretro/gpsp master.

## Rules the user set

- Keep `SHALLOTSOS_CHECKLIST.md` current; give the direct GitHub Actions run
  link for every build, after confirming it is green.
- Push only to the branches above. No pull requests unless asked. Never put
  model identifiers in commits, code or artifacts.
- Commit trailer:
  ```
  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  Claude-Session: https://claude.ai/code/session_01RvKiEDRYuXZQCDsPdKRPea
  ```
  (a new session uses its own session link)
- Never put ROMs, BIOS files, saves, personal data or stock firmware
  binaries in packages. Stock drivers are derived on the device by
  `cubegm/shallots_setup`, never shipped.
- Never flip unknown GPIO pins. Don't modify the boot partition / stock
  boot files (SF3500 verifies them).
- Overlay test builds are fine; **release/publish workflows need explicit
  confirmation** (TreeFrogUI AGENTS.md: don't trigger release workflows or
  `publish_release.sh` unless asked).
- External repos' build scripts are built in CI, not run locally, where
  possible.

## Builds and installing

- Workflow "Build ShallotsOS overlay" (`.github/workflows/shallotsos-overlay.yml`)
  runs on pushes to `claude/**` touching its paths, or by "Run workflow"
  (inputs for picoarch/HCSNES/HCPS1/PCSX4ALL/ebook/gpSP refs, version). A
  picoarch-only change needs a manual run. Artifacts: `ShallotsOS-overlay`
  (copy to SD root), `ShallotsOS-full` (full install), `ShallotsOS-update`
  (`update.zip`: copy to SD root and boot; keeps settings).
- Latest green build at handoff: run 40652006084 (ShallotsOS 69b81c3,
  picoarch f66766a). The user had installed it.
- The launcher on the card runs the update, so launcher changes (update
  screen etc.) show from the update *after* the one that installs them.

## Talking to the handheld (new)

The **USB Debug Shell** works: Settings > System > USB Debug Shell on, then
the console is a USB serial port (CDC ACM) with a root shell; on Windows
"USB Serial Device (COMn)". `tools/shallots_serial.py` runs commands and
prints their output (needs `pip install pyserial`):

```
python tools/shallots_serial.py COM5 "free" "dmesg | tail -30"
python tools/shallots_serial.py COM5 --interactive
```

Useful on the device: `free`, `ps`, `dmesg`, `top -n 1`, `cat /tmp/tfdevice.env`,
`ls /mnt/sdcard/cubegm/logs`, `tail /tmp/treefrog_ui.log` (frontend output of
the current/last launch). An empty `/mnt/sdcard/log.txt` turns on detailed
logs (`logs/picoarch.log`, `logs/cores.log`, `cubegm/logs/zhijack.log`).
The shell's script is `apps/usb_debug/usb_debug.sh`; modules
`cubegm/modules/4.4.186-release/{u_serial,usb_f_acm}.ko`. USB mode (MTP)
stops the shell until the next boot. Mind the RAM: don't copy big files into
`/tmp`.

Launch games on the console itself. The launcher loop (`hijack/zhijack.tpl.sh`)
starts them from `cubegm/frogui_launch.txt` (core path, then ROM path) after
the menu exits; gpSP and PS1 cores run in `cubegm/picoarch_hi`. Starting a
frontend from the shell while the menu runs would fight it for the display.

## In progress at handoff

1. **Hamtaro: Ham-Ham Heartbreak freezes on gpSP** (both gpSP and gpSP
   latest) right after "Yep-P" on the name confirmation screen, i.e. at the
   game's first save write. Suspected save-type problem in gpSP:
   detection (`gba_memory.c` `detect_backup_subcircuit`) only scans the first
   1 MB for EEPROM_V/SRAM_V/FLASH*_V markers, else guesses from bus access;
   Flash answers with a fixed Macronix 64 KB ID (some games want another
   maker). Next steps:
   - get the game code and markers from the ROM (zipped ROMs are unpacked to
     `/mnt/sdcard/picoarch/.unzipped/pa-<name>` after one launch):
     ```
     R="<path to the .gba>"
     dd if="$R" bs=1 skip=160 count=16 2>/dev/null; echo
     grep -a -o 'EEPROM_V[0-9]*\|SRAM_F_V[0-9]*\|SRAM_V[0-9]*\|FLASH1M_V[0-9]*\|FLASH512_V[0-9]*\|FLASH_V[0-9]*' "$R" | sort | uniq -c
     grep -a -b -o 'EEPROM_V\|SRAM_V\|FLASH' "$R" | head -5
     ls -l /mnt/sdcard/picoarch/*/ | grep -i ham
     ```
   - test on mGBA (Y > Emulator > mGBA): saves there = gpSP bug.
   - fix via a `gba_over.h` entry (backup type / flash ID) or the detection,
     build it into "gpSP (latest)" (`scripts/build_gpsp_latest.sh`; a
     ShallotsOS patch can be applied there), and offer it upstream.
2. **GTA Advance** now runs (the out-of-memory cause is fixed: zipped ROMs
   unpack to the SD, not ramfs; the launcher cleans `/tmp/pa-*` after
   games). Still to check: a long session, other games afterwards, speed.
   If it OOMs again: a low-memory gpSP build (`ROM_BUFFER_SIZE` 4-8,
   `SMALL_TRANSLATION_CACHE`) as a separate choice.
3. **Update screen** ("Updating ShallotsOS" + progress bar, `shallots_osd
   --splash`, cubevol paused meanwhile): the fixed version (repaint every
   100 ms) is untested on the device.

## Open device tests and ideas

See `SHALLOTSOS_CHECKLIST.md` ("Device test" items): power-off screen,
music player volume bar, Books hints/chapters/bookmarks, 30 save slots,
box art from `media/images`, volume indicator after a resumed game, PC
Engine frameskip off, PicoDrive frameskip, HCPS1 crackle (upstream). Ideas
for later: ShallotsOS Hub (PC companion app, plan in the checklist),
monthly upstream watch (`.github/workflows/upstream-watch.yml`; schedule
needs the workflow on `main`).

## Key facts learned the hard way

- fb1 (the OSD overlay) is shared with the stock daemon `cubevol`, which
  repaints it; ShallotsOS's volume popups redirect cubevol's fb1 mapping
  (ptrace trick in `apps/osd/shallots_osd.c`). Anything drawn on fb1 must be
  repainted or cubevol paused (SIGSTOP/SIGCONT).
- picoarch's boot resume (`power_resume_redirect`) starts the game before
  FrogUI loads; anything FrogUI starts must also be started by the launcher.
- The display driver's scaler is bilinear only (`HCGE_NEAREST_TODO.md` in
  picoarch). Frameskip "Auto" uses picoarch's `frame_late_tick` rule.
- Stock driver variants and their MD5s: see `apps/setup/shallots_setup.c`.
