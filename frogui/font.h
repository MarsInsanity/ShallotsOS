#ifndef FONT_H
#define FONT_H

#include <stdint.h>

// Initialize font system
void font_init(void);

// Load font from settings (call when font setting changes)
void font_load_from_settings(const char *font_name);

// Load a font directly by its on-disk filename (e.g. "monogram.ttf").
// Searches the standard font directories. Used by the dynamic font picker.
void font_load_file(const char *font_filename);

// Change the active UI font size and invalidate cached glyphs.
void font_set_size(int pixels);
void font_set_render_scale(int percent);

// Retrieve current base UI font size in points/pixels.
int font_get_size(void);

/* Select one face for the entire UI when the chosen language needs glyphs
 * absent from the configured primary font. */
void font_sync_language_fallback(void);

// Draw a text string at position (x, y) with given color. An optional bold
// argument is accepted for compatibility with newer renderers.
void font_draw_text(uint16_t *framebuffer, int screen_width, int screen_height,
                    int x, int y, const char *text, uint16_t color,
                    ...);

// Measure text width in pixels. An optional bold argument is accepted for
// compatibility with newer renderers.
int font_measure_text(const char *text, ...);

// Same as font_draw_text/font_measure_text with explicit synthetic bold.
void font_draw_text_bold(uint16_t *framebuffer, int screen_width,
                         int screen_height, int x, int y, const char *text,
                         uint16_t color, int bold);
int font_measure_text_bold(const char *text, int bold);

// Ascent above and descent below the baseline of the active face, in pixels.
void font_line_metrics(int *ascent_out, int *descent_out);

// Extra faces by absolute path, used by OnionOS themes. font_face_open returns
// a cached handle (or -1). font_face_use makes that face and pixel size the one
// used by the draw/measure calls; font_face_use(-1, 0) restores the UI font.
int font_face_open(const char *path);
void font_face_use(int handle, int pixels);
void font_face_close_all(void);

// Vertical metrics for centering: baseline (top-of-cell to baseline) and
// cap_height (pixel height of capitals = the visible ink band).
void font_cap_metrics(int *baseline_out, int *cap_height_out);

// Get font character width/height - scale with UI_SCALE
#ifndef UI_SCALE
#define UI_SCALE 100
#endif

#endif // FONT_H
