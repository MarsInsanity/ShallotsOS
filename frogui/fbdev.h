/*
 * fbdev.h - Framebuffer driver interface
 */

#ifndef FBDEV_H
#define FBDEV_H

#include <stdint.h>
#include <stddef.h>

// Initialize framebuffer (opens /dev/fb0 and mmaps it)
int fbdev_init(void);

// Clean up framebuffer resources
void fbdev_deinit(void);

// Blit 480x320 RGB565 image to centered position on screen (converts to ARGB8888)
void fbdev_blit(uint16_t *src, int src_w, int src_h);

// Clear entire screen to black
void fbdev_clear(void);

// Get FrogUI dimensions
int fbdev_get_width(void);
int fbdev_get_height(void);

#endif // FBDEV_H
