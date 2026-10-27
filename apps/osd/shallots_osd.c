/*
 * shallots_osd - ShallotsOS's volume, brightness and low-battery popups.
 *
 * The stock daemon cubevol owns the buttons, volume, brightness and battery,
 * and draws its popups on the /dev/fb1 overlay with LVGL. Painting over them
 * blinks: the hardware composites fb1 continuously, so cubevol's pixels show
 * between its write and ours. Restarting cubevol to stop it has upset its
 * button handling before (volume-down needing repeated presses).
 *
 * So cubevol keeps running untouched, except for one thing: it maps fb1 once
 * at startup (fb_framebuffer_init: open, ioctl, mmap, close) and only draws
 * through that mapping afterwards. This helper swaps that mapping in the live
 * process for anonymous memory of the same size (an injected mmap(MAP_FIXED)
 * through ptrace, done at cubevol's ELF entry point and fully restored). From
 * then on cubevol draws where nobody sees it, and this helper draws its own
 * popups on fb1 when the volume or brightness changes, in the colours of the
 * active theme (cubegm/skin/skin.txt, written by FrogUI).
 *
 * If cubevol dies right after the swap, the helper restarts it and records
 * the failure in frogui/.osd_failed; it never swaps again while that file
 * exists. Started by FrogUI (from a /tmp copy, so the SD card can unmount for
 * USB mode) when Settings > ShallotsOS Volume Indicator is on.
 *
 *   shallots_osd            run as the daemon
 *   shallots_osd --preview  (host build) draw each popup into preview-*.ppm
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifndef OSD_PREVIEW
#include <linux/fb.h>
#include <sys/ptrace.h>
#endif

#define SD "/mnt/sdcard"
#define SETTINGS_FILE SD "/frogui/settings.txt"
#define SKIN_FILE SD "/cubegm/skin/skin.txt"
#define FAILED_FILE SD "/frogui/.osd_failed"
#define LOG_ENABLE SD "/log.txt"
#define LOG_FILE SD "/logs/shallots_osd.log"
#define PID_FILE "/tmp/shallots_osd.pid"
#define MEDIA_MARKER "/tmp/shallotsos_media_osd"   /* the media player draws its own */


static long long now_ms(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void logf_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void logf_(const char *fmt, ...) {
	if (access(LOG_ENABLE, F_OK) != 0)
		return;
	mkdir(SD "/logs", 0777);
	FILE *f = fopen(LOG_FILE, "a");
	if (!f)
		return;
	va_list ap;
	va_start(ap, fmt);
	fprintf(f, "[%lld] ", now_ms() / 1000);
	vfprintf(f, fmt, ap);
	fputc('\n', f);
	va_end(ap);
	fclose(f);
}

/* ---- Settings and colours ------------------------------------------------- */

/* "key=value" lines; returns 1 and copies the value when the key is there. */
static int file_value(const char *path, const char *key, char *out, size_t n) {
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;
	char line[256];
	size_t kl = strlen(key);
	int found = 0;
	while (fgets(line, sizeof line, f)) {
		if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
			snprintf(out, n, "%s", line + kl + 1);
			out[strcspn(out, "\r\n")] = '\0';
			found = 1;
			break;
		}
	}
	fclose(f);
	return found;
}

#ifndef OSD_PREVIEW
static int osd_enabled(void) {
	char v[32];
	return !(file_value(SETTINGS_FILE, "osd", v, sizeof v) && strcmp(v, "stock") == 0);
}
#endif

static uint32_t c_card = 0xE6141018u;   /* ARGB: background */
static uint32_t c_text = 0xFFFFFFFFu;   /* icon and digits */
static uint32_t c_fill = 0xFFB06AE0u;   /* bar: the theme's selection colour */

static void load_colours(void) {
	char v[32];
	if (file_value(SKIN_FILE, "background_color", v, sizeof v))
		c_card = 0xE6000000u | (strtoul(v, NULL, 16) & 0xFFFFFFu);
	if (file_value(SKIN_FILE, "text_color", v, sizeof v))
		c_text = 0xFF000000u | (strtoul(v, NULL, 16) & 0xFFFFFFu);
	if (file_value(SKIN_FILE, "selection_color", v, sizeof v))
		c_fill = 0xFF000000u | (strtoul(v, NULL, 16) & 0xFFFFFFu);
	/* A selection colour too close to the card would hide the bar. */
	int dr = (int)((c_fill >> 16) & 255) - (int)((c_card >> 16) & 255);
	int dg = (int)((c_fill >> 8) & 255) - (int)((c_card >> 8) & 255);
	int db = (int)(c_fill & 255) - (int)(c_card & 255);
	if (dr * dr + dg * dg + db * db < 60 * 60)
		c_fill = c_text;
}

/* ---- Drawing (ARGB8888 canvas) -------------------------------------------- */

typedef struct {
	uint32_t *px;
	int w, h, stride;   /* stride in pixels */
} Canvas;

static inline void put(Canvas *c, int x, int y, uint32_t argb) {
	if (x >= 0 && y >= 0 && x < c->w && y < c->h)
		c->px[(size_t)y * c->stride + x] = argb;
}

static void fill_round(Canvas *c, int x, int y, int w, int h, int r, uint32_t argb) {
	for (int py = 0; py < h; py++)
		for (int px = 0; px < w; px++) {
			int dx = px < r ? r - px : (px >= w - r ? px - (w - r - 1) : 0);
			int dy = py < r ? r - py : (py >= h - r ? py - (h - r - 1) : 0);
			if (dx * dx + dy * dy <= r * r)
				put(c, x + px, y + py, argb);
		}
}

/* `top` mixed into `base` by amount a (0..255), keeping base's alpha. */
static uint32_t mix(uint32_t base, uint32_t top, unsigned a) {
	uint32_t out = base & 0xFF000000u;
	for (int sh = 0; sh < 24; sh += 8) {
		unsigned cb = (base >> sh) & 255, ct = (top >> sh) & 255;
		out |= ((ct * a + cb * (255 - a)) / 255) << sh;
	}
	return out;
}

/* 5x7 digits and '%', drawn at 2x. */
static const uint8_t glyphs[11][7] = {
	{0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
	{0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
	{0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
	{0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
	{0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
	{0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03},
};

static int draw_glyph(Canvas *c, int x, int y, int g, uint32_t argb) {
	for (int r = 0; r < 7; r++)
		for (int b = 0; b < 5; b++)
			if (glyphs[g][r] & (0x10 >> b))
				for (int sy = 0; sy < 2; sy++)
					for (int sx = 0; sx < 2; sx++)
						put(c, x + b * 2 + sx, y + r * 2 + sy, argb);
	return 12;
}

static int number_width(int v) {
	int n = v >= 100 ? 3 : v >= 10 ? 2 : 1;
	return n * 12 + 12;   /* digits + '%' */
}

static void draw_number(Canvas *c, int x, int y, int v, uint32_t argb) {
	char s[12];
	snprintf(s, sizeof s, "%d", v);
	for (char *p = s; *p; p++)
		x += draw_glyph(c, x, y, *p - '0', argb);
	draw_glyph(c, x, y, 10, argb);
}

static void icon_speaker(Canvas *c, int x, int y, int level, uint32_t argb) {
	int cy = y + 14;
	for (int py = cy - 5; py <= cy + 5; py++)
		for (int px = x + 2; px < x + 8; px++)
			put(c, px, py, argb);
	for (int k = 0; k < 8; k++)   /* the cone */
		for (int py = cy - 5 - k; py <= cy + 5 + k; py++)
			put(c, x + 8 + k, py, argb);
	if (level == 0) {   /* muted: a cross */
		for (int k = -5; k <= 5; k++)
			for (int t = 0; t < 2; t++) {
				put(c, x + 23 + k + t, cy + k, argb);
				put(c, x + 23 + k + t, cy - k, argb);
			}
		return;
	}
	int waves = level > 66 ? 3 : level > 33 ? 2 : 1;
	for (int wv = 0; wv < waves; wv++) {
		int r = 5 + wv * 5;
		for (int a = -r; a <= r; a++) {
			int dy = a, dx2 = r * r - dy * dy;
			int dx = 0;
			while ((dx + 1) * (dx + 1) <= dx2)
				dx++;
			put(c, x + 17 + dx, cy + dy, argb);
			put(c, x + 18 + dx, cy + dy, argb);
		}
	}
}

static void icon_sun(Canvas *c, int x, int y, uint32_t argb) {
	int cx = x + 14, cy = y + 14;
	for (int py = -6; py <= 6; py++)
		for (int px = -6; px <= 6; px++)
			if (px * px + py * py <= 36)
				put(c, cx + px, cy + py, argb);
	static const int dir[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
	for (int d = 0; d < 8; d++)
		for (int k = 9; k <= 13; k++) {
			int kx = dir[d][0] * k, ky = dir[d][1] * k;
			if (dir[d][0] && dir[d][1]) {
				kx = kx * 7 / 10;
				ky = ky * 7 / 10;
			}
			put(c, cx + kx, cy + ky, argb);
			put(c, cx + kx + 1, cy + ky, argb);
		}
}

static void icon_battery(Canvas *c, int x, int y, int pct, uint32_t argb) {
	int bx = x, by = y + 8, bw = 24, bh = 13;
	for (int px = 0; px < bw; px++) {
		put(c, bx + px, by, argb);
		put(c, bx + px, by + 1, argb);
		put(c, bx + px, by + bh - 1, argb);
		put(c, bx + px, by + bh - 2, argb);
	}
	for (int py = 0; py < bh; py++) {
		put(c, bx, by + py, argb);
		put(c, bx + 1, by + py, argb);
		put(c, bx + bw - 1, by + py, argb);
		put(c, bx + bw - 2, by + py, argb);
	}
	for (int py = by + 4; py < by + bh - 4; py++)
		for (int px = bx + bw; px < bx + bw + 3; px++)
			put(c, px, py, argb);
	int fw = (bw - 8) * pct / 100;
	if (fw < 2)
		fw = 2;
	for (int py = by + 4; py < by + bh - 4; py++)
		for (int px = bx + 4; px < bx + 4 + fw; px++)
			put(c, px, py, 0xFFE04040u);
}

enum { KIND_VOLUME, KIND_BRIGHTNESS, KIND_BATTERY };
#define CARD_W 300
#define CARD_H 56
#define CARD_Y 28

/* The popup, drawn into a CARD_W x CARD_H canvas. */
static void draw_popup(Canvas *c, int kind, int value) {
	for (int y = 0; y < c->h; y++)
		for (int x = 0; x < c->w; x++)
			c->px[(size_t)y * c->stride + x] = 0;
	fill_round(c, 0, 0, CARD_W, CARD_H, 16, c_card);
	int ix = 16, iy = (CARD_H - 28) / 2;
	if (kind == KIND_VOLUME)
		icon_speaker(c, ix, iy, value, c_text);
	else if (kind == KIND_BRIGHTNESS)
		icon_sun(c, ix, iy, c_text);
	else
		icon_battery(c, ix, iy, value, c_text);
	int nw = number_width(100);
	int bx = ix + 28 + 18, bw = CARD_W - bx - 16 - nw - 12, bh = 10;
	int by = (CARD_H - bh) / 2;
	fill_round(c, bx, by, bw, bh, bh / 2, mix(c_card, c_text, 0x50));
	int fw = bw * value / 100;
	if (fw > 0)
		fill_round(c, bx, by, fw < bh ? bh : fw, bh, bh / 2, kind == KIND_BATTERY ? 0xFFE04040u : c_fill);
	draw_number(c, CARD_W - 16 - number_width(value), (CARD_H - 14) / 2, value, c_text);
}

#ifdef OSD_PREVIEW
/* Host preview: each popup over a grey checkerboard, as PPM files. */
static void preview(const char *name, int kind, int value) {
	uint32_t px[CARD_W * CARD_H];
	Canvas c = {px, CARD_W, CARD_H, CARD_W};
	draw_popup(&c, kind, value);
	FILE *f = fopen(name, "wb");
	fprintf(f, "P6\n%d %d\n255\n", CARD_W + 40, CARD_H + 40);
	for (int y = 0; y < CARD_H + 40; y++)
		for (int x = 0; x < CARD_W + 40; x++) {
			unsigned bg = ((x / 10 + y / 10) & 1) ? 90 : 60;
			unsigned r = bg, g = bg, b = bg;
			int cx = x - 20, cy = y - 20;
			if (cx >= 0 && cy >= 0 && cx < CARD_W && cy < CARD_H) {
				uint32_t p = px[cy * CARD_W + cx];
				unsigned a = p >> 24;
				r = (((p >> 16) & 255) * a + r * (255 - a)) / 255;
				g = (((p >> 8) & 255) * a + g * (255 - a)) / 255;
				b = ((p & 255) * a + b * (255 - a)) / 255;
			}
			fputc(r, f);
			fputc(g, f);
			fputc(b, f);
		}
	fclose(f);
}

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;
	load_colours();
	preview("preview-volume.ppm", KIND_VOLUME, 65);
	preview("preview-mute.ppm", KIND_VOLUME, 0);
	preview("preview-brightness.ppm", KIND_BRIGHTNESS, 100);
	preview("preview-battery.ppm", KIND_BATTERY, 8);
	return 0;
}
#else

/* ---- The overlay ---------------------------------------------------------- */

static struct {
	uint8_t *mem;
	size_t len;
	int fd, w, h, line;
	int shown;
	long long until;
} fb1 = {.fd = -1};

static int fb1_open(void) {
	if (fb1.mem)
		return 1;
	char rot[16];
	if (file_value("/tmp/tfdevice.env", "TF_ROTATE", rot, sizeof rot) && atoi(rot) != 0)
		return 0;   /* rotated overlays (SF3500-class) are not handled */
	int fd = open("/dev/fb1", O_RDWR);
	if (fd < 0)
		return 0;
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	if (ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f) ||
	    v.bits_per_pixel != 32 || v.xres < v.yres || v.xres < CARD_W || f.smem_len == 0) {
		close(fd);
		return 0;
	}
	void *m = mmap(NULL, f.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (m == MAP_FAILED) {
		close(fd);
		return 0;
	}
	fb1.mem = m;
	fb1.len = f.smem_len;
	fb1.fd = fd;
	fb1.w = v.xres;
	fb1.h = v.yres;
	fb1.line = f.line_length;
	logf_("fb1 %dx%d virtual %dx%d offset %d, line %d, memory %u", v.xres, v.yres, v.xres_virtual,
	      v.yres_virtual, v.yoffset, f.line_length, f.smem_len);
	return 1;
}

static uint32_t *card_origin(void) {
	int x = (fb1.w - CARD_W) / 2;
	return (uint32_t *)(fb1.mem + (size_t)CARD_Y * fb1.line) + x;
}

static void popup_show(int kind, int value, int msec) {
	if (access(MEDIA_MARKER, F_OK) == 0 || !fb1_open())
		return;
	static uint32_t px[CARD_W * CARD_H];
	Canvas c = {px, CARD_W, CARD_H, CARD_W};
	draw_popup(&c, kind, value);
	uint32_t *dst = card_origin();
	for (int y = 0; y < CARD_H; y++)
		memcpy(dst + (size_t)y * (fb1.line / 4), px + y * CARD_W, CARD_W * 4);
	if (!fb1.shown)
		ioctl(fb1.fd, FBIOBLANK, FB_BLANK_UNBLANK);
	fb1.shown = 1;
	fb1.until = now_ms() + msec;
}

/* cubevol shows cubegm/cubepoweroff.bmp on fb1 when the console powers off;
 * that drawing is now out of sight, so show it here. Shutdown sends SIGTERM
 * to every process. Uncompressed 24/32-bit BMP, centred, black around it. */
#define POWEROFF_BMP SD "/cubegm/cubepoweroff.bmp"

static void show_poweroff(void) {
	if (!fb1_open())
		return;
	for (int y = 0; y < fb1.h; y++)
		for (int x = 0; x < fb1.w; x++)
			((uint32_t *)(fb1.mem + (size_t)y * fb1.line))[x] = 0xFF000000u;
	FILE *f = fopen(POWEROFF_BMP, "rb");
	unsigned char h[54];
	if (f && fread(h, 1, sizeof h, f) == sizeof h && h[0] == 'B' && h[1] == 'M') {
		uint32_t off = h[10] | h[11] << 8 | h[12] << 16 | (uint32_t)h[13] << 24;
		int32_t w = (int32_t)(h[18] | h[19] << 8 | h[20] << 16 | (uint32_t)h[21] << 24);
		int32_t ht = (int32_t)(h[22] | h[23] << 8 | h[24] << 16 | (uint32_t)h[25] << 24);
		int bpp = h[28] | h[29] << 8;
		uint32_t comp = h[30] | h[31] << 8 | h[32] << 16 | (uint32_t)h[33] << 24;
		int top_down = ht < 0;
		if (top_down)
			ht = -ht;
		if ((bpp == 24 || bpp == 32) && (comp == 0 || comp == 3) && w > 0 && ht > 0 &&
		    w <= 4096 && ht <= 4096) {
			int bypp = bpp / 8;
			size_t row_len = ((size_t)w * bypp + 3) & ~(size_t)3;
			unsigned char *row = malloc(row_len);
			int ox = (fb1.w - w) / 2, oy = (fb1.h - ht) / 2;
			for (int r = 0; row && r < ht; r++) {
				if (fseek(f, (long)(off + (size_t)r * row_len), SEEK_SET) || fread(row, 1, row_len, f) != row_len)
					break;
				int y = oy + (top_down ? r : ht - 1 - r);
				if (y < 0 || y >= fb1.h)
					continue;
				uint32_t *dst = (uint32_t *)(fb1.mem + (size_t)y * fb1.line);
				for (int x = 0; x < w; x++) {
					int dx = ox + x;
					if (dx < 0 || dx >= fb1.w)
						continue;
					unsigned char *p = row + (size_t)x * bypp;
					dst[dx] = 0xFF000000u | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
				}
			}
			free(row);
		}
	}
	if (f)
		fclose(f);
	ioctl(fb1.fd, FBIOBLANK, FB_BLANK_UNBLANK);
}

static volatile sig_atomic_t g_term;
static void on_term(int sig) {
	(void)sig;
	g_term = 1;
}

static void popup_tick(void) {
	if (!fb1.shown || now_ms() < fb1.until)
		return;
	uint32_t *dst = card_origin();
	for (int y = 0; y < CARD_H; y++)
		memset(dst + (size_t)y * (fb1.line / 4), 0, CARD_W * 4);
	fb1.shown = 0;
}

/* ---- Values --------------------------------------------------------------- */

/* The values the user set, as cubevol stores them (persistentmem, the same
 * slots FrogUI and the media player read). Not the live I2SO volume: FrogUI
 * mutes that while its menu has no sound, which read as "volume 0". */
struct pmem_req {
	unsigned short flag, id, len, pad;
	void *buf;
};
#define PMEM_GET 0x400C2602u

static int read_volume(void) {
	int fd = open("/dev/persistentmem", O_RDWR);
	if (fd < 0)
		return -1;
	unsigned char buf[260] = {0};
	struct pmem_req req = {3, 0, 260, 0, buf};
	int ok = ioctl(fd, PMEM_GET, &req) == 0;
	close(fd);
	return ok && buf[0] <= 100 ? buf[0] : -1;
}

/* The live output volume: it follows a held volume key step by step, while
 * cubevol may store the value only once the key is let go. Opened per read,
 * like cubevol: a held audio fd could get in the way of the frontends'
 * own audio setup. */
#define I2SO_GET_VOLUME 0x4001080Cu   /* cubevol's api_get_snd_volume: 1 byte, 0..100 */
static int read_live_volume(void) {
	int fd = open("/dev/sndC0i2so", O_WRONLY);
	if (fd < 0)
		return -1;
	unsigned char v = 0;
	int ok = ioctl(fd, I2SO_GET_VOLUME, &v) == 0;
	close(fd);
	return ok && v <= 100 ? v : -1;
}

/* Live volume, except a live 0 while a volume is stored: that is FrogUI
 * muting its menu, not the user (who reaches 0 once the key is let go). */
static int read_shown_volume(void) {
	int stored = read_volume(), live = read_live_volume();
	if (live < 0 || (live == 0 && stored > 0))
		return stored;
	return live;
}

/* Raw backlight (23..255) back to cubevol's 0..100 level. */
static int read_brightness(void) {
	int fd = open("/dev/persistentmem", O_RDWR);
	if (fd < 0)
		return -1;
	int value = 0;
	struct pmem_req req = {1, 30, 1, 0, &value};
	int ok = ioctl(fd, PMEM_GET, &req) == 0;
	close(fd);
	int raw = value & 0xFF;
	if (!ok || raw < 23)
		return -1;
	if (raw < 66)
		return raw - 23;
	int lvl = 43;
	while (lvl < 100 && (lvl + 1) * (lvl + 1) / 43 + 23 <= raw)
		lvl++;
	return lvl;
}

/* Battery percent (FrogUI's curve), -1 if unknown; charging into *chg.
 * The ADC nodes are flaky when reopened, so they stay open. */
static int read_battery(int *chg) {
	static int fd1 = -2, fd5 = -2;
	if (fd1 == -2)
		fd1 = open("/dev/check_adc1", O_RDWR);
	if (fd5 == -2)
		fd5 = open("/dev/check_adc5", O_RDWR);
	unsigned char b = 0, c = 0;
	*chg = 0;
	if (fd5 >= 0 && lseek(fd5, 0, SEEK_SET) == 0 && read(fd5, &c, 1) == 1)
		*chg = c >= 64;
	if (fd1 < 0 || lseek(fd1, 0, SEEK_SET) != 0 || read(fd1, &b, 1) != 1)
		return -1;
	int raw = b;
	if (raw <= 64)
		return 0;
	if (raw <= 153)
		return (raw - 64) * 50 / (153 - 64);
	if (raw < 180)
		return 50 + (raw - 153) * 50 / (180 - 153);
	return 100;
}

/* ---- cubevol -------------------------------------------------------------- */

static int proc_comm_is(pid_t pid, const char *name) {
	char path[64], comm[32] = "";
	snprintf(path, sizeof path, "/proc/%d/comm", (int)pid);
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;
	if (!fgets(comm, sizeof comm, f))
		comm[0] = '\0';
	fclose(f);
	comm[strcspn(comm, "\n")] = '\0';
	return strcmp(comm, name) == 0;
}

static pid_t find_cubevol(void) {
	DIR *d = opendir("/proc");
	if (!d)
		return 0;
	struct dirent *e;
	pid_t found = 0;
	while ((e = readdir(d))) {
		pid_t pid = (pid_t)atoi(e->d_name);
		if (pid > 0 && proc_comm_is(pid, "cubevol")) {
			found = pid;
			break;
		}
	}
	closedir(d);
	return found;
}

/* cubevol's /dev/fb1 mappings, from /proc/<pid>/maps. */
static int fb1_maps(pid_t pid, unsigned long *start, unsigned long *end, unsigned long *off, int max) {
	char path[64], line[256];
	snprintf(path, sizeof path, "/proc/%d/maps", (int)pid);
	FILE *f = fopen(path, "r");
	if (!f)
		return -1;
	int n = 0;
	while (n < max && fgets(line, sizeof line, f)) {
		line[strcspn(line, "\n")] = '\0';
		size_t l = strlen(line);
		if (l < 8 || strcmp(line + l - 8, "/dev/fb1") != 0)
			continue;
		if (sscanf(line, "%lx-%lx %*s %lx", &start[n], &end[n], &off[n]) == 3)
			n++;
	}
	fclose(f);
	return n;
}

/* The ELF entry point of a non-PIE executable: code that already ran and
 * never runs again, so it can hold two injected instructions for a moment. */
static unsigned long exe_entry(pid_t pid) {
	char path[64];
	unsigned char h[52];
	snprintf(path, sizeof path, "/proc/%d/exe", (int)pid);
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return 0;
	ssize_t n = read(fd, h, sizeof h);
	close(fd);
	if (n != (ssize_t)sizeof h || memcmp(h, "\177ELF", 4) != 0 || h[4] != 1 /* 32-bit */ ||
	    h[5] != 1 /* LE */ || h[16] != 2 || h[17] != 0 /* ET_EXEC */ || h[18] != 8 /* MIPS */)
		return 0;
	return (unsigned long)h[24] | (unsigned long)h[25] << 8 | (unsigned long)h[26] << 16 |
	       (unsigned long)h[27] << 24;
}

/* MIPS ptrace USER area: GPRs 0-31, then these. */
#define U_PC 64
#define U_HI 67
#define U_LO 68
#define NR_MMAP 4090   /* o32 mmap(addr, len, prot, flags, fd, offset) */
#define MIPS_SYSCALL 0x0000000Cu
#define MIPS_BREAK 0x0000000Du

static int peek_user(pid_t pid, long idx, long *out) {
	errno = 0;
	long v = ptrace(PTRACE_PEEKUSER, pid, (void *)idx, 0);
	if (errno)
		return -1;
	*out = v;
	return 0;
}

static int peek_mem(pid_t pid, unsigned long addr, long *out) {
	errno = 0;
	long v = ptrace(PTRACE_PEEKDATA, pid, (void *)addr, 0);
	if (errno)
		return -1;
	*out = v;
	return 0;
}

/* Runs mmap(addr, len, RW, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) in the
 * stopped thread `pid`, then puts every register, the two code words and the
 * two stack words back exactly. Returns 0 when the mapping landed at addr. */
static int inject_anon_map(pid_t pid, unsigned long entry, unsigned long addr, unsigned long len) {
	long regs[32], pc, hi, lo, code0, code1, st16, st20;
	for (int i = 0; i < 32; i++)
		if (peek_user(pid, i, &regs[i]))
			return -1;
	if (peek_user(pid, U_PC, &pc) || peek_user(pid, U_HI, &hi) || peek_user(pid, U_LO, &lo))
		return -1;
	unsigned long sp = (unsigned long)regs[29];
	if (peek_mem(pid, entry, &code0) || peek_mem(pid, entry + 4, &code1) ||
	    peek_mem(pid, sp + 16, &st16) || peek_mem(pid, sp + 20, &st20))
		return -1;

	int rc = -1;
	if (ptrace(PTRACE_POKETEXT, pid, (void *)entry, (void *)(long)MIPS_SYSCALL) ||
	    ptrace(PTRACE_POKETEXT, pid, (void *)(entry + 4), (void *)(long)MIPS_BREAK))
		goto restore;
	if (ptrace(PTRACE_POKEDATA, pid, (void *)(sp + 16), (void *)-1L) ||   /* fd */
	    ptrace(PTRACE_POKEDATA, pid, (void *)(sp + 20), (void *)0L))      /* offset */
		goto restore;
	/* regs[0] is the kernel's "in a syscall" flag: clear it so resuming does
	 * not try to restart the call the thread was stopped in. */
	long set[][2] = {{0, 0}, {2, NR_MMAP}, {4, (long)addr}, {5, (long)len},
	                 {6, PROT_READ | PROT_WRITE}, {7, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS},
	                 {U_PC, (long)entry}};
	for (size_t i = 0; i < sizeof set / sizeof set[0]; i++)
		if (ptrace(PTRACE_POKEUSER, pid, (void *)set[i][0], (void *)set[i][1]))
			goto restore;
	if (ptrace(PTRACE_CONT, pid, 0, 0))
		goto restore;
	int st;
	if (waitpid(pid, &st, __WALL) != pid || !WIFSTOPPED(st) || WSTOPSIG(st) != SIGTRAP) {
		logf_("inject: unexpected stop 0x%x", st);
		goto restore;
	}
	long v0, a3;
	if (peek_user(pid, 2, &v0) || peek_user(pid, 7, &a3))
		goto restore;
	if (a3 == 0 && (unsigned long)v0 == addr)
		rc = 0;
	else
		logf_("inject: mmap returned %ld (a3=%ld)", v0, a3);

restore:
	ptrace(PTRACE_POKETEXT, pid, (void *)entry, (void *)code0);
	ptrace(PTRACE_POKETEXT, pid, (void *)(entry + 4), (void *)code1);
	ptrace(PTRACE_POKEDATA, pid, (void *)(sp + 16), (void *)st16);
	ptrace(PTRACE_POKEDATA, pid, (void *)(sp + 20), (void *)st20);
	for (int i = 0; i < 32; i++)
		ptrace(PTRACE_POKEUSER, pid, (void *)(long)i, (void *)regs[i]);
	ptrace(PTRACE_POKEUSER, pid, (void *)U_HI, (void *)hi);
	ptrace(PTRACE_POKEUSER, pid, (void *)U_LO, (void *)lo);
	ptrace(PTRACE_POKEUSER, pid, (void *)U_PC, (void *)pc);
	return rc;
}

/* 1: cubevol's fb1 is now private memory. 0: not ready yet (retry).
 * -1: give up for this cubevol. */
/* Where cubevol's (now hidden) fb1 drawing lives in its memory. */
#define MAX_MAPS 4
static struct {
	pid_t pid;
	int n;
	unsigned long addr[MAX_MAPS], off[MAX_MAPS], len[MAX_MAPS];
} shadow;

static int redirect_cubevol(pid_t pid) {
	unsigned long start[4], end[4], off[4];
	int n = fb1_maps(pid, start, end, off, 4);
	if (n <= 0)
		return n < 0 ? -1 : 0;   /* not mapped yet: cubevol still starting */
	unsigned long entry = exe_entry(pid);
	if (!entry) {
		logf_("cubevol %d: not a non-PIE MIPS executable", (int)pid);
		return -1;
	}
	if (ptrace(PTRACE_ATTACH, pid, 0, 0)) {
		logf_("cubevol %d: attach failed (%s), retrying", (int)pid, strerror(errno));
		return 0;   /* nosleep may be attached right now */
	}
	int st;
	if (waitpid(pid, &st, __WALL) != pid || !WIFSTOPPED(st)) {
		ptrace(PTRACE_DETACH, pid, 0, 0);
		return -1;
	}
	int ok = 1;
	for (int i = 0; i < n && ok; i++) {
		ok = inject_anon_map(pid, entry, start[i], end[i] - start[i]) == 0;
		logf_("cubevol %d: fb1 %08lx-%08lx (offset %lx) %s", (int)pid, start[i], end[i], off[i],
		      ok ? "redirected" : "FAILED");
	}
	ptrace(PTRACE_DETACH, pid, 0, 0);
	if (ok) {
		shadow.pid = pid;
		shadow.n = n;
		for (int i = 0; i < n; i++) {
			shadow.addr[i] = start[i];
			shadow.off[i] = off[i];
			shadow.len[i] = end[i] - start[i];
		}
	}
	return ok ? 1 : -1;
}

/* Double fork: the new cubevol belongs to init, not to us. (SIGCHLD stays
 * at its default: ignoring it would change how waitpid sees the tracee.) */
static void restart_cubevol(void) {
	pid_t p = fork();
	if (p == 0) {
		if (fork() == 0) {
			setsid();
			for (int fd = 3; fd < 256; fd++)
				close(fd);
			execl("/usr/bin/cubevol", "cubevol", (char *)NULL);
			_exit(127);
		}
		_exit(0);
	}
	if (p > 0)
		waitpid(p, NULL, 0);
}

static void mark_failed(const char *why) {
	FILE *f = fopen(FAILED_FILE, "w");
	if (f) {
		fprintf(f, "%s\n", why);
		fclose(f);
	}
	logf_("disabled: %s", why);
}

/* ---- cubevol's full-screen pictures --------------------------------------- */

/* cubevol still draws its power-off (and USB) screens, into memory nobody
 * sees (each of its fb1 mappings, possibly two pages it flips between).
 * Watch that memory: when a quarter or more of an 8x6 grid of points
 * spread over one mapping turns opaque, a popup can't explain it, so it is
 * a full-screen picture (cubepoweroff.bmp may not reach the edges); copy that mapping to the same place in the real fb1 for as long
 * as it stays. Only a change to full counts, so a buffer that starts out
 * opaque never triggers it. */
static int mirror_fd = -1;
static pid_t mirror_pid;

static int shadow_open(void) {
	if (mirror_fd >= 0 && mirror_pid == shadow.pid)
		return 1;
	if (mirror_fd >= 0)
		close(mirror_fd);
	char path[64];
	snprintf(path, sizeof path, "/proc/%d/mem", (int)shadow.pid);
	mirror_fd = open(path, O_RDONLY);
	mirror_pid = shadow.pid;
	return mirror_fd >= 0;
}

#define GRID_X 8
#define GRID_Y 6
/* Opaque points of the grid in mapping i (positions within the mapping, one
 * fb1 page from its start); -1: can't tell. */
static int shadow_opaque(int i) {
	if (!shadow.pid || i >= shadow.n || !fb1_open() || !shadow_open())
		return -1;
	int n = 0;
	for (int gy = 0; gy < GRID_Y; gy++)
		for (int gx = 0; gx < GRID_X; gx++) {
			int x = fb1.w * (2 * gx + 1) / (2 * GRID_X), y = fb1.h * (2 * gy + 1) / (2 * GRID_Y);
			unsigned long o = (unsigned long)y * fb1.line + (unsigned long)x * 4;
			if (o + 4 > shadow.len[i])
				return -1;
			uint32_t px;
			if (pread(mirror_fd, &px, 4, (off_t)(shadow.addr[i] + o)) != 4)
				return -1;
			n += (px >> 24) != 0;
		}
	return n;
}
static int shadow_full(int i) {
	int n = shadow_opaque(i);
	return n < 0 ? -1 : n * 4 >= GRID_X * GRID_Y;
}

static void shadow_copy(int i) {
	if (shadow.off[i] >= fb1.len)
		return;
	size_t n = shadow.len[i];
	if (n > fb1.len - shadow.off[i])
		n = fb1.len - shadow.off[i];
	if (pread(mirror_fd, fb1.mem + shadow.off[i], n, (off_t)shadow.addr[i]) > 0)
		ioctl(fb1.fd, FBIOBLANK, FB_BLANK_UNBLANK);
}

/* Returns 1 while cubevol's picture is on screen. */
static int mirror_tick(void) {
	static int prev[MAX_MAPS] = {-1, -1, -1, -1}, active[MAX_MAPS];
	static pid_t for_pid;
	if (for_pid != shadow.pid) {   /* new cubevol: new baseline */
		for_pid = shadow.pid;
		for (int i = 0; i < MAX_MAPS; i++) { prev[i] = -1; active[i] = 0; }
	}
	int any = 0;
	for (int i = 0; i < shadow.n; i++) {
		int cnt = shadow_opaque(i);
		if (cnt < 0) { any |= active[i]; continue; }
		int full = cnt * 4 >= GRID_X * GRID_Y;
		static int last_cnt[MAX_MAPS] = {-1, -1, -1, -1};
		if (cnt != last_cnt[i] && prev[i] >= 0)
			logf_("mapping %d: %d/%d opaque", i, cnt, GRID_X * GRID_Y);
		last_cnt[i] = cnt;
		if (prev[i] < 0) {
			prev[i] = full;
			logf_("mapping %d baseline: %d/%d opaque", i, cnt, GRID_X * GRID_Y);
			continue;
		}
		if (full && (active[i] || !prev[i])) {
			if (!active[i])
				logf_("mapping %d: full-screen picture (%d/%d opaque), shown", i, cnt, GRID_X * GRID_Y);
			active[i] = 1;
			fb1.shown = 0;   /* our popup is under it */
			shadow_copy(i);
		} else if (!full && active[i]) {
			unsigned long off = shadow.off[i];
			size_t n = shadow.len[i];
			if (off < fb1.len) {
				if (n > fb1.len - off) n = fb1.len - off;
				memset(fb1.mem + off, 0, n);
			}
			active[i] = 0;
		}
		prev[i] = full;
		any |= active[i];
	}
	return any;
}

/* ---- Driver check (diagnostic) ------------------------------------------ */

/* With log.txt on the card, once per boot: which display/audio driver this
 * console has and which one runs, as checksums only (logs/driver_check.txt).
 * This tells whether a console can supply its own driver for a ShallotsOS
 * install. No file content is copied. */
typedef struct { uint32_t h[4]; uint64_t n; unsigned char b[64]; size_t bl; } Md5;
static const uint32_t md5_k[64] = {
	0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
	0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
	0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
	0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
	0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
	0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
	0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
	0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
static const unsigned char md5_r[64] = {
	7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
	4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
static void md5_block(Md5 *m, const unsigned char *p) {
	uint32_t w[16], a = m->h[0], b = m->h[1], c = m->h[2], d = m->h[3];
	for (int i = 0; i < 16; i++)
		w[i] = p[i*4] | p[i*4+1] << 8 | p[i*4+2] << 16 | (uint32_t)p[i*4+3] << 24;
	for (int i = 0; i < 64; i++) {
		uint32_t f; int g;
		if (i < 16) { f = (b & c) | (~b & d); g = i; }
		else if (i < 32) { f = (d & b) | (~d & c); g = (5*i + 1) & 15; }
		else if (i < 48) { f = b ^ c ^ d; g = (3*i + 5) & 15; }
		else { f = c ^ (b | ~d); g = (7*i) & 15; }
		uint32_t t = d; d = c; c = b;
		uint32_t x = a + f + md5_k[i] + w[g];
		b += (x << md5_r[i]) | (x >> (32 - md5_r[i]));
		a = t;
	}
	m->h[0] += a; m->h[1] += b; m->h[2] += c; m->h[3] += d;
}
static void md5_add(Md5 *m, const unsigned char *p, size_t n) {
	m->n += n;
	while (n) {
		size_t k = 64 - m->bl < n ? 64 - m->bl : n;
		memcpy(m->b + m->bl, p, k);
		m->bl += k; p += k; n -= k;
		if (m->bl == 64) { md5_block(m, m->b); m->bl = 0; }
	}
}
static void md5_hex(Md5 *m, char out[33]) {
	uint64_t bits = m->n * 8;
	unsigned char pad = 0x80, z = 0, len[8];
	md5_add(m, &pad, 1);
	while (m->bl != 56) md5_add(m, &z, 1);
	for (int i = 0; i < 8; i++) len[i] = (unsigned char)(bits >> (8*i));
	md5_add(m, len, 8);
	for (int i = 0; i < 16; i++)
		sprintf(out + i*2, "%02x", (m->h[i/4] >> (8*(i%4))) & 255);
}
/* 0 when the file can't be read. */
static long file_md5(const char *path, char out[33], unsigned char magic[4]) {
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	Md5 m = {{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476}, 0, {0}, 0};
	unsigned char buf[4096];
	size_t r;
	long total = 0;
	memset(magic, 0, 4);
	while ((r = fread(buf, 1, sizeof buf, f)) > 0) {
		if (!total) memcpy(magic, buf, r < 4 ? r : 4);
		md5_add(&m, buf, r);
		total += (long)r;
	}
	fclose(f);
	md5_hex(&m, out);
	return total;
}
static const char *known_driver(const char *md5) {
	static const char *k[][2] = {
		{"f19ff1cebbb57a9a657084e1ce27b02c", "TreeFrogUI driver_r36sx.so (= SF3500 card driver)"},
		{"b8655fbf58c4e61fc705b2ff77c0d8bd", "TreeFrogUI driver_r36sx27.so (stubbed)"},
		{"ee629cce4710d501a7feb682e2850def", "TreeFrogUI driver_sf3500.so (= SF3500 decrypted)"},
		{"83ceee0dfc442eb684147eebd33024c4", "TreeFrogUI driver_sf3000.so (= SF3000 card driver)"},
		{"19377667bf2af1c55845f85290e1a85a", "TreeFrogUI driver_gb350.so"},
		{"ee32501eb84e2bbdfbe559fa7c60af29", "R36HD card driver (encrypted)"},
	};
	for (size_t i = 0; i < sizeof k / sizeof k[0]; i++)
		if (!strcmp(md5, k[i][0])) return k[i][1];
	return "unknown";
}
static void check_one(FILE *o, const char *path) {
	char md5[33];
	unsigned char mg[4];
	long n = file_md5(path, md5, mg);
	if (!n) { fprintf(o, "%s: missing\n", path); return; }
	fprintf(o, "%s: %ld bytes, %s, md5 %s (%s)\n", path, n,
	        memcmp(mg, "\177ELF", 4) ? "encrypted/not ELF" : "ELF", md5, known_driver(md5));
}
static void driver_check(void) {
	if (access(LOG_ENABLE, F_OK) != 0 || access("/tmp/shallots_driver_check", F_OK) == 0)
		return;
	close(open("/tmp/shallots_driver_check", O_CREAT | O_WRONLY, 0644));
	mkdir(SD "/logs", 0777);
	FILE *o = fopen(SD "/logs/driver_check.txt", "w");
	if (!o) return;
	char v[400] = "";
	fprintf(o, "ShallotsOS driver check (checksums only)\n");
	if (file_value("/tmp/tfdevice.env", "TF_DEVICE", v, sizeof v)) fprintf(o, "TF_DEVICE=%s\n", v);
	if (file_value("/tmp/tfdevice.env", "TF_DRIVER", v, sizeof v)) fprintf(o, "TF_DRIVER=%s (in use)\n", v);
	fprintf(o, "driver27.flag: %s\n", access(SD "/cubegm/driver27.flag", F_OK) == 0 ? "present" : "absent");
	FILE *pv = fopen("/proc/version", "r");
	if (pv) {
		if (fgets(v, sizeof v, pv)) fprintf(o, "kernel: %s%s", v, strchr(v, '\n') ? "" : "\n");
		fclose(pv);
	}
	check_one(o, SD "/cubegm/driver.so");
	check_one(o, "/tmp/cubegm/driver.so");
	DIR *d = opendir(SD "/cubegm");
	struct dirent *e;
	while (d && (e = readdir(d))) {
		if (strncmp(e->d_name, "driver_", 7) || !strstr(e->d_name, ".so")) continue;
		char p[300];
		snprintf(p, sizeof p, SD "/cubegm/%s", e->d_name);
		check_one(o, p);
	}
	if (d) closedir(d);
	fprintf(o, "/tmp/cubegm:");
	d = opendir("/tmp/cubegm");
	while (d && (e = readdir(d)))
		if (e->d_name[0] != '.') fprintf(o, " %s", e->d_name);
	if (d) closedir(d);
	fprintf(o, "\n");
	fclose(o);
}

/* ---- Main loop ------------------------------------------------------------ */

static int already_running(void) {
	FILE *f = fopen(PID_FILE, "r");
	int pid = 0;
	if (f) {
		if (fscanf(f, "%d", &pid) != 1)
			pid = 0;
		fclose(f);
	}
	return pid > 0 && pid != getpid() && proc_comm_is(pid, "shallots_osd");
}

int main(void) {
	driver_check();
	if (already_running() || !osd_enabled())
		return 0;
	FILE *pf = fopen(PID_FILE, "w");
	if (pf) {
		fprintf(pf, "%d\n", (int)getpid());
		fclose(pf);
	}
	if (chdir("/") != 0)
		return 1;
	(void)!nice(5);   /* popups can wait a little; the game cannot */
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_term;   /* no SA_RESTART: wakes the poll sleep */
	sigaction(SIGTERM, &sa, NULL);
	load_colours();
	logf_("started");

	pid_t cv = 0;              /* the cubevol we look after */
	int cv_state = 0;          /* 0 waiting, 1 redirected, -1 gave up */
	long long cv_seen = 0, cv_check = 0, skin_check = 0, batt_check = 0, batt_warned = 0;
	int vol = read_shown_volume(), bri = read_brightness();
	time_t skin_mtime = 0;

	for (;;) {
		if (g_term) {
			/* Powering off: only if cubevol's own screen can't be seen.
			 * picoarch clears fb1 as it exits, maybe just after this, so
			 * keep the screen up until the system stops this process. */
			unlink(PID_FILE);
			if (cv_state != 1)
				return 0;
			logf_("SIGTERM: showing the power-off screen");
			for (int i = 0; i < 100; i++) {
				int shown = 0;
				for (int m = 0; m < shadow.n; m++)
					if (shadow_full(m) == 1) { shadow_copy(m); shown = 1; }
				if (!shown)
					show_poweroff();
				usleep(200000);
			}
			return 0;
		}
		long long t = now_ms();
		if (t - cv_check >= 1000) {
			cv_check = t;
			if (!osd_enabled()) {
				logf_("turned off in Settings; exiting");
				unlink(PID_FILE);
				return 0;
			}
			if (!cv || kill(cv, 0) != 0 || !proc_comm_is(cv, "cubevol")) {
				pid_t now = find_cubevol();
				if (now != cv) {
					cv = now;
					cv_state = 0;
					cv_seen = t;
				}
			}
			/* Give a new cubevol 3 s to finish starting before the swap. */
			if (cv && cv_state == 0 && t - cv_seen >= 3000 && access(FAILED_FILE, F_OK) != 0) {
				cv_state = redirect_cubevol(cv);
				if (cv_state == 1) {
					usleep(800000);
					if (kill(cv, 0) != 0 || !proc_comm_is(cv, "cubevol")) {
						mark_failed("cubevol exited after its fb1 mapping was replaced");
						restart_cubevol();
						cv = 0;
						cv_state = -1;
					}
				} else if (cv_state == -1) {
					mark_failed("could not replace cubevol's fb1 mapping");
				}
			}
		}
		if (t - skin_check >= 3000) {   /* theme changes */
			skin_check = t;
			struct stat sb;
			if (stat(SKIN_FILE, &sb) == 0 && sb.st_mtime != skin_mtime) {
				skin_mtime = sb.st_mtime;
				load_colours();
			}
		}
		/* Only draw while cubevol's own popup is out of sight. */
		if (cv_state == 1 && mirror_tick()) {
			/* cubevol's power-off/USB screen is up: keep copying it. */
		} else if (cv_state == 1) {
			int v = read_shown_volume(), b = read_brightness();
			if (v >= 0 && vol >= 0 && v != vol)
				popup_show(KIND_VOLUME, v, 1500);
			else if (b >= 0 && bri >= 0 && b != bri)
				popup_show(KIND_BRIGHTNESS, b, 1500);
			if (v >= 0)
				vol = v;
			if (b >= 0)
				bri = b;
			if (t - batt_check >= 30000) {
				batt_check = t;
				int chg, pct = read_battery(&chg);
				if (pct >= 0 && pct <= 10 && !chg && (!batt_warned || t - batt_warned >= 120000)) {
					batt_warned = t;
					popup_show(KIND_BATTERY, pct, 3000);
				}
			}
			popup_tick();
		}
		usleep(50000);
	}
}
#endif
