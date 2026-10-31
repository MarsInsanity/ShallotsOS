/*
 * shallots_ui - themed screens for ShallotsOS script apps.
 *
 * A script app (App/<name>/launch.sh) has no screen of its own: the menu
 * has exited when it runs. This helper draws ShallotsOS-style screens in the
 * active theme's colours, background and font, reads the buttons, and
 * reports the result through its exit status and standard output:
 *
 *   shallots_ui message  [--title T] [--wait] TEXT     show text (--wait: until A/B)
 *   shallots_ui progress [--title T] PERCENT [TEXT]    progress bar
 *   shallots_ui confirm  [--title T] [--yes L] [--no L] TEXT
 *                                                       exit 0 = yes, 1 = no/B
 *   shallots_ui menu     [--title T] [--index] [--selected N] ITEM...
 *                                                       prints the item (or its index)
 *   shallots_ui keyboard [--title T] [--initial TEXT]  prints the typed text
 *   shallots_ui notify   [--seconds N] TEXT             a toast over the current screen
 *   shallots_ui clear                                   hand the screen back
 *
 * Exit status: 0 done / yes, 1 cancelled / no, 2 usage error, 3 no usable
 * screen (rotated panels are not handled yet), 4 no font.
 *
 * It draws on /dev/fb1, the overlay plane above everything (32-bit ARGB),
 * and repaints while it waits so nothing else drawing there (the stock
 * cubevol daemon) covers it. A screen stays up after the command returns,
 * until the next command or `clear`; the launcher clears it when the app
 * ends. Buttons come from cubevol's shared memory, as for the emulators.
 *
 * Host build (-DUI_HOST) for previews and tests: draws into memory, takes
 * buttons from UI_KEYS ("down,down,a") and writes the last frame to UI_SHOT.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifndef UI_HOST
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_NO_STDIO_WRITE
#include "stb_image.h"

static const char *sd = "/mnt/sdcard";

/* ---- Buttons (cubevol's joy_key bits) ------------------------------------- */

enum {
	B_SELECT = 1 << 0, B_START = 1 << 3, B_UP = 1 << 4, B_RIGHT = 1 << 5,
	B_DOWN = 1 << 6, B_LEFT = 1 << 7, B_L2 = 1 << 8, B_R2 = 1 << 9,
	B_L = 1 << 10, B_R = 1 << 11, B_X = 1 << 12, B_A = 1 << 13,
	B_B = 1 << 14, B_Y = 1 << 15, B_ALL = 0xFFFF
};

static __attribute__((unused)) long long now_ms(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

/* ---- Screen ------------------------------------------------------------- */

typedef struct {
	uint32_t *px;          /* the frame, composed in RAM */
	int w, h;
	uint8_t *fb;           /* fb1 */
	size_t fb_len;
	int fb_line, fd;
} Screen;
static Screen scr = {.fd = -1};
static int S100 = 100;     /* scale x100: panel height / 480 */
#define SC(v) ((v) * S100 / 100)

#ifdef UI_HOST
static const char *host_keys;

static int screen_open(void) {
	scr.w = 640;
	scr.h = 480;
	scr.fb_line = scr.w * 4;
	scr.fb_len = (size_t)scr.fb_line * scr.h;
	scr.fb = calloc(1, scr.fb_len);
	scr.px = calloc((size_t)scr.w * scr.h, 4);
	return scr.fb && scr.px;
}

static void write_shot(void) {
	const char *path = getenv("UI_SHOT");
	FILE *f = path ? fopen(path, "wb") : NULL;
	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", scr.w, scr.h);
	for (int y = 0; y < scr.h; y++)
		for (int x = 0; x < scr.w; x++) {
			uint32_t p = ((uint32_t *)(scr.fb + (size_t)y * scr.fb_line))[x];
			fputc(p >> 16 & 255, f);
			fputc(p >> 8 & 255, f);
			fputc(p & 255, f);
		}
	fclose(f);
}

/* UI_KEYS: comma-separated presses (up,down,left,right,a,b,x,y,l,r,start,select). */
static uint32_t host_next_press(void) {
	static const struct { const char *n; uint32_t b; } names[] = {
		{"up", B_UP}, {"down", B_DOWN}, {"left", B_LEFT}, {"right", B_RIGHT},
		{"a", B_A}, {"b", B_B}, {"x", B_X}, {"y", B_Y}, {"l", B_L}, {"r", B_R},
		{"start", B_START}, {"select", B_SELECT},
	};
	if (!host_keys || !*host_keys)
		return (uint32_t)-1;   /* out of keys */
	const char *e = strchr(host_keys, ',');
	size_t n = e ? (size_t)(e - host_keys) : strlen(host_keys);
	uint32_t b = 0;
	for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
		if (strlen(names[i].n) == n && !strncmp(names[i].n, host_keys, n))
			b = names[i].b;
	host_keys = e ? e + 1 : host_keys + n;
	return b;
}
#else
static volatile uint32_t *keys_shm;

static int rotated(void) {
	FILE *f = fopen("/tmp/tfdevice.env", "r");
	char line[128];
	int rot = 0;
	while (f && fgets(line, sizeof line, f))
		if (!strncmp(line, "TF_ROTATE=", 10))
			rot = atoi(line + 10);
	if (f)
		fclose(f);
	return rot != 0;
}

static int screen_open(void) {
	if (rotated())
		return 0;
	int fd = open("/dev/fb1", O_RDWR);
	if (fd < 0)
		return 0;
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	if (ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f) ||
	    v.bits_per_pixel != 32 || v.xres < 320 || v.yres < 240 || v.xres < v.yres) {
		close(fd);
		return 0;
	}
	void *m = mmap(NULL, f.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (m == MAP_FAILED) {
		close(fd);
		return 0;
	}
	scr.fd = fd;
	scr.fb = m;
	scr.fb_len = f.smem_len;
	scr.fb_line = f.line_length;
	scr.w = v.xres;
	scr.h = v.yres;
	scr.px = calloc((size_t)scr.w * scr.h, 4);
	if (!scr.px)
		return 0;
	ioctl(fd, FBIOBLANK, FB_BLANK_UNBLANK);
	key_t k = ftok("/tmp/joy_key", 'a');
	int id = k == (key_t)-1 ? -1 : shmget(k, 4, 0666);
	void *p = id < 0 ? (void *)-1 : shmat(id, NULL, SHM_RDONLY);
	if (p != (void *)-1)
		keys_shm = p;
	return 1;
}

/* The stock menu and cubevol both write joy_key, so single reads flicker at
 * press/release edges: take the value two of three reads agree on. */
static uint32_t read_keys(void) {
	if (!keys_shm)
		return 0;
	uint32_t a = *keys_shm & B_ALL;
	usleep(2000);
	uint32_t b = *keys_shm & B_ALL;
	if (a == b)
		return a;
	usleep(2000);
	return *keys_shm & B_ALL;
}
#endif

static void present(void) {
	for (int y = 0; y < scr.h; y++)
		memcpy(scr.fb + (size_t)y * scr.fb_line, scr.px + (size_t)y * scr.w, (size_t)scr.w * 4);
}

/* Wait for a press; returns its button (with auto-repeat for the d-pad and
 * shoulders). Repaints the frame meanwhile. */
static uint32_t wait_press(void) {
#ifdef UI_HOST
	uint32_t b = host_next_press();
	present();
	if (b == (uint32_t)-1) {   /* script ran out: behave like B */
		write_shot();
		return B_B;
	}
	return b;
#else
	/* Buttons already down when this screen opened (the press that opened
	 * it) count only after they are released and pressed again. */
	static uint32_t prev = B_ALL;
	static uint32_t repeat_key;          /* pressed on this screen: may repeat */
	static long long held_since, last_repeat;
	long long paint = 0;
	for (;;) {
		long long t = now_ms();
		if (t - paint >= 100) {
			present();
			paint = t;
		}
		uint32_t cur = read_keys();
		uint32_t down = cur & ~prev;
		uint32_t rep = B_UP | B_DOWN | B_LEFT | B_RIGHT | B_L | B_R;
		prev = cur;
		if (down) {
			held_since = last_repeat = t;
			for (int i = 0; i < 16; i++)
				if (down & (1u << i)) {
					repeat_key = (1u << i) & rep;
					return 1u << i;
				}
		}
		if (!(cur & repeat_key))
			repeat_key = 0;
		else if (t - held_since > 350 && t - last_repeat > 90) {
			last_repeat = t;
			return repeat_key;
		}
		usleep(14000);
	}
#endif
}

/* ---- Theme ---------------------------------------------------------------- */

static uint32_t c_bg = 0xFF141018, c_text = 0xFFFFFFFF, c_sel = 0xFFB06AE0, c_sel_text = 0xFFFFFFFF;
static char font_path[512], bg_path[512];
static int bg_rotated;   /* Miyoo-format themes store background.png upside down */

static void load_theme(void) {
	char path[600], line[600];
	snprintf(path, sizeof path, "%s/cubegm/skin/skin.txt", sd);
	FILE *f = fopen(path, "r");
	while (f && fgets(line, sizeof line, f)) {
		char *v = strchr(line, '=');
		if (!v)
			continue;
		*v++ = '\0';
		v[strcspn(v, "\r\n")] = '\0';
		uint32_t rgb = 0xFF000000u | (strtoul(v, NULL, 16) & 0xFFFFFFu);
		if (!strcmp(line, "text_color"))
			c_text = rgb;
		else if (!strcmp(line, "selection_color"))
			c_sel = rgb;
		else if (!strcmp(line, "sel_text_color"))
			c_sel_text = rgb;
		else if (!strcmp(line, "background_color"))
			c_bg = rgb;
		else if (!strcmp(line, "font"))
			snprintf(font_path, sizeof font_path, "%s", v);
		else if (!strcmp(line, "background"))
			snprintf(bg_path, sizeof bg_path, "%s", v);
		else if (!strcmp(line, "background_rotated"))
			bg_rotated = atoi(v);
	}
	if (f)
		fclose(f);
}

/* ---- Drawing -------------------------------------------------------------- */

static uint32_t blend(uint32_t dst, uint32_t src, unsigned a) {
	if (a >= 255)
		return src | 0xFF000000u;
	uint32_t out = 0xFF000000u;
	for (int s = 0; s < 24; s += 8) {
		unsigned d = dst >> s & 255, c = src >> s & 255;
		out |= ((c * a + d * (255 - a)) / 255) << s;
	}
	return out;
}

static void fill(int x, int y, int w, int h, uint32_t c, unsigned a) {
	for (int yy = y < 0 ? 0 : y; yy < y + h && yy < scr.h; yy++)
		for (int xx = x < 0 ? 0 : x; xx < x + w && xx < scr.w; xx++) {
			uint32_t *p = &scr.px[(size_t)yy * scr.w + xx];
			*p = blend(*p, c, a);
		}
}

static void fill_round(int x, int y, int w, int h, int r, uint32_t c, unsigned a) {
	if (r * 2 > h)
		r = h / 2;
	for (int py = 0; py < h; py++)
		for (int px = 0; px < w; px++) {
			int dx = px < r ? r - px : (px >= w - r ? px - (w - r - 1) : 0);
			int dy = py < r ? r - py : (py >= h - r ? py - (h - r - 1) : 0);
			int X = x + px, Y = y + py;
			if (dx * dx + dy * dy <= r * r && X >= 0 && Y >= 0 && X < scr.w && Y < scr.h) {
				uint32_t *p = &scr.px[(size_t)Y * scr.w + X];
				*p = blend(*p, c, a);
			}
		}
}

static uint32_t *bg_cache;

/* The theme background, filled and cropped to the panel, dimmed for text. */
static void draw_background(void) {
	if (!bg_cache) {
		int w, h, n;
		unsigned char *im = bg_path[0] ? stbi_load(bg_path, &w, &h, &n, 3) : NULL;
		bg_cache = malloc((size_t)scr.w * scr.h * 4);
		if (!bg_cache)
			return;
		for (int y = 0; y < scr.h; y++)
			for (int x = 0; x < scr.w; x++) {
				uint32_t c = c_bg;
				if (im) {
					/* cover: scale the shorter side to fit, centre the rest */
					long sx = (long)w * 1000 / scr.w, sy = (long)h * 1000 / scr.h;
					long s = sx < sy ? sx : sy;
					int ix = (int)(((long)x * s + ((long)w * 1000 - (long)scr.w * s) / 2) / 1000);
					int iy = (int)(((long)y * s + ((long)h * 1000 - (long)scr.h * s) / 2) / 1000);
					if (ix >= w) ix = w - 1;
					if (iy >= h) iy = h - 1;
					if (bg_rotated) {
						ix = w - 1 - ix;
						iy = h - 1 - iy;
					}
					unsigned char *p = im + ((size_t)iy * w + ix) * 3;
					c = 0xFF000000u | (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
					c = blend(c, 0, 100);   /* dim */
				}
				bg_cache[(size_t)y * scr.w + x] = c;
			}
		if (im)
			stbi_image_free(im);
	}
	memcpy(scr.px, bg_cache, (size_t)scr.w * scr.h * 4);
}

/* ---- Text ----------------------------------------------------------------- */

static stbtt_fontinfo font;
static unsigned char *font_data;

static int load_font(void) {
	char fallback1[600], fallback2[600];
	snprintf(fallback1, sizeof fallback1, "%s/frogui/onion/fonts/Exo-2-Bold-Italic.ttf", sd);
	snprintf(fallback2, sizeof fallback2, "%s/frogui/fonts/alium_Nunito.ttf", sd);
	const char *paths[] = {font_path, fallback1, fallback2, getenv("UI_FONT"), NULL};
	for (int i = 0; i < 4; i++) {
		if (!paths[i] || !paths[i][0])
			continue;
		FILE *f = fopen(paths[i], "rb");
		if (!f)
			continue;
		fseek(f, 0, SEEK_END);
		long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		font_data = n > 0 ? malloc((size_t)n) : NULL;
		if (font_data && fread(font_data, 1, (size_t)n, f) == (size_t)n &&
		    stbtt_InitFont(&font, font_data, stbtt_GetFontOffsetForIndex(font_data, 0))) {
			fclose(f);
			return 1;
		}
		free(font_data);
		font_data = NULL;
		fclose(f);
	}
	return 0;
}

static int utf8_next(const char **s) {
	const unsigned char *p = (const unsigned char *)*s;
	int c = *p++;
	if (c >= 0xF0 && p[0] && p[1] && p[2]) {
		c = (c & 7) << 18 | (p[0] & 63) << 12 | (p[1] & 63) << 6 | (p[2] & 63);
		p += 3;
	} else if (c >= 0xE0 && p[0] && p[1]) {
		c = (c & 15) << 12 | (p[0] & 63) << 6 | (p[1] & 63);
		p += 2;
	} else if (c >= 0xC0 && p[0]) {
		c = (c & 31) << 6 | (p[0] & 63);
		p += 1;
	}
	*s = (const char *)p;
	return c;
}

/* Width of `n` bytes of text (all of it if n < 0) at pixel size px. */
static int text_width(const char *s, int n, int px) {
	float sc = stbtt_ScaleForPixelHeight(&font, (float)px), x = 0;
	const char *end = n < 0 ? s + strlen(s) : s + n;
	int prev = 0;
	while (s < end && *s) {
		int c = utf8_next(&s), adv, lsb;
		if (prev)
			x += sc * stbtt_GetCodepointKernAdvance(&font, prev, c);
		stbtt_GetCodepointHMetrics(&font, c, &adv, &lsb);
		x += sc * adv;
		prev = c;
	}
	return (int)(x + 0.5f);
}

/* Draws n bytes of s with its top at y; returns the pen x after it. */
static int draw_text_n(int x, int y, const char *s, int n, int px, uint32_t color) {
	float sc = stbtt_ScaleForPixelHeight(&font, (float)px), pen = (float)x;
	int asc, desc, gap;
	stbtt_GetFontVMetrics(&font, &asc, &desc, &gap);
	int base = y + (int)(asc * sc + 0.5f);
	const char *end = n < 0 ? s + strlen(s) : s + n;
	int prev = 0;
	while (s < end && *s) {
		int c = utf8_next(&s), adv, lsb;
		if (prev)
			pen += sc * stbtt_GetCodepointKernAdvance(&font, prev, c);
		stbtt_GetCodepointHMetrics(&font, c, &adv, &lsb);
		int x0, y0, x1, y1;
		float fx = pen - (int)pen;
		stbtt_GetCodepointBitmapBoxSubpixel(&font, c, sc, sc, fx, 0, &x0, &y0, &x1, &y1);
		int gw = x1 - x0, gh = y1 - y0;
		if (gw > 0 && gh > 0 && gw < 512 && gh < 512) {
			unsigned char *g = malloc((size_t)gw * gh);
			if (g) {
				stbtt_MakeCodepointBitmapSubpixel(&font, g, gw, gh, gw, sc, sc, fx, 0, c);
				for (int yy = 0; yy < gh; yy++)
					for (int xx = 0; xx < gw; xx++) {
						int X = (int)pen + x0 + xx, Y = base + y0 + yy;
						unsigned a = g[yy * gw + xx];
						if (a && X >= 0 && Y >= 0 && X < scr.w && Y < scr.h) {
							uint32_t *p = &scr.px[(size_t)Y * scr.w + X];
							*p = blend(*p, color, a);
						}
					}
				free(g);
			}
		}
		pen += sc * adv;
		prev = c;
	}
	return (int)pen;
}

static void draw_text(int x, int y, const char *s, int px, uint32_t color) {
	draw_text_n(x, y, s, -1, px, color);
}

/* Text cut to fit `w`, with "..." when it doesn't. */
static void draw_text_fit(int x, int y, int w, const char *s, int px, uint32_t color) {
	if (text_width(s, -1, px) <= w) {
		draw_text(x, y, s, px, color);
		return;
	}
	int dots = text_width("...", -1, px), n = (int)strlen(s);
	while (n > 0 && text_width(s, n, px) + dots > w)
		n--;
	while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80)
		n--;   /* not inside a UTF-8 character */
	int pen = draw_text_n(x, y, s, n, px, color);
	draw_text(pen, y, "...", px, color);
}

/* Word-wrapped text in a w-wide column, each line centred if `centre`.
 * Returns the height used; draws nothing when `dry`. Honours "\n". */
static int draw_wrapped(int x, int y, int w, const char *s, int px, uint32_t color, int centre, int dry) {
	int line_h = px * 13 / 10, h = 0;
	while (*s) {
		const char *nl = strchr(s, '\n');
		int seg = nl ? (int)(nl - s) : (int)strlen(s);
		int n = seg;
		if (text_width(s, n, px) > w) {   /* longest prefix ending at a space */
			int last_space = -1;
			for (int i = 0; i < seg && text_width(s, i, px) <= w; i++)
				if (s[i] == ' ')
					last_space = i;
			if (last_space > 0)
				n = last_space;
			else
				for (n = seg; n > 1 && text_width(s, n, px) > w; n--)
					;
		}
		if (!dry) {
			int lw = text_width(s, n, px);
			draw_text_n(centre ? x + (w - lw) / 2 : x, y + h, s, n, px, color);
		}
		h += line_h;
		s += n;
		while (*s == ' ')
			s++;
		if (*s == '\n' && n == seg)
			s++;
	}
	return h;
}

/* ---- Common layout -------------------------------------------------------- */

static int body_top(void) { return SC(72); }
static int body_bottom(void) { return scr.h - SC(56); }

static void draw_frame(const char *title) {
	draw_background();
	if (title && *title) {
		fill(0, 0, scr.w, SC(60), 0, 120);
		draw_text_fit(SC(24), SC(14), scr.w - SC(48), title, SC(30), c_text);
	}
}

/* Footer hints: pairs of (button, label), NULL-terminated. */
static void draw_hints(const char *const *pairs) {
	int y = scr.h - SC(48), h = SC(48);
	fill(0, y, scr.w, h, 0, 120);
	int x = scr.w - SC(16), size = SC(20);
	int n = 0;
	while (pairs[n])
		n += 2;
	for (int i = n - 2; i >= 0; i -= 2) {   /* right-aligned, last pair rightmost */
		int lw = text_width(pairs[i + 1], -1, size);
		int bw = text_width(pairs[i], -1, size) + SC(16);
		x -= lw;
		draw_text(x, y + (h - size) / 2 - SC(2), pairs[i + 1], size, c_text);
		x -= SC(8) + bw;
		fill_round(x, y + SC(10), bw, h - SC(20), (h - SC(20)) / 2, c_text, 230);
		draw_text(x + SC(8), y + (h - size) / 2 - SC(2), pairs[i], size, c_bg);
		x -= SC(20);
	}
}

/* ---- Commands ------------------------------------------------------------- */

static int cmd_message(const char *title, const char *text, int wait) {
	draw_frame(title);
	int w = scr.w - SC(96);
	int top = body_top(), bottom = wait ? body_bottom() : scr.h - SC(16);
	int th = draw_wrapped(0, 0, w, text, SC(26), c_text, 1, 1);
	int y = top + ((bottom - top) - th) / 2;
	draw_wrapped(SC(48), y < top ? top : y, w, text, SC(26), c_text, 1, 0);
	if (wait) {
		static const char *const hints[] = {"A", "OK", NULL};
		draw_hints(hints);
	}
	present();
	if (!wait)
		return 0;
	for (;;) {
		uint32_t b = wait_press();
		if (b == B_A)
			return 0;
		if (b == B_B)
			return 1;
	}
}

static int cmd_progress(const char *title, int pct, const char *text) {
	if (pct < 0) pct = 0;
	if (pct > 100) pct = 100;
	draw_frame(title);
	int bw = scr.w * 7 / 10, bh = SC(18), bx = (scr.w - bw) / 2, by = scr.h / 2 + SC(10);
	if (text && *text) {
		int w = scr.w - SC(96);
		int th = draw_wrapped(0, 0, w, text, SC(26), c_text, 1, 1);
		draw_wrapped(SC(48), by - SC(24) - th, w, text, SC(26), c_text, 1, 0);
	}
	fill_round(bx, by, bw, bh, bh / 2, blend(c_bg, c_text, 60), 255);
	if (pct > 0) {
		int fw = bw * pct / 100;
		fill_round(bx, by, fw < bh ? bh : fw, bh, bh / 2, c_sel, 255);
	}
	char p[8];
	snprintf(p, sizeof p, "%d%%", pct);
	draw_text((scr.w - text_width(p, -1, SC(22))) / 2, by + bh + SC(10), p, SC(22), c_text);
	present();
	return 0;
}

static int cmd_confirm(const char *title, const char *text, const char *yes, const char *no) {
	int sel = 1;   /* "No" first: a stray A press must not confirm something destructive */
	for (;;) {
		draw_frame(title);
		int w = scr.w - SC(96), top = body_top(), bottom = body_bottom() - SC(80);
		int th = draw_wrapped(0, 0, w, text, SC(26), c_text, 1, 1);
		int y = top + ((bottom - top) - th) / 2;
		draw_wrapped(SC(48), y < top ? top : y, w, text, SC(26), c_text, 1, 0);
		const char *labels[2] = {yes, no};
		int bw = SC(170), bh = SC(52), gap = SC(28);
		int bx = (scr.w - bw * 2 - gap) / 2, by = body_bottom() - SC(76);
		for (int i = 0; i < 2; i++) {
			int x = bx + i * (bw + gap);
			int on = i == sel;
			fill_round(x, by, bw, bh, bh / 2, on ? c_sel : 0, on ? 255 : 110);
			int lw = text_width(labels[i], -1, SC(24));
			draw_text(x + (bw - lw) / 2, by + (bh - SC(24)) / 2 - SC(2), labels[i], SC(24),
			          on ? c_sel_text : c_text);
		}
		static const char *const hints[] = {"A", "Select", "B", "Back", NULL};
		draw_hints(hints);
		present();
		uint32_t b = wait_press();
		if (b == B_LEFT || b == B_RIGHT || b == B_UP || b == B_DOWN)
			sel = !sel;
		else if (b == B_A)
			return sel == 0 ? 0 : 1;
		else if (b == B_B)
			return 1;
	}
}

static int cmd_menu(const char *title, char **items, int n, int print_index, int sel) {
	if (n <= 0)
		return 2;
	if (sel < 0 || sel >= n)
		sel = 0;
	int row = SC(48), top = body_top(), rows = (body_bottom() - top - SC(8)) / row, scroll = 0;
	for (;;) {
		if (sel < scroll) scroll = sel;
		if (sel >= scroll + rows) scroll = sel - rows + 1;
		draw_frame(title);
		for (int i = 0; i < rows && scroll + i < n; i++) {
			int idx = scroll + i, y = top + i * row, on = idx == sel;
			if (on)
				fill_round(SC(16), y, scr.w - SC(32), row - SC(4), (row - SC(4)) / 2, c_sel, 255);
			draw_text_fit(SC(36), y + (row - SC(4) - SC(26)) / 2 - SC(2), scr.w - SC(72),
			              items[idx], SC(26), on ? c_sel_text : c_text);
		}
		if (n > rows) {   /* position, bottom right of the list */
			char pos[24];
			snprintf(pos, sizeof pos, "%d/%d", sel + 1, n);
			draw_text(SC(20), scr.h - SC(48) + SC(12), pos, SC(20), c_text);
		}
		static const char *const hints[] = {"A", "Select", "B", "Back", NULL};
		draw_hints(hints);
		present();
		uint32_t b = wait_press();
		if (b == B_UP) sel = sel > 0 ? sel - 1 : n - 1;
		else if (b == B_DOWN) sel = sel < n - 1 ? sel + 1 : 0;
		else if (b == B_LEFT || b == B_L) sel = sel - rows < 0 ? 0 : sel - rows;
		else if (b == B_RIGHT || b == B_R) sel = sel + rows >= n ? n - 1 : sel + rows;
		else if (b == B_A) {
			if (print_index)
				printf("%d\n", sel);
			else
				printf("%s\n", items[sel]);
			return 0;
		} else if (b == B_B)
			return 1;
	}
}

static int cmd_keyboard(const char *title, const char *initial) {
	static const char *rows_lc[] = {"1234567890", "qwertyuiop", "asdfghjkl'", "zxcvbnm,.-"};
	static const char *rows_uc[] = {"!@#$%^&*()", "QWERTYUIOP", "ASDFGHJKL\"", "ZXCVBNM;:_"};
	static const char *special[] = {"Shift", "Space", "Delete", "Done"};
	char text[256];
	snprintf(text, sizeof text, "%s", initial ? initial : "");
	int shift = 0, r = 1, c = 0;
	for (;;) {
		draw_frame(title);
		/* the text field */
		int fx = SC(32), fy = body_top() + SC(4), fw = scr.w - SC(64), fh = SC(52);
		fill_round(fx, fy, fw, fh, SC(12), 0, 140);
		int tw = text_width(text, -1, SC(28)), avail = fw - SC(32);
		const char *shown = text;
		while (tw > avail && *shown) {   /* keep the end in view */
			shown++;
			while (((unsigned char)*shown & 0xC0) == 0x80)
				shown++;
			tw = text_width(shown, -1, SC(28));
		}
		int pen = draw_text_n(fx + SC(16), fy + (fh - SC(28)) / 2 - SC(2), shown, -1, SC(28), c_text);
		fill(pen + SC(2), fy + SC(10), SC(3), fh - SC(20), c_sel, 255);   /* cursor */
		/* the keys: 4 rows of 10, then the special row of 4 */
		const char **rows = shift ? rows_uc : rows_lc;
		int kw = (scr.w - SC(64)) / 10, kh = SC(48), kx = SC(32), ky = fy + fh + SC(14);
		for (int y = 0; y < 5; y++) {
			int cols = y < 4 ? 10 : 4, w = y < 4 ? kw : (kw * 10) / 4;
			for (int x = 0; x < cols; x++) {
				int on = y == r && x == c;
				int X = kx + x * w, Y = ky + y * kh;
				fill_round(X + SC(3), Y + SC(3), w - SC(6), kh - SC(6), SC(10), on ? c_sel : 0, on ? 255 : 110);
				char ch[2] = {y < 4 ? rows[y][x] : 0, 0};
				const char *label = y < 4 ? ch : special[x];
				int lw = text_width(label, -1, SC(24));
				draw_text(X + (w - lw) / 2, Y + (kh - SC(24)) / 2 - SC(2), label, SC(24),
				          on ? c_sel_text : c_text);
			}
		}
		static const char *const hints[] = {"A", "Type", "Y", "Delete", "X", "Shift", "START", "Done", "B", "Cancel", NULL};
		draw_hints(hints);
		present();
		uint32_t b = wait_press();
		int cols = r < 4 ? 10 : 4;
		if (b == B_UP || b == B_DOWN) {
			int nr = (r + (b == B_UP ? 4 : 1)) % 5;
			/* keep the column roughly in place between the 10- and 4-key rows */
			if (r < 4 && nr == 4) c = c * 4 / 10;
			else if (r == 4 && nr < 4) c = c * 10 / 4 + 1;
			r = nr;
			if (c >= (r < 4 ? 10 : 4)) c = (r < 4 ? 10 : 4) - 1;
		} else if (b == B_LEFT) c = (c + cols - 1) % cols;
		else if (b == B_RIGHT) c = (c + 1) % cols;
		else if (b == B_X || b == B_L || b == B_R) shift = !shift;
		else if (b == B_Y) {
			size_t n = strlen(text);
			while (n > 0 && ((unsigned char)text[n - 1] & 0xC0) == 0x80)
				n--;
			if (n > 0)
				text[n - 1] = '\0';
		} else if (b == B_START) {
			printf("%s\n", text);
			return 0;
		} else if (b == B_B)
			return 1;
		else if (b == B_A) {
			size_t n = strlen(text);
			if (r < 4) {
				if (n + 1 < sizeof text) {
					text[n] = rows[r][c];
					text[n + 1] = '\0';
				}
			} else if (c == 0) shift = !shift;
			else if (c == 1) {
				if (n + 1 < sizeof text) {
					text[n] = ' ';
					text[n + 1] = '\0';
				}
			} else if (c == 2) {
				if (n > 0) text[n - 1] = '\0';
			} else {
				printf("%s\n", text);
				return 0;
			}
		}
	}
}

/* A toast over whatever is on screen, then the screen as it was. */
static int cmd_notify(const char *text, int seconds) {
	for (int y = 0; y < scr.h; y++)
		memcpy(scr.px + (size_t)y * scr.w, scr.fb + (size_t)y * scr.fb_line, (size_t)scr.w * 4);
	uint32_t *saved = malloc((size_t)scr.w * scr.h * 4);
	if (saved)
		memcpy(saved, scr.px, (size_t)scr.w * scr.h * 4);
	int w = scr.w - SC(120), size = SC(24);
	int th = draw_wrapped(0, 0, w - SC(40), text, size, c_text, 1, 1);
	int ch = th + SC(28), cy = scr.h - SC(40) - ch;
	/* a fully transparent screen has no background: give the toast one */
	fill_round(SC(60), cy, w, ch, SC(18), c_bg, 235);
	draw_wrapped(SC(80), cy + SC(14), w - SC(40), text, size, c_text, 1, 0);
	present();
#ifdef UI_HOST
	(void)seconds;
	write_shot();
#else
	long long end = now_ms() + (long long)seconds * 1000;
	while (now_ms() < end) {
		present();
		usleep(100000);
	}
#endif
	if (saved) {
		memcpy(scr.px, saved, (size_t)scr.w * scr.h * 4);
		present();
		free(saved);
	}
	return 0;
}

static int cmd_clear(void) {
	memset(scr.fb, 0, scr.fb_len);
	return 0;
}

/* ---- Arguments ------------------------------------------------------------ */

static int usage(void) {
	fprintf(stderr,
	        "usage: shallots_ui message [--title T] [--wait] TEXT\n"
	        "       shallots_ui progress [--title T] PERCENT [TEXT]\n"
	        "       shallots_ui confirm [--title T] [--yes L] [--no L] TEXT\n"
	        "       shallots_ui menu [--title T] [--index] [--selected N] ITEM...\n"
	        "       shallots_ui keyboard [--title T] [--initial TEXT]\n"
	        "       shallots_ui notify [--seconds N] TEXT\n"
	        "       shallots_ui clear\n");
	return 2;
}

int main(int argc, char **argv) {
	if (argc < 2)
		return usage();
	if (getenv("SHALLOTS_SD"))
		sd = getenv("SHALLOTS_SD");
#ifdef UI_HOST
	host_keys = getenv("UI_KEYS");
#endif
	const char *cmd = argv[1], *title = NULL, *yes = "Yes", *no = "No", *initial = NULL;
	int wait = 0, print_index = 0, selected = 0, seconds = 2;
	char **pos = calloc((size_t)argc, sizeof *pos);
	int npos = 0;
	if (!pos)
		return 2;
	for (int i = 2; i < argc; i++) {
		const char *a = argv[i];
		int more = i + 1 < argc;
		if (!strcmp(a, "--title") && more) title = argv[++i];
		else if (!strcmp(a, "--yes") && more) yes = argv[++i];
		else if (!strcmp(a, "--no") && more) no = argv[++i];
		else if (!strcmp(a, "--initial") && more) initial = argv[++i];
		else if (!strcmp(a, "--selected") && more) selected = atoi(argv[++i]);
		else if (!strcmp(a, "--seconds") && more) seconds = atoi(argv[++i]);
		else if (!strcmp(a, "--wait")) wait = 1;
		else if (!strcmp(a, "--index")) print_index = 1;
		else if (!strcmp(a, "--")) {
			while (++i < argc)
				pos[npos++] = argv[i];
		} else pos[npos++] = argv[i];
	}
	static const char *const known[] = {"message", "progress", "confirm", "menu", "keyboard", "notify", "clear"};
	int ok = 0;
	for (size_t i = 0; i < sizeof known / sizeof known[0]; i++)
		ok |= !strcmp(cmd, known[i]);
	if (!ok)
		return usage();

	if (!screen_open()) {
		/* No usable screen: say it on stderr; interactive commands can't answer. */
		if (npos)
			fprintf(stderr, "shallots_ui: %s\n", pos[npos - 1]);
		return 3;
	}
	S100 = scr.h * 100 / 480;
	if (!strcmp(cmd, "clear"))
		return cmd_clear();
	load_theme();
	if (!load_font()) {
		fprintf(stderr, "shallots_ui: no font found\n");
		return 4;
	}

	int rc;
	if (!strcmp(cmd, "message"))
		rc = npos ? cmd_message(title, pos[0], wait) : usage();
	else if (!strcmp(cmd, "progress"))
		rc = npos ? cmd_progress(title, atoi(pos[0]), npos > 1 ? pos[1] : NULL) : usage();
	else if (!strcmp(cmd, "confirm"))
		rc = npos ? cmd_confirm(title, pos[0], yes, no) : usage();
	else if (!strcmp(cmd, "menu"))
		rc = npos ? cmd_menu(title, pos, npos, print_index, selected) : usage();
	else if (!strcmp(cmd, "keyboard"))
		rc = cmd_keyboard(title, initial);
	else
		rc = npos ? cmd_notify(pos[0], seconds > 0 ? seconds : 2) : usage();
#ifdef UI_HOST
	if (strcmp(cmd, "notify"))
		write_shot();
#endif
	fflush(stdout);
	return rc;
}
