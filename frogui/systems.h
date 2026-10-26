#ifndef SYSTEMS_H
#define SYSTEMS_H

#include <stddef.h>

/* ShallotsOS's ROM folders, named like Recalbox's (roms/nes, roms/gbc,
 * roms/megadrive, roms/psx, ...), one folder per system. Each system lists
 * its emulators, the first one installed being the default; the others are
 * picked per game or per system (X on a game). TreeFrogUI's folders, which
 * chose the emulator by folder (nesq, gbav, snes02...), map onto them. */

#define SH_MAX_CORES 10

typedef struct {
  const char *folder;    /* Recalbox folder name */
  const char *name;      /* Recalbox full name */
  const char *icon;      /* OnionOS icon name */
  const char *cores[SH_MAX_CORES];  /* emulator paths, default first */
} ShSystem;

int systems_count(void);
const ShSystem *systems_at(int i);

/* The system for a folder: a Recalbox name, or a TreeFrogUI one it replaces. */
const ShSystem *systems_find(const char *folder);
/* The system whose Recalbox folder is exactly this name (case-insensitive). */
const ShSystem *systems_find_exact(const char *folder);

/* The installed emulators of a system (n of them). */
int systems_cores(const ShSystem *s, const char **out, int max);
/* Default emulator: the first installed one, or NULL. */
const char *systems_default_core(const ShSystem *s);
/* A short label for an emulator path ("QuickNES", "PCSX4ALL"...). */
const char *systems_core_label(const char *core_path, char *buf, size_t size);

/* TreeFrogUI -> ShallotsOS: the folder a file in legacy_folder moves to (the
 * file's extension splits mixed folders: .gbc out of gb, .sms out of sega),
 * or NULL when legacy_folder is not a TreeFrogUI folder that moves. */
const char *systems_migrate_target(const char *legacy_folder, const char *file);
/* The emulator legacy_folder used when it differs from its new system's
 * default (nesq -> QuickNES): kept for each moved game. NULL otherwise. */
const char *systems_legacy_core(const char *legacy_folder, const char *target);
/* The FrogUI icon-set key for a folder (megadrive -> sega, psx -> ps1). */
const char *systems_icon_key(const char *folder);
/* Number of TreeFrogUI folder names, and each name. */
int systems_legacy_count(void);
const char *systems_legacy_at(int i);

#endif
