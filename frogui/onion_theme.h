#ifndef ONION_THEME_H
#define ONION_THEME_H

#include "render.h"
#include "theme.h"
#include <stddef.h>
#include <stdint.h>

/*
 * OnionOS theme support.
 *
 * Themes use the exact OnionOS (Miyoo Mini) layout: a folder in
 * /mnt/sdcard/Themes containing config.json, skin/ images, optional icons/,
 * a font file and sound/change.wav. Theme .zip files from the Onion theme
 * catalogue can be dropped into the same folder; they are unpacked the first
 * time they are selected.
 *
 * Every drawing call works in Onion's 640x480 design space. On wider panels
 * full-width bars stretch to the panel, centred content is offset and
 * right-anchored items stay on the right edge.
 */

#define ONION_THEMES_DIR "/mnt/sdcard/Themes"
/* Stand-in for Onion's /mnt/SDCARD/miyoo/app: skin/ and fonts/ used when a
 * theme omits an image or names a font that is not on the card. */
#define ONION_FALLBACK_DIR "/mnt/sdcard/frogui/onion"
/* Onion's per-profile theme overrides (written by its Tweaks app). */
#define ONION_OVERRIDES_DIR "/mnt/sdcard/Saves/CurrentProfile/theme"

/* ---- Theme discovery and activation ---- */
void onion_theme_scan(void);
int onion_theme_count(void);
const char *onion_theme_folder(int index); /* folder or .zip file name */
const char *onion_theme_label(int index);  /* name for menus */
int onion_theme_index_of(const char *folder);
/* Load a theme (index < 0 turns Onion mode off). Returns 1 on success. */
int onion_theme_select(int index);
int onion_theme_active(void);
int onion_theme_current(void);
/* Colours derived from the active theme for screens drawn with the palette. */
const Theme *onion_theme_palette(void);
/* Theme navigation sound (sound/change.wav) as interleaved stereo frames at
 * `rate`. Returns the frame count, or 0 when the theme has no sound. */
int onion_theme_change_sound(int rate, const int16_t **frames);
/* Release fonts, images and sounds. */
void onion_theme_unload(void);

/* ---- Screens ---- */
enum { ONION_HOME_RECENT, ONION_HOME_FAVORITE, ONION_HOME_GAME,
       ONION_HOME_APP, ONION_HOME_SETTING, ONION_HOME_EXPERT };

typedef struct {
  int kind;          /* ONION_HOME_* */
  const char *label;
} OnionHomeItem;

typedef struct {
  const char *label;
  const char *folder; /* ROM folder name used to find the console icon */
} OnionGridItem;

enum { ONION_ROW_PLAIN, ONION_ROW_HEADER, ONION_ROW_TOGGLE, ONION_ROW_VALUE,
       ONION_ROW_INFO };

typedef struct {
  const char *label;
  const char *value;     /* ONION_ROW_VALUE / ONION_ROW_INFO text */
  const char *icon_path; /* optional icon drawn at the left */
  int kind;              /* ONION_ROW_* */
  int toggle_on;
  int disabled;
  int folder;            /* drawn with the theme's icon-folder */
  int favourite;         /* drawn with the theme's ic-favorite-mark */
} OnionListItem;

void onion_render_background(uint16_t *fb);
void onion_render_header(uint16_t *fb, const char *title, int show_logo);
void onion_render_footer(uint16_t *fb, const char *a_label,
                         const char *b_label, int current, int total);
void onion_render_home(uint16_t *fb, const OnionHomeItem *items, int count,
                       int selected);
/* items holds every system; only the page containing `selected` is drawn. */
void onion_render_grid(uint16_t *fb, const OnionGridItem *items, int count,
                       int selected);
/* items holds only the visible rows; `first` is the absolute index of
 * items[0], used for the footer counter. */
void onion_render_list(uint16_t *fb, const OnionListItem *items, int visible,
                       int selected_row, int large, int label_right_limit);
int onion_list_rows(int large);
/* Box art beside a game list (Onion draws preview-bg behind it). Returns the
 * left edge it occupies so list labels can stop short of it. */
int onion_render_preview(uint16_t *fb, const Thumbnail *art);
/* GameSwitcher overlay for a full-screen capture already in fb. `header` is
 * the header title (Onion shows play time there); `game` the selected game.
 * The legend (extra/gs-legend) is drawn when show_legend is set. */
/* A short message above the hint bar. */
void onion_render_toast(uint16_t *fb, const char *text);
/* Yes/no prompt (lines split on '\n') over a dimmed screen. */
void onion_render_prompt(uint16_t *fb, const char *message, const char *a_label,
                         const char *b_label);
enum { ONION_GS_NORMAL, ONION_GS_MINIMAL, ONION_GS_FULLSCREEN };
void onion_render_switcher(uint16_t *fb, int view, const char *header,
                           const char *game, int has_prev, int has_next,
                           int current, int total, int show_legend,
                           const char *a_label, const char *b_label);
/* App/console icon lookups (paths owned by the module, valid until the next
 * call). Return NULL when the theme does not provide one. */
const char *onion_theme_app_icon(const char *app_key);
/* Provided by the browser: path of the TreeFrogUI icon-pack icon for a ROM
 * folder, used for systems the theme has no icon for. */
const char *frogui_fallback_system_icon(const char *folder, char *path,
                                        size_t size);
int onion_theme_has_console_icons(void);

#endif /* ONION_THEME_H */
