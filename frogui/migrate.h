#ifndef MIGRATE_H
#define MIGRATE_H

/* First boot of ShallotsOS on a TreeFrogUI card: move the games from
 * TreeFrogUI's folders (nesq, gbav, sega, a26, ps1...) into ShallotsOS's
 * Recalbox-style ones (nes, gba, megadrive, atari2600, psx...).
 *
 * Nothing is deleted: games, their saves and save states (picoarch/<folder>),
 * artwork and gamelists are renamed into place; a file whose name is already
 * taken at the destination stays where it is. Old folders are removed only
 * once empty. Recents, favourites, play time, per-game emulator choices and
 * the resume files are rewritten to the new paths, and a game that ran on a
 * folder's own emulator (nesq = QuickNES) keeps it as a per-game choice. */

typedef struct {
  int moved;     /* games and game folders moved */
  int kept;      /* left behind: the name was taken at the destination */
  int failed;    /* rename errors */
} MigrateReport;

/* 1 when the card still has TreeFrogUI folders to move and the move was
 * not done (or declined) yet. */
int migrate_needed(const char *roms);

/* Move everything, rewrite the stored paths, then create the ShallotsOS
 * folders and mark the card as done. */
void migrate_run(const char *roms, MigrateReport *rep);

/* Create the ShallotsOS (Recalbox) system folders and mark the card as
 * done; for a card that has nothing to move. */
void migrate_finish(const char *roms);

/* 1 once the card has ShallotsOS's folders (marker written). */
int migrate_is_done(void);

#endif
