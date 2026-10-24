/*
 * input.h - Input device interface
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>

#define KEYMAP_FILE "/mnt/sdcard/frogui/keymap.txt"
#define FROG_RAW_MAX_BIT 16
#define FROG_RAW_BIT_COUNT 17
#define FROG_RAW_BUTTON_MASK 0x0001FFFFu

typedef enum {
    FROG_BTN_UP = 0,
    FROG_BTN_DOWN,
    FROG_BTN_LEFT,
    FROG_BTN_RIGHT,
    FROG_BTN_A,
    FROG_BTN_B,
    FROG_BTN_X,
    FROG_BTN_Y,
    FROG_BTN_L1,
    FROG_BTN_R1,
    FROG_BTN_L2,
    FROG_BTN_R2,
    FROG_BTN_START,
    FROG_BTN_SELECT,
    FROG_BTN_FN,
    FROG_BTN_COUNT
} FrogButton;

/* Human-readable name for each button (used in remap wizard). */
const char *input_btn_name(FrogButton btn);

/* Initialize input system (cubevol shared memory). */
int  input_init(void);
void input_deinit(void);

/* Call once per frame before reading button state. */
void input_update(void);

/* Current state (updated by input_update). */
bool input_is_pressed(FrogButton btn);
bool input_was_pressed(FrogButton btn);   /* true only on rising edge this frame */
bool input_repeat(FrogButton btn);        /* edge, then auto-repeats while held (time-based) */

/* Raw cubevol value (bits 0..16 inclusive, mask 0x1FFFF) needed by remap wizard. */
uint32_t input_get_raw_state(void);
void input_set_ext_raw(uint32_t raw);

/* Remap table: logical button -> raw bit index (0..16), or -1 = unmapped. */
void input_reset_defaults(void);
void input_set_raw_bit(FrogButton btn, int raw_bit);
int  input_get_raw_bit(FrogButton btn);

/* True when this device has the physical FN button (R36SX family, detected
 * via TF_DEVICE; see input.c). Gates the FN default mapping and the FN step
 * of the mapping wizard. */
bool input_fn_available(void);

/* Persist/restore keymap file.  Returns 0 on success, -1 on error/not-found. */
int  input_load_remap(const char *path);
int  input_save_remap(const char *path);

#endif /* INPUT_H */
