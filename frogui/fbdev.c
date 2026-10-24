/*
 * fbdev.c - Display for SF3000
 * Replicates picoarch sf3000_fb_init() exactly:
 *   1. SDL_VIDEODRIVER=dummy + SDL_Init(VIDEO)
 *   2. /dev/dis ioctl {1,0,0}
 *   3. mmap fb0 using smem_len
 *   4. Write with 90-deg rotation (picoarch cvt565 + fb_x/fb_y mapping)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <dlfcn.h>
#include "fbdev.h"

#ifndef FROGUI_WIDTH
#define FROGUI_WIDTH  480
#define FROGUI_HEIGHT 320
#endif

#define FB_X_VIS   180
#define FB_Y_TOT  1014
#define PHYS_W     640
#define PHYS_H     480

static int       fb_fd  = -1;
static uint32_t *fb_mem = NULL;
static size_t    fb_size = 0;
static int       dst_stride = 0;

static inline uint32_t cvt565(uint16_t c) {
    return 0xFF000000u
        | (((c & 0xF800u) << 8) | ((c & 0xE000u) << 3))
        | (((c & 0x07E0u) << 5) | ((c & 0x0600u) >> 1))
        | (((c & 0x001Fu) << 3) | ((c & 0x001Cu) >> 2));
}

static void xlog(const char *msg) {
    FILE *f = fopen("/mnt/sdcard/frogui_debug.log", "a");
    if (f) { fputs(msg, f); fputc('\n', f); fflush(f); fsync(fileno(f)); fclose(f); }
    fprintf(stderr, "FROGUI: %s\n", msg);
}

int fbdev_init(void) {
    char msg[128];
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    /* --- Replicate picoarch plat_init exactly --- */

    /* 1. SDL_VIDEODRIVER=dummy + SDL_Init(VIDEO) */
    xlog("fbdev_init: SDL dummy init (picoarch style)...");
    setenv("SDL_NOMOUSE",    "1",     1);
    setenv("SDL_VIDEODRIVER","dummy", 1);   /* dummy = no HW touch, same as picoarch */
    void *sdl = dlopen("libSDL-1.2.so.0", RTLD_LAZY | RTLD_GLOBAL);
    if (sdl) {
        int (*sdl_init)(unsigned int) = dlsym(sdl, "SDL_Init");
        if (sdl_init) {
            int r = sdl_init(0x00000020u); /* SDL_INIT_VIDEO */
            snprintf(msg, sizeof(msg), "fbdev_init: SDL_Init ret=%d", r);
            xlog(msg);
        }
        /* keep handle open */
    } else {
        xlog("fbdev_init: SDL not found");
    }

    /* 2. Wire display controller to fb0 — AFTER SDL_Init, same as picoarch */
    {
        int dis = open("/dev/dis", O_RDWR);
        if (dis >= 0) {
            struct { int a, b, c; } buf = {1, 0, 0};
            int r = ioctl(dis, 0xc00c0e0c, &buf);
            snprintf(msg, sizeof(msg), "fbdev_init: /dev/dis ioctl ret=%d", r);
            xlog(msg);
            close(dis);
        } else {
            xlog("fbdev_init: /dev/dis open failed");
        }
    }

    /* 3. Open + mmap fb0 using smem_len (picoarch uses smem_len not yres_virtual*pitch) */
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) { xlog("fbdev_init: ERROR open fb0"); return -1; }

    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

    dst_stride = (int)(finfo.line_length / 4);
    snprintf(msg, sizeof(msg), "fbdev_init: fb %dx%d bpp=%d stride=%d smem=%u",
             vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, dst_stride, finfo.smem_len);
    xlog(msg);

    fb_size = finfo.smem_len ? (size_t)finfo.smem_len
                             : (size_t)vinfo.yres_virtual * finfo.line_length;
    fb_mem = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_mem == MAP_FAILED) { xlog("fbdev_init: mmap failed"); close(fb_fd); fb_fd=-1; return -1; }

    /* 4. Clear to opaque black (picoarch does same) */
    for (size_t i = 0; i < fb_size / 4; i++)
        fb_mem[i] = 0xFF000000u;

    xlog("fbdev_init: OK");
    return 0;
}

void fbdev_deinit(void) {
    if (fb_mem) { munmap(fb_mem, fb_size); fb_mem = NULL; }
    if (fb_fd >= 0) { close(fb_fd); fb_fd = -1; }
}

void fbdev_blit(uint16_t *src, int src_w, int src_h) {
    if (!fb_mem) return;

    /* Re-assert per-frame (picoarch does this every blit) */
    {
        int dis = open("/dev/dis", O_RDWR);
        if (dis >= 0) {
            struct { int a, b, c; } buf = {1, 0, 0};
            ioctl(dis, 0xc00c0e0c, &buf);
            close(dis);
        }
    }

    int fb_y_len = src_w * FB_Y_TOT / PHYS_W;
    int fb_y_off = (FB_Y_TOT - fb_y_len) / 2;
    int fb_x_len = src_h * FB_X_VIS / PHYS_H;
    int fb_x_off = (FB_X_VIS - fb_x_len) / 2;

    for (int fb_y = fb_y_off; fb_y < fb_y_off + fb_y_len; fb_y++) {
        int sx = (fb_y - fb_y_off) * src_w / fb_y_len;
        if (sx >= src_w) sx = src_w - 1;
        uint32_t *dst = fb_mem + fb_y * dst_stride;
        for (int fb_x = fb_x_off; fb_x < fb_x_off + fb_x_len; fb_x++) {
            int sy = src_h - 1 - (fb_x - fb_x_off) * src_h / fb_x_len;
            if (sy < 0) sy = 0; if (sy >= src_h) sy = src_h - 1;
            dst[fb_x] = cvt565(src[sy * src_w + sx]);
        }
    }
}

void fbdev_fill_all_pages_red(void) {
    if (!fb_mem) return;
    size_t n = fb_size / 4;
    for (size_t i = 0; i < n; i++)
        fb_mem[i] = 0xFFFF0000u;  /* opaque red ARGB */
}

void fbdev_clear(void) {
    if (fb_mem)
        for (size_t i = 0; i < fb_size / 4; i++)
            fb_mem[i] = 0xFF000000u;
}

int fbdev_get_width(void)  { return FROGUI_WIDTH; }
int fbdev_get_height(void) { return FROGUI_HEIGHT; }
