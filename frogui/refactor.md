# FrogUI refactor plan (DRY / KISS / YAGNI)

Scope: the **shipped** frontend only — the libretro core built by
`build_libretro.sh` → `frogui_libretro.so`. That build compiles:

```
frogui_libretro.c render.c font.c recent_games.c settings.c
theme.c favorites.c banner.c backlight.c input.c core_override.c
```

Everything else in the repo (`frogos.c`, `frogos_sf3000.c`, the Makefiles,
the `*_makefile.py` scripts) belongs to the retired standalone build and is
**not** part of the shipped core.

Guardrail for every change: it must still build with `./build_libretro.sh`
(MIPS toolchain, `-Wall -Werror`-clean is nice-to-have but at least no new
warnings), and behaviour on device must not change. No feature work — pure
deletion and de-duplication.

---

## Tier 1 — YAGNI: delete dead code (biggest win, lowest risk)

### 1.1 `settings.c` is compiled but has ZERO callers  ★ do first
`settings.c` (699 lines) exposes a whole settings API (`settings_init`,
`settings_load`, `settings_save`, `settings_handle_input`, `settings_show_menu`,
`settings_cycle_option`, …). **None** of them are called by any compiled source.
`frogui_libretro.c` has its **own** static settings implementation
(`settings_load_file`, `settings_save_file`, `settings_apply`, the
`settings_menu`) and never touches `settings.c`.

- Action: remove `settings.c` from the `for src in …` list **and** the link
  line in `build_libretro.sh`. Delete `settings.c` and `settings.h` **iff**
  nothing else includes `settings.h` (grep first — `render`/`theme` may include
  it for a struct; if so, keep the header, drop only the `.c` from the build).
- Verify: `./build_libretro.sh` links clean (it uses `-Wl,--no-undefined`, so a
  missing symbol would fail the link — a good safety net).

### 1.2 Legacy standalone build — delete
Not referenced by `build_libretro.sh`; only by the retired `Makefile` /
`Makefile.sf3000` (the black-screen trap called out in `CLAUDE.md`).

- `frogos.c` (2024 lines), `frogos_sf3000.c` (441 lines)
- `Makefile`, `Makefile.sf3000`
- `fix_makefile.py`, `patch_makefile.py`, `safe_patch.py`, `fix_makefile`
- Action: `git rm` them. Grep first for any script/CI referencing them.

### 1.3 Duplicate `console_mappings[]` rows
In `frogui_libretro.c`:
- `{"GBA", … gpsp_libretro.so}` appears 3× (lines ~71, 77, and another).
- `{"gbaf", … mgba_libretro.so}` appears 2× (lines 75–76, literally back to back).
- Action: delete the duplicate rows. First match wins in the lookup, so the
  dupes are pure dead weight.

**Tier 1 removes ~3200 lines + 6 dead build files with near-zero risk.**

---

## Tier 2 — DRY: collapse the 4× parallel settings structures

Each user setting is currently spelled out in **four** places that must be kept
in sync by hand (brightness and volume are touched 11× each across the file):

1. `settings_load_file()` — one `else if (strcmp(line,"key")==0)` per field.
2. `settings_save_file()` — one `fprintf(f,"key=%s\n",…)` per field.
3. `settings_apply()` — per-field clamping.
4. `handle_input()` settings-menu branch — one `else if (idx==ROW_X)` per field,
   each doing `(val + delta + N) % N` or a clamp.

Replace with a single field-descriptor table, e.g.:

```c
typedef enum { ST_ONOFF, ST_RANGE, ST_ENUM } SettingType;
typedef struct {
    const char  *key;        /* file key + used to match on load */
    SettingType  type;
    int         *value;      /* pointer to the settings_* global */
    int          min, max, step;   /* ST_RANGE */
    const char *const *names;      /* ST_ENUM / ST_ONOFF label table */
    int          name_count;
} SettingDef;
static const SettingDef settings_defs[] = { … };
```

Then load/save/clamp/cycle each become one loop over `settings_defs`. On/off
fields collapse to `ST_ONOFF` with `onoff_names`; theme/font/filter become
`ST_ENUM` with their existing name tables. This is the highest-value DRY change
but also the one most able to change behaviour — do it in isolation, diff the
generated `settings.txt` before/after against a known-good card to confirm the
serialization is byte-identical.

**Risk: medium. One focused agent, one file, verify serialization round-trips.**

---

## Tier 3 — KISS + smaller DRY (optional, do after Tier 1–2 land)

### 3.1 Split `handle_input()` (360 lines)
It is five independent overlay state machines inlined back-to-back (search
keyboard, core picker, remap wizard, settings menu, main browser). Extract one
static handler per overlay (`handle_search_kbd()`, `handle_core_picker()`, …)
and have `handle_input()` dispatch to the active one. Pure code-move, no logic
change.

### 3.2 `menu_nav()` cursor helper
The `up/down` + wrap idiom (`(idx - 1 + count) % count` / `(idx + 1) % count`)
and the `left/right` page idiom repeat across all 5 overlays (30 `input_was_pressed`
sites). Extract:
```c
static int menu_nav(int idx, int count);          /* up/down wrap */
static int menu_page(int idx, int count, int page); /* left/right clamp */
```

### 3.3 `write_text_file()` helper
The `fopen / fprintf / fflush / fsync(fileno) / fclose` idiom repeats (volume
write in `settings_apply`, the launch file, `settings_save_file`). One helper:
`static void write_text_file(const char *path, const char *fmt, ...);`

### 3.4 (low priority) `console_mappings` CORES_PATH prefix
120 rows each hardcode `CORES_PATH "/…_libretro.so"`. Could store just the bare
`.so` name and prepend `CORES_PATH` at lookup. Saves repetition but hurts
grep-ability and touches a big table — **skip unless bored**; not worth the
diff/risk.

---

## Suggested order for cheap-model agents

1. **Agent A (surgical, low risk):** Tier 1.1 + 1.2 + 1.3 — delete dead files,
   drop `settings.c` from `build_libretro.sh`, remove dup mapping rows. Must end
   with a clean `./build_libretro.sh`.
2. **Agent B (one file, medium risk):** Tier 2 — settings descriptor table.
   Verify `settings.txt` round-trips identically.
3. **Agent C (mechanical):** Tier 3.1 + 3.2 — split `handle_input`, add nav
   helpers. Pure code-move; build must stay clean.

Each agent: build with `./build_libretro.sh` before returning; report the diff
and the build result. Do NOT touch on-device behaviour.
