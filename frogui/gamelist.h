#ifndef GAMELIST_H
#define GAMELIST_H

#include <stddef.h>

/* EmulationStation gamelist.xml / OnionOS miyoogamelist.xml support. Each
 * ROM folder (or a parent of it) may hold one; ShallotsOS reads the game's
 * <name> and its <image> / <thumbnail> from it. Lists are parsed once and
 * kept in memory, and re-read when the file changes. */

/* The game's name from its folder's gamelist, or NULL. */
const char *gamelist_name(const char *rom_path);

/* The game's picture: <image> then <thumbnail>, as an existing file path.
 * Returns 1 and writes out when one exists. */
int gamelist_image(const char *rom_path, char *out, size_t size);

/* Forget everything read so far (after a file-cache rebuild). */
void gamelist_forget(void);

#endif
