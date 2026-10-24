/*
 * core_override.c - per-game / per-folder core selection overrides.
 * See core_override.h.
 */
#include <stdio.h>
#include <string.h>
#include "core_override.h"

#define OVERRIDE_FILE "/mnt/sdcard/frogui/core_overrides.txt"
#define MAX_OVERRIDES 512
#define KEY_LEN       512
#define CORE_LEN      256

typedef struct {
    char key[KEY_LEN];
    char core[CORE_LEN];
} Override;

static Override overrides[MAX_OVERRIDES];
static int override_count = 0;

static void save_file(void) {
    FILE *f = fopen(OVERRIDE_FILE, "w");
    if (!f) return;
    for (int i = 0; i < override_count; i++)
        fprintf(f, "%s|%s\n", overrides[i].key, overrides[i].core);
    fclose(f);
}

void core_override_load(void) {
    override_count = 0;
    FILE *f = fopen(OVERRIDE_FILE, "r");
    if (!f) return;
    char line[KEY_LEN + CORE_LEN + 2];
    while (override_count < MAX_OVERRIDES && fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *bar = strchr(line, '|');
        if (!bar) continue;
        *bar = '\0';
        const char *core = bar + 1;
        if (line[0] == '\0' || core[0] == '\0') continue;
        strncpy(overrides[override_count].key,  line, KEY_LEN - 1);
        overrides[override_count].key[KEY_LEN - 1] = '\0';
        strncpy(overrides[override_count].core, core, CORE_LEN - 1);
        overrides[override_count].core[CORE_LEN - 1] = '\0';
        override_count++;
    }
    fclose(f);
}

static const char *find(const char *key) {
    if (!key) return NULL;
    for (int i = 0; i < override_count; i++)
        if (strcmp(overrides[i].key, key) == 0)
            return overrides[i].core;
    return NULL;
}

const char *core_override_lookup(const char *rom_path, const char *folder_path) {
    const char *c = find(rom_path);      /* per-game first */
    if (c) return c;
    return find(folder_path);            /* then per-folder */
}

void core_override_set(const char *key, const char *core_path) {
    if (!key || key[0] == '\0') return;
    int idx = -1;
    for (int i = 0; i < override_count; i++)
        if (strcmp(overrides[i].key, key) == 0) { idx = i; break; }

    if (!core_path || core_path[0] == '\0') {
        /* clear: remove entry if present */
        if (idx >= 0) {
            for (int i = idx; i < override_count - 1; i++)
                overrides[i] = overrides[i + 1];
            override_count--;
            save_file();
        }
        return;
    }

    if (idx < 0) {
        if (override_count >= MAX_OVERRIDES) return;
        idx = override_count++;
        strncpy(overrides[idx].key, key, KEY_LEN - 1);
        overrides[idx].key[KEY_LEN - 1] = '\0';
    }
    strncpy(overrides[idx].core, core_path, CORE_LEN - 1);
    overrides[idx].core[CORE_LEN - 1] = '\0';
    save_file();
}
