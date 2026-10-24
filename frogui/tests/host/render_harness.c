/*
 * Host render harness: runs the FrogUI libretro core on a desktop Linux
 * machine, replays a button script and saves frames as binary PPM files.
 *
 *   render_harness OUT_DIR STEP...
 *
 * Steps: wait:N (run N frames), A/B/X/Y/L/R/UP/DOWN/LEFT/RIGHT/START/SELECT
 * (press and release), shot:NAME (save OUT_DIR/NAME.ppm).
 * The core reads /mnt/sdcard, so point that at a scratch card first.
 */
#include "../../libretro.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* The battery ADC lives in /dev on the device. Linked with --wrap=open, this
 * serves HARNESS_BATTERY_RAW (raw ADC byte, e.g. 170) from a temporary file
 * so screenshots include the battery widget. */
int __real_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...) {
  const char *raw = getenv("HARNESS_BATTERY_RAW");
  if (raw && (!strcmp(path, "/dev/check_adc1") || !strcmp(path, "/dev/check_adc5"))) {
    char tmpl[] = "/tmp/harness-adc-XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd >= 0) {
      unsigned char b = (unsigned char)(strcmp(path, "/dev/check_adc1") ? 0 : atoi(raw));
      if (write(fd, &b, 1) != 1) b = 0;
      unlink(tmpl);
    }
    return fd;
  }
  va_list ap;
  va_start(ap, flags);
  int mode = (flags & O_CREAT) ? va_arg(ap, int) : 0;
  va_end(ap);
  return __real_open(path, flags, mode);
}

static uint16_t *frame;
static unsigned frame_w, frame_h;
static unsigned buttons;

static void video_cb(const void *data, unsigned w, unsigned h, size_t pitch) {
  if (!data)
    return;
  if (!frame || w != frame_w || h != frame_h) {
    free(frame);
    frame = malloc((size_t)w * h * sizeof *frame);
    frame_w = w;
    frame_h = h;
  }
  for (unsigned y = 0; y < h; y++)
    memcpy(frame + (size_t)y * w, (const char *)data + y * pitch,
           w * sizeof *frame);
}

static bool environ_cb(unsigned cmd, void *data) {
  (void)cmd;
  (void)data;
  return false;
}
static void input_poll_cb(void) {}
static int16_t input_state_cb(unsigned port, unsigned device, unsigned index,
                              unsigned id) {
  (void)index;
  return port == 0 && device == RETRO_DEVICE_JOYPAD && ((buttons >> id) & 1);
}
static void audio_cb(int16_t l, int16_t r) {
  (void)l;
  (void)r;
}
static size_t audio_batch_cb(const int16_t *d, size_t frames) {
  (void)d;
  return frames;
}

static void run(int frames) {
  for (int i = 0; i < frames; i++)
    retro_run();
}

/* Run frames for at least `ms` of wall time: face buttons must be held for
 * ~65 ms before FrogUI accepts them (stick-drift filter). */
static void run_ms(int ms) {
  struct timespec start, now;
  clock_gettime(CLOCK_MONOTONIC, &start);
  do {
    retro_run();
    clock_gettime(CLOCK_MONOTONIC, &now);
  } while ((now.tv_sec - start.tv_sec) * 1000 +
               (now.tv_nsec - start.tv_nsec) / 1000000 < ms);
}

static int button_id(const char *name) {
  static const struct {
    const char *name;
    int id;
  } map[] = {{"A", RETRO_DEVICE_ID_JOYPAD_A},
             {"B", RETRO_DEVICE_ID_JOYPAD_B},
             {"X", RETRO_DEVICE_ID_JOYPAD_X},
             {"Y", RETRO_DEVICE_ID_JOYPAD_Y},
             {"L", RETRO_DEVICE_ID_JOYPAD_L},
             {"R", RETRO_DEVICE_ID_JOYPAD_R},
             {"UP", RETRO_DEVICE_ID_JOYPAD_UP},
             {"DOWN", RETRO_DEVICE_ID_JOYPAD_DOWN},
             {"LEFT", RETRO_DEVICE_ID_JOYPAD_LEFT},
             {"RIGHT", RETRO_DEVICE_ID_JOYPAD_RIGHT},
             {"START", RETRO_DEVICE_ID_JOYPAD_START},
             {"SELECT", RETRO_DEVICE_ID_JOYPAD_SELECT}};
  for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
    if (!strcmp(map[i].name, name))
      return map[i].id;
  return -1;
}

static int save_ppm(const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f || !frame)
    return 0;
  fprintf(f, "P6\n%u %u\n255\n", frame_w, frame_h);
  for (size_t i = 0; i < (size_t)frame_w * frame_h; i++) {
    uint16_t c = frame[i];
    unsigned char rgb[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31),
                            (unsigned char)(((c >> 5) & 63) * 255 / 63),
                            (unsigned char)((c & 31) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  return 1;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s OUT_DIR STEP...\n", argv[0]);
    return 2;
  }
  retro_set_environment(environ_cb);
  retro_set_video_refresh(video_cb);
  retro_set_input_poll(input_poll_cb);
  retro_set_input_state(input_state_cb);
  retro_set_audio_sample(audio_cb);
  retro_set_audio_sample_batch(audio_batch_cb);
  retro_init();
  run(30);
  for (int i = 2; i < argc; i++) {
    const char *step = argv[i];
    if (!strncmp(step, "wait:", 5)) {
      run(atoi(step + 5));
    } else if (!strncmp(step, "shot:", 5)) {
      char path[1024];
      snprintf(path, sizeof path, "%s/%s.ppm", argv[1], step + 5);
      run(8); /* let crossfades settle */
      if (!save_ppm(path)) {
        fprintf(stderr, "could not save %s\n", path);
        return 1;
      }
    } else {
      int id = button_id(step);
      if (id < 0) {
        fprintf(stderr, "unknown step %s\n", step);
        return 2;
      }
      buttons |= 1u << id;
      run_ms(100);
      buttons &= ~(1u << id);
      run_ms(30);
    }
  }
  retro_deinit();
  free(frame);
  return 0;
}
