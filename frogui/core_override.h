/*
 * core_override.h - per-game / per-folder core selection overrides.
 *
 * Stored at /mnt/sdcard/frogui/core_overrides.txt, one entry per line:
 *   <key>|<core_path>
 * where <key> is either a ROM's full path (per-game) or a folder's full path
 * (per-folder). Lookup prefers a per-game match, then per-folder.
 */
#ifndef CORE_OVERRIDE_H
#define CORE_OVERRIDE_H

/* Load overrides from disk into memory. Call once at startup. */
void core_override_load(void);

/* Return the override core path for a ROM, or NULL if none.
 * Per-game (rom_path) wins over per-folder (folder_path). */
const char *core_override_lookup(const char *rom_path, const char *folder_path);

/* Set (or, when core_path is NULL, clear) the override for key, then persist.
 * key is a ROM full path (per-game) or folder full path (per-folder). */
void core_override_set(const char *key, const char *core_path);

#endif /* CORE_OVERRIDE_H */
