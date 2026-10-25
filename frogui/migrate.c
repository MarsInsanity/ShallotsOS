#include "migrate.h"

#include "core_override.h"
#include "systems.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define MIGRATE_DONE "/mnt/sdcard/frogui/.shallotsos_folders"
#define SAVES_ROOT "/mnt/sdcard/picoarch"
#define MIG_PATH 1024

/* Files that store full ROM paths (tab or '|' separated, one per line). */
static const char *const path_files[] = {
    "/mnt/sdcard/game_history.txt",
    "/mnt/sdcard/frogui/favorites.txt",
    "/mnt/sdcard/frogui/core_overrides.txt",
    "/mnt/sdcard/frogui/playtime.txt",
    "/mnt/sdcard/frogui/play_sessions.txt",
    "/mnt/sdcard/picoarch/last_game.txt",
    "/mnt/sdcard/picoarch/resume_on_boot.txt",
    NULL,
};

/* Artwork and scraper folders inside a system folder: merged file by file
 * into the new folder's own, not moved as a whole. */
static int is_art_dir(const char *name) {
  return !strcasecmp(name, ".res") || !strcasecmp(name, "Imgs") ||
         !strcasecmp(name, "images") || !strcasecmp(name, "media") ||
         !strcasecmp(name, "downloaded_images") || !strcasecmp(name, "manuals") ||
         !strcasecmp(name, "videos");
}

static int is_dir(const char *p) {
  struct stat st;
  return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int exists(const char *p) { return access(p, F_OK) == 0; }

/* The folder a legacy folder's content goes to: for a file, by its
 * extension; for a folder or the folder itself, the folder's target. */
static const char *target_for(const char *legacy, const char *name, int name_is_dir) {
  return systems_migrate_target(legacy, name_is_dir ? NULL : name);
}

static int moves_away(const char *legacy, const char *target) {
  return target && strcasecmp(legacy, target) != 0;
}

/* rename(), creating the destination folder first. 1 moved, 0 kept (the
 * name is taken), -1 error. */
static int move_one(const char *src, const char *dst_dir, const char *name) {
  char dst[MIG_PATH];
  mkdir(dst_dir, 0755);
  snprintf(dst, sizeof dst, "%s/%s", dst_dir, name);
  if (exists(dst))
    return 0;
  return rename(src, dst) == 0 ? 1 : -1;
}

/* Merge every file of src_dir into dst_dir (recursing into folders);
 * src_dir is removed if that empties it. */
static void merge_dir(const char *src_dir, const char *dst_dir, MigrateReport *rep) {
  DIR *d = opendir(src_dir);
  if (!d)
    return;
  mkdir(dst_dir, 0755);
  struct dirent *e;
  while ((e = readdir(d))) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
      continue;
    char src[MIG_PATH], dst[MIG_PATH];
    snprintf(src, sizeof src, "%s/%s", src_dir, e->d_name);
    snprintf(dst, sizeof dst, "%s/%s", dst_dir, e->d_name);
    if (is_dir(src) && is_dir(dst)) {
      merge_dir(src, dst, rep);
      continue;
    }
    int r = move_one(src, dst_dir, e->d_name);
    if (r < 0 && rep)
      rep->failed++;
  }
  closedir(d);
  rmdir(src_dir);
}

/* A game's saves, save states, screenshots and cheats follow it:
 * picoarch/<legacy>/<stem>.* -> picoarch/<target>/. */
static void move_saves(const char *legacy, const char *target, const char *file) {
  char stem[512], from[MIG_PATH], to[MIG_PATH];
  snprintf(stem, sizeof stem, "%s", file);
  char *dot = strrchr(stem, '.');
  if (dot)
    *dot = '\0';
  size_t sl = strlen(stem);
  const char *subdirs[] = {"", "/cheats", NULL};
  for (int k = 0; subdirs[k]; k++) {
    snprintf(from, sizeof from, SAVES_ROOT "/%s%s", legacy, subdirs[k]);
    snprintf(to, sizeof to, SAVES_ROOT "/%s%s", target, subdirs[k]);
    DIR *d = opendir(from);
    if (!d)
      continue;
    struct dirent *e;
    while ((e = readdir(d))) {
      if (strncmp(e->d_name, stem, sl) || e->d_name[sl] != '.')
        continue;
      char src[MIG_PATH];
      snprintf(src, sizeof src, "%s/%s", from, e->d_name);
      if (!is_dir(src)) {
        mkdir(SAVES_ROOT "/", 0755);
        char parent[MIG_PATH];
        snprintf(parent, sizeof parent, SAVES_ROOT "/%s", target);
        mkdir(parent, 0755);
        move_one(src, to, e->d_name);
      }
    }
    closedir(d);
  }
}

/* A game's own pictures follow it when it goes somewhere else than its
 * folder's art: <legacy>/<art dir>/<stem>.png, <stem>-title.png... */
static void move_art(const char *roms, const char *legacy, const char *target, const char *file) {
  static const char *const art_dirs[] = {".res", "Imgs", "images", "media/images",
                                         "media/thumbnails", "media/box2d", NULL};
  char stem[512];
  snprintf(stem, sizeof stem, "%s", file);
  char *dot = strrchr(stem, '.');
  if (dot)
    *dot = '\0';
  size_t sl = strlen(stem);
  for (int k = 0; art_dirs[k]; k++) {
    char from[MIG_PATH], to[MIG_PATH];
    snprintf(from, sizeof from, "%s/%s/%s", roms, legacy, art_dirs[k]);
    DIR *d = opendir(from);
    if (!d)
      continue;
    snprintf(to, sizeof to, "%s/%s/%s", roms, target, art_dirs[k]);
    char *slash = strrchr(to, '/');
    if (strchr(art_dirs[k], '/') && slash) {   /* media/... : make media first */
      *slash = '\0';
      mkdir(to, 0755);
      *slash = '/';
    }
    struct dirent *e;
    while ((e = readdir(d))) {
      char c = e->d_name[sl];
      if (strncmp(e->d_name, stem, sl) || (c != '.' && c != '-' && c != '_'))
        continue;
      char src[MIG_PATH];
      snprintf(src, sizeof src, "%s/%s", from, e->d_name);
      move_one(src, to, e->d_name);
    }
    closedir(d);
  }
}

/* ---- stored paths ---------------------------------------------------------- */

/* The new path of a ROM path that moved (by where the file is now), written
 * to out; 0 when it did not move. */
static int new_path_for(const char *roms, const char *path, char *out, size_t size) {
  size_t rl = strlen(roms);
  if (strncmp(path, roms, rl) || path[rl] != '/')
    return 0;
  const char *rel = path + rl + 1;
  const char *slash = strchr(rel, '/');
  char legacy[128];
  size_t ll = slash ? (size_t)(slash - rel) : strlen(rel);
  if (ll == 0 || ll >= sizeof legacy)
    return 0;
  memcpy(legacy, rel, ll);
  legacy[ll] = '\0';
  const char *rest = slash ? slash + 1 : "";
  const char *first_end = strchr(rest, '/');
  const char *target;
  if (!rest[0]) {
    target = target_for(legacy, NULL, 1);
  } else if (first_end) {
    target = target_for(legacy, NULL, 1);   /* inside a game folder */
  } else {
    target = target_for(legacy, rest, 0);
  }
  if (!moves_away(legacy, target))
    return 0;
  snprintf(out, size, "%s/%s%s%s", roms, target, rest[0] ? "/" : "", rest);
  /* Only where the move actually happened. */
  return !exists(path) && exists(out);
}

static void rewrite_file(const char *file, const char *roms) {
  FILE *in = fopen(file, "r");
  if (!in)
    return;
  char tmp[MIG_PATH];
  snprintf(tmp, sizeof tmp, "%s.migrating", file);
  FILE *out = fopen(tmp, "w");
  if (!out) {
    fclose(in);
    return;
  }
  char line[4096];
  size_t rl = strlen(roms);
  int changed = 0;
  while (fgets(line, sizeof line, in)) {
    char res[8192];
    size_t w = 0;
    const char *p = line;
    while (*p && w < sizeof res - 1) {
      const char *hit = strstr(p, roms);
      if (!hit || hit[rl] != '/') {
        size_t n = strlen(p);
        if (n > sizeof res - 1 - w)
          n = sizeof res - 1 - w;
        memcpy(res + w, p, n);
        w += n;
        break;
      }
      size_t pre = (size_t)(hit - p);
      memcpy(res + w, p, pre < sizeof res - 1 - w ? pre : sizeof res - 1 - w);
      w += pre < sizeof res - 1 - w ? pre : sizeof res - 1 - w;
      size_t tl = strcspn(hit, "\t|\r\n\"");
      char old[MIG_PATH], neu[MIG_PATH];
      snprintf(old, sizeof old, "%.*s", (int)tl, hit);
      const char *use = old;
      if (new_path_for(roms, old, neu, sizeof neu)) {
        use = neu;
        changed = 1;
      }
      size_t ul = strlen(use);
      if (ul > sizeof res - 1 - w)
        ul = sizeof res - 1 - w;
      memcpy(res + w, use, ul);
      w += ul;
      p = hit + tl;
    }
    res[w] = '\0';
    fputs(res, out);
  }
  fclose(in);
  if (fclose(out) == 0 && changed) {
    rename(tmp, file);
  } else {
    unlink(tmp);
  }
}

/* ---- the move ---------------------------------------------------------------- */

static int folder_needs_move(const char *roms, const char *legacy) {
  char dir[MIG_PATH];
  snprintf(dir, sizeof dir, "%s/%s", roms, legacy);
  DIR *d = opendir(dir);
  if (!d)
    return 0;
  int need = 0;
  struct dirent *e;
  while (!need && (e = readdir(d))) {
    if (e->d_name[0] == '.' && strcasecmp(e->d_name, ".res"))
      continue;
    char p[MIG_PATH];
    snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
    need = moves_away(legacy, target_for(legacy, e->d_name, is_dir(p)));
  }
  closedir(d);
  return need;
}

int migrate_is_done(void) { return exists(MIGRATE_DONE); }

int migrate_needed(const char *roms) {
  if (exists(MIGRATE_DONE))
    return 0;
  DIR *d = opendir(roms);
  if (!d)
    return 0;
  int need = 0;
  struct dirent *e;
  while (!need && (e = readdir(d))) {
    if (e->d_name[0] == '.' || !systems_migrate_target(e->d_name, NULL))
      continue;
    need = folder_needs_move(roms, e->d_name);
  }
  closedir(d);
  return need;
}

typedef struct {
  char path[MIG_PATH];
  const char *core;
} KeepCore;

static void migrate_folder(const char *roms, const char *legacy, MigrateReport *rep,
                           KeepCore **keep, int *nkeep, int *capkeep) {
  char dir[MIG_PATH];
  snprintf(dir, sizeof dir, "%s/%s", roms, legacy);
  DIR *d = opendir(dir);
  if (!d)
    return;
  /* Read the names first: renaming while iterating is not safe on FAT. */
  char **names = NULL;
  int n = 0, cap = 0;
  struct dirent *e;
  while ((e = readdir(d))) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
      continue;
    if (n == cap) {
      cap = cap ? cap * 2 : 64;
      char **g = realloc(names, (size_t)cap * sizeof *names);
      if (!g)
        break;
      names = g;
    }
    names[n] = strdup(e->d_name);
    if (names[n])
      n++;
  }
  closedir(d);
  const char *folder_target = target_for(legacy, NULL, 1);
  /* Games first (their own pictures with them), then the art folders. */
  for (int pass = 0; pass < 2; pass++)
  for (int i = 0; i < n; i++) {
    char src[MIG_PATH], dst_dir[MIG_PATH];
    snprintf(src, sizeof src, "%s/%s", dir, names[i]);
    int sdir = is_dir(src);
    if ((sdir && is_art_dir(names[i])) != pass)
      continue;
    if (sdir && is_art_dir(names[i])) {
      if (moves_away(legacy, folder_target)) {
        snprintf(dst_dir, sizeof dst_dir, "%s/%s/%s", roms, folder_target, names[i]);
        char parent[MIG_PATH];
        snprintf(parent, sizeof parent, "%s/%s", roms, folder_target);
        mkdir(parent, 0755);
        merge_dir(src, dst_dir, rep);
      }
      continue;
    }
    if (names[i][0] == '.')
      continue;   /* other hidden files stay */
    const char *target = target_for(legacy, names[i], sdir);
    if (!moves_away(legacy, target))
      continue;
    if (!sdir && (!strcasecmp(names[i], "gamelist.xml") ||
                  !strcasecmp(names[i], "miyoogamelist.xml")))
      target = folder_target;
    snprintf(dst_dir, sizeof dst_dir, "%s/%s", roms, target);
    int r = move_one(src, dst_dir, names[i]);
    if (r > 0) {
      rep->moved++;
      if (!sdir)
        move_saves(legacy, target, names[i]);
      if (!sdir && (!moves_away(legacy, folder_target) || strcasecmp(target, folder_target)))
        move_art(roms, legacy, target, names[i]);
      const char *core = systems_legacy_core(legacy, target);
      if (core && *nkeep < 100000) {
        if (*nkeep == *capkeep) {
          *capkeep = *capkeep ? *capkeep * 2 : 64;
          KeepCore *g = realloc(*keep, (size_t)*capkeep * sizeof **keep);
          if (!g)
            continue;
          *keep = g;
        }
        snprintf((*keep)[*nkeep].path, sizeof (*keep)[*nkeep].path, "%s/%s", dst_dir, names[i]);
        (*keep)[*nkeep].core = core;
        (*nkeep)++;
      }
    } else if (r == 0) {
      rep->kept++;
    } else {
      rep->failed++;
    }
  }
  for (int i = 0; i < n; i++)
    free(names[i]);
  free(names);
  /* The folder's remaining saves (per-folder emulator settings...) follow
   * the folder, then empty old folders go. */
  if (moves_away(legacy, folder_target)) {
    char from[MIG_PATH], to[MIG_PATH];
    snprintf(from, sizeof from, SAVES_ROOT "/%s", legacy);
    snprintf(to, sizeof to, SAVES_ROOT "/%s", folder_target);
    if (is_dir(from))
      merge_dir(from, to, NULL);
    rmdir(dir);
  }
}

void migrate_finish(const char *roms) {
  mkdir(roms, 0755);
  /* TreeFrogUI's empty folders go (rmdir only removes empty ones), and
   * their empty save folders. */
  for (int i = 0; i < systems_legacy_count(); i++) {
    const char *legacy = systems_legacy_at(i);
    if (!moves_away(legacy, target_for(legacy, NULL, 1)))
      continue;
    char p[MIG_PATH];
    snprintf(p, sizeof p, "%s/%s", roms, legacy);
    rmdir(p);
    snprintf(p, sizeof p, SAVES_ROOT "/%s", legacy);
    rmdir(p);
  }
  for (int i = 0; i < systems_count(); i++) {
    char p[MIG_PATH];
    snprintf(p, sizeof p, "%s/%s", roms, systems_at(i)->folder);
    mkdir(p, 0755);
  }
  mkdir("/mnt/sdcard/frogui", 0755);
  FILE *f = fopen(MIGRATE_DONE, "w");
  if (f) {
    fputs("ShallotsOS (Recalbox) ROM folders\n", f);
    fclose(f);
  }
  sync();
}

void migrate_run(const char *roms, MigrateReport *rep) {
  memset(rep, 0, sizeof *rep);
  KeepCore *keep = NULL;
  int nkeep = 0, capkeep = 0;
  /* Folder names first, as for the files. */
  char **folders = NULL;
  int n = 0, cap = 0;
  DIR *d = opendir(roms);
  if (d) {
    struct dirent *e;
    while ((e = readdir(d))) {
      if (e->d_name[0] == '.' || !systems_migrate_target(e->d_name, NULL))
        continue;
      if (n == cap) {
        cap = cap ? cap * 2 : 32;
        char **g = realloc(folders, (size_t)cap * sizeof *folders);
        if (!g)
          break;
        folders = g;
      }
      folders[n] = strdup(e->d_name);
      if (folders[n])
        n++;
    }
    closedir(d);
  }
  for (int i = 0; i < n; i++) {
    migrate_folder(roms, folders[i], rep, &keep, &nkeep, &capkeep);
    free(folders[i]);
  }
  free(folders);

  for (int i = 0; path_files[i]; i++)
    rewrite_file(path_files[i], roms);
  /* Overrides were rewritten on disk: reload, then add the kept emulators. */
  core_override_load();
  for (int i = 0; i < nkeep; i++)
    if (!core_override_lookup(keep[i].path, NULL))
      core_override_set(keep[i].path, keep[i].core);
  free(keep);
  migrate_finish(roms);
}
