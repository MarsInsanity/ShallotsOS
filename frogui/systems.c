#include "systems.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define C(name) "/mnt/sdcard/cubegm/cores/" name "_libretro.so"
/* The stock firmware's own libretro cores (cubegm/cores/libemu_*.so), still
 * on cards set up from the stock backup. Never libemu_md.so: TreeFrogUI's
 * boot hijack replaces that one. */
#define STOCK(name) "/mnt/sdcard/cubegm/cores/libemu_" name ".so"
#define PCSX4ALL "/mnt/sdcard/cubegm/pcsx4all"
#define PPSSPP "/mnt/sdcard/cubegm/ppsspp"
#define PICO286 "/mnt/sdcard/cubegm/pico286"
#define DSPERATE "/mnt/sdcard/cubegm/dsperate/run_sf3000.sh"

/* Recalbox folder names and full names (package/recalbox-romfs2/systems/
 * <folder>/system.ini), for every system ShallotsOS has an emulator for. */
static const ShSystem systems[] = {
    {"nes", "Nintendo Entertainment System", "fc", {C("fceumm"), C("quicknes"), C("nestopia")}},
    {"fds", "Family Computer Disk System", "fds", {C("fceumm")}},
    {"snes", "Super Nintendo Entertainment System", "sfc",
     {C("snes9x2005_plus"), C("snes9x2002"), C("snes9x2005"), C("snes9x2010"),
      C("hcsnes_fast"), C("hcsnes_plus"), STOCK("sfc")}},
    {"gb", "Game Boy", "gb", {C("gambatte"), C("gearboy"), C("tgbdual")}},
    {"gbc", "Game Boy Color", "gbc", {C("gambatte"), C("gearboy"), C("tgbdual")}},
    {"gba", "Game Boy Advance", "gba",
     {C("gpsp"), C("gpsp_multicore"), C("vba_next"), C("mgba")}},
    {"nds", "Nintendo DS", "nds", {DSPERATE}},
    {"virtualboy", "Virtual Boy", "vb", {C("mednafen_vb")}},
    {"pokemini", "Pokémon Mini", "poke", {C("pokemini")}},
    {"gw", "Game and Watch", "gw", {C("gw")}},
    {"megadrive", "Sega Megadrive", "md", {C("picodrive"), C("genesis_plus_gx")}},
    {"mastersystem", "Sega Master System / Mark III", "ms",
     {C("picodrive"), C("genesis_plus_gx"), C("gearsystem")}},
    {"gamegear", "Sega Game Gear", "gg", {C("gearsystem"), C("genesis_plus_gx"), C("picodrive")}},
    {"sg1000", "Sega SG1000", "ms", {C("genesis_plus_gx"), C("gearsystem")}},
    {"segacd", "Sega CD", "segacd", {C("genesis_plus_gx"), C("picodrive")}},
    {"sega32x", "Sega 32X", "32X", {C("picodrive")}},
    {"psx", "Sony Playstation 1", "ps", {PCSX4ALL, C("pcsx_rearmed"), C("hcps1"), C("qpsx")}},
    {"psp", "Sony Playstation Portable", "psp", {PPSSPP, C("ppsspp")}},
    {"pcengine", "PC Engine", "pce", {C("mednafen_pce_fast"), C("mednafen_supergrafx")}},
    {"supergrafx", "Supergrafx", "sgfx", {C("mednafen_supergrafx")}},
    {"pcfx", "NEC PC-FX", "pce", {C("mednafen_pcfx")}},
    {"pc88", "NEC PC-88", "pc88", {C("quasi88")}},
    {"ngp", "Neo-Geo Pocket", "ngp", {C("race")}},
    {"ngpc", "Neo-Geo Pocket Color", "ngp", {C("race")}},
    {"neogeo", "Neo-Geo AES", "neogeo",
     {C("fbalpha2012_neogeo"), C("geolith"), C("fbneo")}},
    {"mame", "Mame", "arcade", {C("mame2000"), C("mame2003_plus")}},
    {"fbneo", "FinalBurn Neo", "arcade", {C("fbneo")}},
    {"wswan", "WonderSwan", "ws", {C("mednafen_wswan")}},
    {"wswanc", "WonderSwan Color", "ws", {C("mednafen_wswan")}},
    {"supervision", "Watara Supervision", "supervision", {C("potator")}},
    {"atari2600", "Atari 2600", "atari", {C("stella2014")}},
    {"atari5200", "Atari 5200", "5200", {C("a5200")}},
    {"atari7800", "Atari 7800", "7800", {C("prosystem")}},
    {"atari800", "Atari 8bits", "atari", {C("atari800")}},
    {"lynx", "Lynx", "lynx", {C("handy")}},
    {"atarist", "Atari ST", "atarist", {C("castaway")}},
    {"colecovision", "Colecovision", "col", {C("gearcoleco")}},
    {"intellivision", "Mattel Intellivision", "itv", {C("freeintv")}},
    {"channelf", "Fairchild Channel F", "fairchild", {C("freechaf")}},
    {"o2em", "Odyssey2", "ody", {C("o2em")}},
    {"vectrex", "Vectrex", "vectrex", {C("vecx")}},
    {"msx1", "MSX1", "msx", {C("bluemsx")}},
    {"c64", "Commodore 64", "c64", {C("vice_x64"), C("frodo")}},
    {"vic20", "Commodore VIC-20", "c64", {C("vice_xvic")}},
    {"amiga600", "Amiga ECS/OCS", "amiga", {C("uae")}},
    {"amstradcpc", "AmstradCPC", "cpc", {C("cap32")}},
    {"zxspectrum", "ZXSpectrum", "zxs", {C("fuse")}},
    {"zx81", "ZX81", "zxs", {C("81")}},
    {"thomson", "Thomson", "thomson", {C("theodore")}},
    {"x68000", "Sharp X68000", "x68000", {C("x68k")}},
    {"dos", "Dos (x86)", "dos", {PICO286}},
    {"pico8", "PICO-8", "pico", {C("fake08"), C("retro8")}},
    {"tic80", "TIC-80", "tic", {C("tic80")}},
    {"lowresnx", "LowRes NX", "lowresnx", {C("lowresnx")}},
    {"arduboy", "Arduboy", "arduboy", {C("ardens"), C("arduous")}},
    {"scummvm", "ScummVM", "scummvm", {C("scummvm")}},
    {"quake", "Quake", "ports", {C("tyrquake")}},
    {"quake2", "Quake 2", "ports", {C("vitaquake2")}},
    {"doom", "Doom", "ports", {C("prboom")}},
    {"wolfenstein3d", "Wolfenstein 3D", "ports", {C("ecwolf")}},
    {"cavestory", "Cave Story", "ports", {C("nxengine")}},
    {"flashback", "Flashback", "ports", {C("reminiscence")}},
    {"rickdangerous", "Rick Dangerous", "ports", {C("xrick")}},
    {"outrun", "Out Run", "ports", {C("cannonball")}},
    {"gong", "Gong", "ports", {C("gong")}},
};
#define SYSTEMS_N ((int)(sizeof systems / sizeof systems[0]))

/* TreeFrogUI folder -> ShallotsOS folder, and the emulator that folder ran
 * when it is not the new system's default. Folders not listed here (cps1-3,
 * j2me, ports with no Recalbox system, apps and media) stay as they are. */
static const struct {
  const char *legacy, *target, *core;
} legacy_map[] = {
    {"FC", "nes", NULL},
    {"NES", "nes", C("quicknes")},
    {"nesq", "nes", C("quicknes")},
    {"nest", "nes", C("nestopia")},
    {"SFC", "snes", NULL},
    {"snes02", "snes", C("snes9x2002")},
    {"gbgb", "gb", C("gearboy")},
    {"gbb", "gb", C("tgbdual")},
    {"dblcherrygb", "gb", NULL},
    {"GBA", "gba", NULL},
    {"gbac", "gba", C("gpsp_multicore")},
    {"gbav", "gba", C("vba_next")},
    {"mgba", "gba", C("mgba")},
    {"gbaf", "gba", C("mgba")},
    {"sega", "megadrive", NULL},
    {"MD", "megadrive", NULL},
    {"gpgx", "megadrive", C("genesis_plus_gx")},
    {"SMS", "mastersystem", NULL},
    {"gg", "gamegear", NULL},
    {"GG", "gamegear", NULL},
    {"32x", "sega32x", NULL},
    {"a26", "atari2600", NULL},
    {"a5200", "atari5200", NULL},
    {"a78", "atari7800", NULL},
    {"a800", "atari800", NULL},
    {"lnx", "lynx", NULL},
    {"atari-st", "atarist", NULL},
    {"pce", "pcengine", NULL},
    {"pcesgx", "supergrafx", NULL},
    {"pc8800", "pc88", NULL},
    {"wsv", "supervision", NULL},
    {"vb", "virtualboy", NULL},
    {"msx", "msx1", NULL},
    {"c64sc", "c64", NULL},
    {"c64f", "c64", C("frodo")},
    {"c64fc", "c64", C("frodo")},
    {"amstrad", "amstradcpc", NULL},
    {"amstradb", "amstradcpc", NULL},
    {"spec", "zxspectrum", NULL},
    {"col", "colecovision", NULL},
    {"wolf3d", "wolfenstein3d", NULL},
    {"prboom", "doom", NULL},
    {"xrick", "rickdangerous", NULL},
    {"fake08", "pico8", NULL},
    {"retro8", "pico8", C("retro8")},
    {"pico286", "dos", NULL},
    {"lowres-nx", "lowresnx", NULL},
    {"m2k", "mame", NULL},
    {"arcade", "mame", NULL},
    {"geolith", "neogeo", C("geolith")},
    {"pokem", "pokemini", NULL},
    {"int", "intellivision", NULL},
    {"fcf", "channelf", NULL},
    {"arduous", "arduboy", C("arduous")},
    {"vec", "vectrex", NULL},
    {"thom", "thomson", NULL},
    {"xmil", "x68000", NULL},
    {"amiga", "amiga600", NULL},
    {"ps1", "psx", NULL},
    {"PS", "psx", NULL},
    {"ps1r", "psx", C("pcsx_rearmed")},
    {"qpsx", "psx", C("qpsx")},
    {"PSP", "psp", NULL},
    /* Same name in both: only mixed contents move (.gbc out of gb...). */
    {"gb", "gb", NULL},
    {"ngpc", "ngpc", NULL},
    {"wswan", "wswan", NULL},
};
#define LEGACY_N ((int)(sizeof legacy_map / sizeof legacy_map[0]))

/* Files that belong to another system than their folder's (TreeFrogUI ran
 * several from one folder). Only applied to TreeFrogUI folders. */
static const struct {
  const char *ext, *target;
} ext_split[] = {
    {"gbc", "gbc"}, {"cgb", "gbc"}, {"gb", "gb"}, {"sgb", "gb"},
    {"sms", "mastersystem"}, {"gg", "gamegear"}, {"sg", "sg1000"},
    {"32x", "sega32x"}, {"ngp", "ngp"}, {"ngc", "ngpc"},
    {"ws", "wswan"}, {"wsc", "wswanc"},
};

int systems_count(void) { return SYSTEMS_N; }
const ShSystem *systems_at(int i) { return i >= 0 && i < SYSTEMS_N ? &systems[i] : NULL; }

const ShSystem *systems_find_exact(const char *folder) {
  if (!folder)
    return NULL;
  for (int i = 0; i < SYSTEMS_N; i++)
    if (!strcasecmp(systems[i].folder, folder))
      return &systems[i];
  return NULL;
}

const ShSystem *systems_find(const char *folder) {
  const ShSystem *s = systems_find_exact(folder);
  if (s || !folder)
    return s;
  for (int i = 0; i < LEGACY_N; i++)
    if (!strcasecmp(legacy_map[i].legacy, folder))
      return systems_find_exact(legacy_map[i].target);
  return NULL;
}

int systems_cores(const ShSystem *s, const char **out, int max) {
  int n = 0;
  for (int i = 0; s && i < SH_MAX_CORES && s->cores[i] && n < max; i++)
    if (access(s->cores[i], F_OK) == 0)
      out[n++] = s->cores[i];
  return n;
}

const char *systems_default_core(const ShSystem *s) {
  const char *c[1];
  return systems_cores(s, c, 1) ? c[0] : NULL;
}

const char *systems_core_label(const char *core, char *buf, size_t size) {
  static const struct { const char *file, *label; } labels[] = {
      {"fceumm", "FCEUmm"}, {"quicknes", "QuickNES"}, {"nestopia", "Nestopia"},
      {"snes9x2005_plus", "Snes9x 2005 Plus"}, {"snes9x2005", "Snes9x 2005"},
      {"snes9x2002", "Snes9x 2002"}, {"snes9x2010", "Snes9x 2010"},
      {"hcsnes_fast", "HCSNES Fast"}, {"hcsnes_plus", "HCSNES Plus"},
      {"libemu_sfc.so", "Stock Snes9x 2005"},
      {"gambatte", "Gambatte"}, {"gearboy", "Gearboy"}, {"tgbdual", "TGB Dual"},
      {"gpsp", "gpSP"}, {"gpsp_multicore", "gpSP multicore"}, {"vba_next", "VBA Next"},
      {"mgba", "mGBA"}, {"picodrive", "PicoDrive"}, {"genesis_plus_gx", "Genesis Plus GX"},
      {"gearsystem", "Gearsystem"}, {"pcsx_rearmed", "PCSX ReARMed"}, {"hcps1", "HCPS1"}, {"qpsx", "QPSX"},
      {"pcsx4all", "PCSX4ALL"}, {"ppsspp", "PPSSPP"}, {"fbalpha2012_neogeo", "FB Alpha 2012"},
      {"geolith", "Geolith"}, {"fbneo", "FinalBurn Neo"}, {"mame2000", "MAME 2000"},
      {"mame2003_plus", "MAME 2003-Plus"}, {"mednafen_pce_fast", "Beetle PCE Fast"},
      {"mednafen_supergrafx", "Beetle SuperGrafx"}, {"vice_x64", "VICE x64"},
      {"frodo", "Frodo"}, {"fake08", "FAKE-08"}, {"retro8", "Retro8"},
      {"ardens", "Ardens"}, {"arduous", "Arduous"}, {"race", "RACE"},
  };
  const char *base = strrchr(core, '/');
  base = base ? base + 1 : core;
  char stem[64];
  snprintf(stem, sizeof stem, "%s", base);
  char *suf = strstr(stem, "_libretro.so");
  if (suf)
    *suf = '\0';
  if (!strcmp(stem, "run_sf3000.sh"))
    snprintf(stem, sizeof stem, "DSperate");
  for (size_t i = 0; i < sizeof labels / sizeof labels[0]; i++)
    if (!strcmp(stem, labels[i].file)) {
      snprintf(buf, size, "%s", labels[i].label);
      return buf;
    }
  snprintf(buf, size, "%s", stem);
  return buf;
}

static int legacy_index(const char *folder) {
  for (int i = 0; folder && i < LEGACY_N; i++)
    if (!strcasecmp(legacy_map[i].legacy, folder))
      return i;
  return -1;
}

const char *systems_migrate_target(const char *legacy_folder, const char *file) {
  int i = legacy_index(legacy_folder);
  if (i < 0)
    return NULL;
  const char *dot = file ? strrchr(file, '.') : NULL;
  if (dot)
    for (size_t k = 0; k < sizeof ext_split / sizeof ext_split[0]; k++)
      if (!strcasecmp(dot + 1, ext_split[k].ext))
        return ext_split[k].target;
  return legacy_map[i].target;
}

const char *systems_legacy_core(const char *legacy_folder, const char *target) {
  int i = legacy_index(legacy_folder);
  const ShSystem *s = systems_find_exact(target);
  if (i < 0 || !legacy_map[i].core || !s)
    return NULL;
  /* Only an emulator the new system offers (tgbdual for a .gbc moved out
   * of gbb, say), and not when it is that system's default anyway. */
  for (int k = 0; k < SH_MAX_CORES && s->cores[k]; k++)
    if (!strcmp(s->cores[k], legacy_map[i].core))
      return k == 0 ? NULL : legacy_map[i].core;
  return NULL;
}

int systems_legacy_count(void) { return LEGACY_N; }
const char *systems_legacy_at(int i) { return i >= 0 && i < LEGACY_N ? legacy_map[i].legacy : NULL; }

/* FrogUI's own icon set (frogui/system-icons, icon packs) is named after
 * TreeFrogUI's folders. */
const char *systems_icon_key(const char *folder) {
  static const struct { const char *folder, *key; } keys[] = {
      {"virtualboy", "vb"}, {"pokemini", "pokem"}, {"megadrive", "sega"},
      {"mastersystem", "gg"}, {"gamegear", "gg"}, {"sg1000", "gg"}, {"sega32x", "32x"},
      {"psx", "ps1"}, {"pcengine", "pce"}, {"supergrafx", "pcesgx"}, {"pc88", "pc8800"},
      {"ngp", "ngpc"}, {"mame", "m2k"}, {"fbneo", "cps2"}, {"wswanc", "wswan"},
      {"supervision", "wsv"}, {"atari2600", "a26"}, {"atari5200", "a5200"},
      {"atari7800", "a78"}, {"atari800", "a800"}, {"lynx", "lnx"},
      {"colecovision", "col"}, {"intellivision", "int"}, {"channelf", "fcf"},
      {"vectrex", "vec"}, {"msx1", "msx"}, {"amiga600", "amiga"},
      {"amstradcpc", "amstrad"}, {"zxspectrum", "spec"}, {"thomson", "thom"},
      {"x68000", "xmil"}, {"dos", "pico286"}, {"lowresnx", "lowres-nx"},
      {"doom", "prboom"}, {"wolfenstein3d", "wolf3d"}, {"rickdangerous", "xrick"},
  };
  for (size_t i = 0; folder && i < sizeof keys / sizeof keys[0]; i++)
    if (!strcasecmp(folder, keys[i].folder))
      return keys[i].key;
  return folder;
}
