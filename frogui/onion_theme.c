#include "onion_theme.h"
#include "systems.h"

#include "common/i18n.h"
#include "font.h"
#include "stb_image.h" /* decls only; impl lives in banner.c */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define ONION_W 640
#define ONION_PATH_MAX 512

/* ------------------------------------------------------------------------ */
/* Small helpers                                                             */
/* ------------------------------------------------------------------------ */

static int file_exists(const char *path) {
  struct stat st;
  return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int dir_exists(const char *path) {
  struct stat st;
  return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static const char *path_basename(const char *path) {
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

static int has_suffix_ci(const char *s, const char *suffix) {
  size_t n = strlen(s), m = strlen(suffix);
  return n >= m && strcasecmp(s + n - m, suffix) == 0;
}

static int contains_ci(const char *hay, const char *needle) {
  size_t n = strlen(needle);
  for (; *hay; hay++)
    if (!strncasecmp(hay, needle, n))
      return 1;
  return n == 0;
}

static void mkdir_parents(const char *path) {
  char tmp[ONION_PATH_MAX];
  snprintf(tmp, sizeof tmp, "%s", path);
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
}

static char *read_file(const char *path, long *size_out, long max_size) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (size < 0 || size > max_size) {
    fclose(f);
    return NULL;
  }
  char *data = malloc((size_t)size + 1);
  if (!data || fread(data, 1, (size_t)size, f) != (size_t)size) {
    free(data);
    fclose(f);
    return NULL;
  }
  fclose(f);
  data[size] = '\0';
  if (size_out)
    *size_out = size;
  return data;
}

static inline uint16_t rgb565(unsigned r, unsigned g, unsigned b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* RGB565 blend without divides (they are slow on the single-core MIPS
 * devices). Full 8-bit alpha keeps theme gradients free of banding. */
static inline uint16_t blend565(uint16_t dst, uint16_t src, unsigned a) {
  if (a >= 255)
    return src;
  if (a == 0)
    return dst;
  int k = (int)(a + (a >> 7)); /* 0..255 -> 0..256 */
  int dr = (dst >> 11) & 31, dg = (dst >> 5) & 63, db = dst & 31;
  int r = dr + ((((src >> 11) & 31) - dr) * k >> 8);
  int g = dg + ((((src >> 5) & 63) - dg) * k >> 8);
  int b = db + (((src & 31) - db) * k >> 8);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

/* Same blend done at 8 bits per channel with an ordered-dither offset `t`
 * (0..15): fading gradients would otherwise band, because each RGB565
 * channel steps at a different alpha. */
static inline uint16_t blend565_dither(uint16_t dst, uint16_t src, unsigned a,
                                       unsigned t) {
  int k = (int)(a + (a >> 7));
  int dr = ((dst >> 11) & 31) << 3, dg = ((dst >> 5) & 63) << 2, db = (dst & 31) << 3;
  int sr = ((src >> 11) & 31) << 3, sg = ((src >> 5) & 63) << 2, sb = (src & 31) << 3;
  int r = dr + ((sr - dr) * k >> 8) + (int)(t >> 1);
  int g = dg + ((sg - dg) * k >> 8) + (int)(t >> 2);
  int b = db + ((sb - db) * k >> 8) + (int)(t >> 1);
  if (r > 255) r = 255;
  if (g > 255) g = 255;
  if (b > 255) b = 255;
  return rgb565((unsigned)r, (unsigned)g, (unsigned)b);
}

static const uint8_t bayer4[4][4] = {
    {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static unsigned luma565(uint16_t c) {
  unsigned r = ((c >> 11) & 31) * 255 / 31;
  unsigned g = ((c >> 5) & 63) * 255 / 63;
  unsigned b = (c & 31) * 255 / 31;
  return (299 * r + 587 * g + 114 * b) / 1000;
}

/* ------------------------------------------------------------------------ */
/* Minimal JSON reader (objects, arrays, strings, numbers, literals).        */
/* Tolerates comments and trailing commas, which hand-edited configs have.   */
/* ------------------------------------------------------------------------ */

enum { J_NULL, J_BOOL, J_NUM, J_STR, J_OBJ, J_ARR };

typedef struct JNode {
  int type;
  double num;
  char *str;
  char *key;
  struct JNode *child;
  struct JNode *next;
} JNode;

typedef struct {
  const char *p;
  int depth;
} JParser;

static void j_free(JNode *n) {
  while (n) {
    JNode *next = n->next;
    j_free(n->child);
    free(n->str);
    free(n->key);
    free(n);
    n = next;
  }
}

static void j_skip(JParser *jp) {
  for (;;) {
    while (*jp->p && isspace((unsigned char)*jp->p))
      jp->p++;
    if (jp->p[0] == '/' && jp->p[1] == '/') {
      while (*jp->p && *jp->p != '\n')
        jp->p++;
    } else if (jp->p[0] == '/' && jp->p[1] == '*') {
      jp->p += 2;
      while (*jp->p && !(jp->p[0] == '*' && jp->p[1] == '/'))
        jp->p++;
      if (*jp->p)
        jp->p += 2;
    } else {
      return;
    }
  }
}

static void utf8_put(char **out, unsigned cp) {
  char *o = *out;
  if (cp < 0x80) {
    *o++ = (char)cp;
  } else if (cp < 0x800) {
    *o++ = (char)(0xC0 | (cp >> 6));
    *o++ = (char)(0x80 | (cp & 0x3F));
  } else {
    *o++ = (char)(0xE0 | (cp >> 12));
    *o++ = (char)(0x80 | ((cp >> 6) & 0x3F));
    *o++ = (char)(0x80 | (cp & 0x3F));
  }
  *out = o;
}

static char *j_string(JParser *jp) {
  if (*jp->p != '"')
    return NULL;
  jp->p++;
  const char *start = jp->p;
  size_t cap = 1;
  while (*jp->p && *jp->p != '"') {
    if (*jp->p == '\\' && jp->p[1])
      jp->p++;
    jp->p++;
    cap += 3;
  }
  if (*jp->p != '"')
    return NULL;
  char *out = malloc(cap + 1), *o = out;
  if (!out)
    return NULL;
  for (const char *s = start; s < jp->p; s++) {
    if (*s != '\\') {
      *o++ = *s;
      continue;
    }
    s++;
    switch (*s) {
    case 'n': *o++ = '\n'; break;
    case 't': *o++ = '\t'; break;
    case 'r': *o++ = '\r'; break;
    case 'b': *o++ = '\b'; break;
    case 'f': *o++ = '\f'; break;
    case 'u': {
      unsigned cp = 0;
      int i;
      for (i = 1; i <= 4 && isxdigit((unsigned char)s[i]); i++)
        cp = cp * 16 + (unsigned)(isdigit((unsigned char)s[i])
                                       ? s[i] - '0'
                                       : (tolower((unsigned char)s[i]) - 'a' + 10));
      s += i - 1;
      utf8_put(&o, cp);
      break;
    }
    default: *o++ = *s; break;
    }
  }
  *o = '\0';
  jp->p++;
  return out;
}

static JNode *j_value(JParser *jp);

static JNode *j_container(JParser *jp, int type) {
  JNode *node = calloc(1, sizeof *node), **tail;
  if (!node)
    return NULL;
  node->type = type;
  tail = &node->child;
  char close = type == J_OBJ ? '}' : ']';
  jp->p++;
  if (++jp->depth > 32) {
    j_free(node);
    return NULL;
  }
  for (;;) {
    j_skip(jp);
    if (*jp->p == close) {
      jp->p++;
      break;
    }
    char *key = NULL;
    if (type == J_OBJ) {
      key = j_string(jp);
      j_skip(jp);
      if (!key || *jp->p != ':') {
        free(key);
        j_free(node);
        return NULL;
      }
      jp->p++;
    }
    JNode *child = j_value(jp);
    if (!child) {
      free(key);
      j_free(node);
      return NULL;
    }
    child->key = key;
    *tail = child;
    tail = &child->next;
    j_skip(jp);
    if (*jp->p == ',') {
      jp->p++;
      continue;
    }
    if (*jp->p == close) {
      jp->p++;
      break;
    }
    j_free(node);
    return NULL;
  }
  jp->depth--;
  return node;
}

static JNode *j_value(JParser *jp) {
  j_skip(jp);
  if (*jp->p == '{')
    return j_container(jp, J_OBJ);
  if (*jp->p == '[')
    return j_container(jp, J_ARR);
  JNode *node = calloc(1, sizeof *node);
  if (!node)
    return NULL;
  if (*jp->p == '"') {
    node->type = J_STR;
    node->str = j_string(jp);
    if (!node->str) {
      free(node);
      return NULL;
    }
  } else if (!strncmp(jp->p, "true", 4) || !strncmp(jp->p, "false", 5)) {
    node->type = J_BOOL;
    node->num = jp->p[0] == 't';
    jp->p += jp->p[0] == 't' ? 4 : 5;
  } else if (!strncmp(jp->p, "null", 4)) {
    node->type = J_NULL;
    jp->p += 4;
  } else {
    char *end;
    node->type = J_NUM;
    node->num = strtod(jp->p, &end);
    if (end == jp->p) {
      free(node);
      return NULL;
    }
    jp->p = end;
  }
  return node;
}

static JNode *j_parse_file(const char *path) {
  char *text = read_file(path, NULL, 256 * 1024);
  if (!text)
    return NULL;
  JParser jp = {text, 0};
  /* Skip a UTF-8 byte order mark written by some Windows editors. */
  if ((unsigned char)jp.p[0] == 0xEF && (unsigned char)jp.p[1] == 0xBB &&
      (unsigned char)jp.p[2] == 0xBF)
    jp.p += 3;
  JNode *root = j_value(&jp);
  free(text);
  if (root && root->type != J_OBJ) {
    j_free(root);
    root = NULL;
  }
  return root;
}

static JNode *j_get(const JNode *obj, const char *key) {
  if (!obj || obj->type != J_OBJ)
    return NULL;
  for (JNode *c = obj->child; c; c = c->next)
    if (c->key && strcmp(c->key, key) == 0)
      return c;
  return NULL;
}

static int j_string_into(const JNode *obj, const char *key, char *out,
                         size_t size) {
  JNode *n = j_get(obj, key);
  if (!n || n->type != J_STR)
    return 0;
  snprintf(out, size, "%s", n->str);
  return 1;
}

static int j_int(const JNode *obj, const char *key, int *out) {
  JNode *n = j_get(obj, key);
  if (!n)
    return 0;
  if (n->type == J_NUM || n->type == J_BOOL) {
    *out = (int)n->num;
    return 1;
  }
  if (n->type == J_STR && n->str[0]) {
    *out = atoi(n->str);
    return 1;
  }
  return 0;
}

static int j_bool(const JNode *obj, const char *key, int *out) {
  JNode *n = j_get(obj, key);
  if (!n)
    return 0;
  if (n->type == J_BOOL || n->type == J_NUM) {
    *out = n->num != 0;
    return 1;
  }
  if (n->type == J_STR) {
    *out = !strcasecmp(n->str, "true") || !strcmp(n->str, "1");
    return 1;
  }
  return 0;
}

/* Onion's hex2sdl accepts "#RRGGBB" (and tolerates a missing '#'). */
static int parse_hex_color(const char *s, uint8_t rgb[3]) {
  if (!s)
    return 0;
  if (*s == '#')
    s++;
  unsigned v = 0;
  int n = 0;
  for (; s[n] && isxdigit((unsigned char)s[n]) && n < 8; n++)
    v = v * 16 + (unsigned)(isdigit((unsigned char)s[n])
                                ? s[n] - '0'
                                : tolower((unsigned char)s[n]) - 'a' + 10);
  if (n == 3) {
    rgb[0] = (uint8_t)(((v >> 8) & 15) * 17);
    rgb[1] = (uint8_t)(((v >> 4) & 15) * 17);
    rgb[2] = (uint8_t)((v & 15) * 17);
    return 1;
  }
  if (n < 6)
    return 0;
  if (n == 8)
    v >>= 8; /* RRGGBBAA: drop alpha */
  rgb[0] = (uint8_t)(v >> 16);
  rgb[1] = (uint8_t)(v >> 8);
  rgb[2] = (uint8_t)v;
  return 1;
}

static int j_color(const JNode *obj, const char *key, uint16_t *out) {
  JNode *n = j_get(obj, key);
  uint8_t rgb[3];
  if (!n || n->type != J_STR || !parse_hex_color(n->str, rgb))
    return 0;
  *out = rgb565(rgb[0], rgb[1], rgb[2]);
  return 1;
}

/* ------------------------------------------------------------------------ */
/* Theme configuration (mirrors Onion's Theme_s and its fallback rules)      */
/* ------------------------------------------------------------------------ */

enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

typedef struct {
  char font[ONION_PATH_MAX];
  int size;
  uint16_t color;
} FontStyle;

typedef struct {
  char name[128];
  char author[128];
  char description[256];
  int hide_icon_labels;
  int hide_hint_labels;
  struct {
    int visible;
    char font[ONION_PATH_MAX];
    int size;
    uint16_t color;
    int offset_x, offset_y;
    int align;
    int fixed;
  } battery;
  int border_left, border_right;
  FontStyle title, hint, currentpage, total, list;
  struct {
    char font[ONION_PATH_MAX];
    int grid1x4, grid3x4;
    uint16_t color, selectedcolor;
  } grid;
} OnionConfig;

#define ONION_FALLBACK_FONT ONION_FALLBACK_DIR "/fonts/Exo-2-Bold-Italic.ttf"

static void config_defaults(OnionConfig *c) {
  memset(c, 0, sizeof *c);
  uint16_t white = rgb565(255, 255, 255);
  c->battery.size = 24;
  c->battery.color = white;
  snprintf(c->battery.font, sizeof c->battery.font, "%s", ONION_FALLBACK_FONT);
  FontStyle base = {ONION_FALLBACK_FONT, 36, 0};
  base.color = white;
  c->title = base;
  c->hint = base;
  c->hint.size = 40;
  c->currentpage.color = white;
  c->total.color = white;
  c->list = base;
  c->list.size = 24;
  snprintf(c->grid.font, sizeof c->grid.font, "%s", ONION_FALLBACK_FONT);
  c->grid.grid1x4 = 24;
  c->grid.grid3x4 = 18;
  c->grid.color = rgb565(104, 104, 104);
  c->grid.selectedcolor = white;
}

static void config_font_style(const JNode *obj, FontStyle *dest,
                              const FontStyle *fallback) {
  if (!j_string_into(obj, "font", dest->font, sizeof dest->font) && fallback)
    snprintf(dest->font, sizeof dest->font, "%s", fallback->font);
  if (!j_int(obj, "size", &dest->size) && fallback)
    dest->size = fallback->size;
  if (!j_color(obj, "color", &dest->color) && fallback)
    dest->color = fallback->color;
}

static int config_apply(OnionConfig *c, const char *path, int use_fallbacks) {
  JNode *root = j_parse_file(path);
  if (!root)
    return 0;
  JNode *battery = j_get(root, "batteryPercentage");
  JNode *labels = j_get(root, "hideLabels");
  JNode *frame = j_get(root, "frame");
  JNode *grid = j_get(root, "grid");

  j_string_into(root, "name", c->name, sizeof c->name);
  j_string_into(root, "author", c->author, sizeof c->author);
  j_string_into(root, "description", c->description, sizeof c->description);

  if (labels) {
    j_bool(labels, "icons", &c->hide_icon_labels);
    j_bool(labels, "hints", &c->hide_hint_labels);
  } else {
    int hide;
    if (j_bool(root, "hideIconTitle", &hide))
      c->hide_icon_labels = c->hide_hint_labels = hide;
  }

  config_font_style(j_get(root, "title"), &c->title, NULL);
  config_font_style(j_get(root, "hint"), &c->hint,
                    use_fallbacks ? &c->title : NULL);
  config_font_style(j_get(root, "currentpage"), &c->currentpage,
                    use_fallbacks ? &c->hint : NULL);
  config_font_style(j_get(root, "total"), &c->total,
                    use_fallbacks ? &c->hint : NULL);
  config_font_style(j_get(root, "list"), &c->list,
                    use_fallbacks ? &c->title : NULL);

  j_string_into(grid, "font", c->grid.font, sizeof c->grid.font);
  j_int(grid, "grid1x4", &c->grid.grid1x4);
  j_int(grid, "grid3x4", &c->grid.grid3x4);
  j_color(grid, "color", &c->grid.color);
  j_color(grid, "selectedcolor", &c->grid.selectedcolor);

  j_bool(battery, "visible", &c->battery.visible);
  if (!j_string_into(battery, "font", c->battery.font, sizeof c->battery.font) &&
      use_fallbacks)
    snprintf(c->battery.font, sizeof c->battery.font, "%s", c->hint.font);
  j_int(battery, "size", &c->battery.size);
  if (!j_color(battery, "color", &c->battery.color) && use_fallbacks)
    c->battery.color = c->hint.color;
  j_int(battery, "offsetX", &c->battery.offset_x);
  j_int(battery, "offsetY", &c->battery.offset_y);
  char align[16];
  int onleft;
  if (j_string_into(battery, "textAlign", align, sizeof align)) {
    c->battery.align = !strcmp(align, "center")  ? ALIGN_CENTER
                       : !strcmp(align, "right") ? ALIGN_RIGHT
                                                 : ALIGN_LEFT;
  } else if (j_bool(battery, "onleft", &onleft)) {
    c->battery.align = onleft ? ALIGN_RIGHT : ALIGN_LEFT;
  }
  j_bool(battery, "fixed", &c->battery.fixed);
  j_int(frame, "border-left", &c->border_left);
  j_int(frame, "border-right", &c->border_right);
  j_free(root);
  return 1;
}

/* ------------------------------------------------------------------------ */
/* Images                                                                    */
/* ------------------------------------------------------------------------ */

typedef struct {
  uint16_t *px;
  uint8_t *a;
  int w, h;
  int opaque;
  int loaded; /* 1 = tried (px may still be NULL when missing) */
} OImg;

static void img_free(OImg *im) {
  free(im->px);
  free(im->a);
  memset(im, 0, sizeof *im);
}

static int img_load_file(OImg *im, const char *path, int max_w, int max_h) {
  int w, h, ch;
  unsigned char *rgba = stbi_load(path, &w, &h, &ch, 4);
  if (!rgba)
    return 0;
  if (w <= 0 || h <= 0) {
    stbi_image_free(rgba);
    return 0;
  }
  int dw = w, dh = h;
  if (max_w > 0 && dw > max_w) {
    dh = dh * max_w / dw;
    dw = max_w;
  }
  if (max_h > 0 && dh > max_h) {
    dw = dw * max_h / dh;
    dh = max_h;
  }
  if (dw < 1)
    dw = 1;
  if (dh < 1)
    dh = 1;
  im->px = malloc((size_t)dw * dh * sizeof *im->px);
  im->a = malloc((size_t)dw * dh);
  if (!im->px || !im->a) {
    stbi_image_free(rgba);
    img_free(im);
    return 0;
  }
  int opaque = 1;
  int scaled = dw != w || dh != h;
  /* Backgrounds and bars are often smooth gradients; an ordered dither keeps
   * them from banding in the RGB565 framebuffer. Small art stays exact. */
  int dither = dw >= 320;
  for (int y = 0; y < dh; y++) {
    for (int x = 0; x < dw; x++) {
      unsigned r, g, b, a;
      if (!scaled) {
        const unsigned char *p = rgba + ((size_t)y * w + x) * 4;
        r = p[0], g = p[1], b = p[2], a = p[3];
      } else {
        /* Box filter: icons downscaled from large packs stay smooth. */
        int x0 = x * w / dw, x1 = (x + 1) * w / dw;
        int y0 = y * h / dh, y1 = (y + 1) * h / dh;
        if (x1 <= x0) x1 = x0 + 1;
        if (y1 <= y0) y1 = y0 + 1;
        unsigned long sr = 0, sg = 0, sb = 0, sa = 0, n = 0;
        for (int sy = y0; sy < y1; sy++)
          for (int sx = x0; sx < x1; sx++) {
            const unsigned char *p = rgba + ((size_t)sy * w + sx) * 4;
            sr += (unsigned long)p[0] * p[3];
            sg += (unsigned long)p[1] * p[3];
            sb += (unsigned long)p[2] * p[3];
            sa += p[3];
            n++;
          }
        a = (unsigned)(sa / n);
        r = sa ? (unsigned)(sr / sa) : 0;
        g = sa ? (unsigned)(sg / sa) : 0;
        b = sa ? (unsigned)(sb / sa) : 0;
      }
      size_t i = (size_t)y * dw + x;
      if (dither) {
        unsigned t = bayer4[y & 3][x & 3];
        r += t >> 1;
        g += t >> 2;
        b += t >> 1;
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
      }
      im->px[i] = rgb565(r, g, b);
      im->a[i] = (uint8_t)a;
      if (a != 255)
        opaque = 0;
    }
  }
  stbi_image_free(rgba);
  im->w = dw;
  im->h = dh;
  im->opaque = opaque;
  return 1;
}

/* Draw `im` scaled to dw x dh at (dx, dy), multiplying its alpha by `alpha`
 * (255 = as authored). Nearest-neighbour keeps pixel-art skins crisp. */
static void img_draw_ex(uint16_t *fb, const OImg *im, int sx, int sy, int sw,
                        int sh, int dx, int dy, int dw, int dh,
                        unsigned alpha) {
  if (!fb || !im || !im->px || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
    return;
  int W = SCREEN_WIDTH, H = SCREEN_HEIGHT;
  for (int y = 0; y < dh; y++) {
    int py = dy + y;
    if (py < 0 || py >= H)
      continue;
    int iy = sy + y * sh / dh;
    if (iy < 0 || iy >= im->h)
      continue;
    const uint16_t *srow = im->px + (size_t)iy * im->w;
    const uint8_t *arow = im->a + (size_t)iy * im->w;
    uint16_t *drow = fb + (size_t)py * W;
    for (int x = 0; x < dw; x++) {
      int px = dx + x;
      if (px < 0 || px >= W)
        continue;
      int ix = sx + (dw == sw ? x : x * sw / dw);
      if (ix < 0 || ix >= im->w)
        continue;
      unsigned a = arow[ix];
      if (alpha < 255)
        a = a * alpha / 255;
      if (a >= 255)
        drow[px] = srow[ix];
      else if (a)
        drow[px] = blend565_dither(drow[px], srow[ix], a, bayer4[py & 3][px & 3]);
    }
  }
}

static void img_draw(uint16_t *fb, const OImg *im, int x, int y) {
  if (im && im->px)
    img_draw_ex(fb, im, 0, 0, im->w, im->h, x, y, im->w, im->h, 255);
}

static void img_draw_alpha(uint16_t *fb, const OImg *im, int x, int y,
                           unsigned alpha) {
  if (im && im->px)
    img_draw_ex(fb, im, 0, 0, im->w, im->h, x, y, im->w, im->h, alpha);
}

/* Full-width bars are authored at 640 px; stretch them across wider panels. */
static void img_draw_wide(uint16_t *fb, const OImg *im, int y, int src_h) {
  if (!im || !im->px)
    return;
  int h = src_h > 0 && src_h < im->h ? src_h : im->h;
  int dw = im->w >= ONION_W ? SCREEN_WIDTH * im->w / ONION_W : im->w;
  img_draw_ex(fb, im, 0, 0, im->w, h, 0, y, dw, h, 255);
}

/* Skin images Onion loads by name (see Onion's theme/resources.h). */
enum {
  IMG_BACKGROUND, IMG_BG_TITLE, IMG_LOGO, IMG_BATTERY_0, IMG_BATTERY_20,
  IMG_BATTERY_50, IMG_BATTERY_80, IMG_BATTERY_100, IMG_BATTERY_100_BACK,
  IMG_BATTERY_CHARGING, IMG_BG_LIST_S, IMG_BG_LIST_L, IMG_DIVIDER,
  IMG_TOGGLE_ON, IMG_TOGGLE_OFF, IMG_BG_FOOTER, IMG_BUTTON_A, IMG_BUTTON_B,
  IMG_ARROW_LEFT, IMG_ARROW_RIGHT, IMG_PREVIEW_BG, IMG_DOT_ACTIVE,
  IMG_DOT_NEUTRAL, IMG_GAME_ITEM_N, IMG_GAME_ITEM_F, IMG_GS_TOP,
  IMG_GS_BOTTOM, IMG_FAVORITE_MARK, IMG_FOLDER, IMG_GS_ARROW_LEFT,
  IMG_GS_ARROW_RIGHT, IMG_GS_LEGEND,
  IMG_HOME_FIRST, /* ic-<kind>-n then ic-<kind>-f for each home tile */
  IMG_COUNT = IMG_HOME_FIRST + 12
};

static const char *const image_names[IMG_HOME_FIRST] = {
    "background", "bg-title", "miyoo-topbar", "power-0%-icon",
    "power-20%-icon", "power-50%-icon", "power-80%-icon", "power-full-icon",
    "power-full-icon_back", "ic-power-charge-100%", "bg-list-s", "bg-list-l",
    "div-line-h", "extra/toggle-on", "extra/toggle-off", "tips-bar-bg",
    "icon-A-54", "icon-B-54", "icon-left-arrow-24", "icon-right-arrow-24",
    "preview-bg", "dot-a", "dot-n", "bg-game-item-n", "bg-game-item-f",
    "extra/gs-top-bar", "extra/gs-bottom-bar", "ic-favorite-mark",
    "icon-folder", "extra/arrowLeft", "extra/arrowRight", "extra/gs-legend",
};

static const char *const home_image_keys[6] = {"recent", "favorite", "game",
                                               "app",    "setting",
                                               "retroarch"};

/* ------------------------------------------------------------------------ */
/* State                                                                     */
/* ------------------------------------------------------------------------ */

#define MAX_ONION_THEMES 96

typedef struct {
  char folder[256]; /* directory name, or zip file name */
  char label[128];
  int is_zip;
} ThemeEntry;

static ThemeEntry theme_list[MAX_ONION_THEMES];
static int theme_list_count;
static int theme_current = -1;
static char theme_path[ONION_PATH_MAX]; /* with trailing slash */
static OnionConfig cfg;
static OImg images[IMG_COUNT];
static uint16_t *bg_screen;   /* background scaled to the panel */
static int bg_screen_w, bg_screen_h;
static Theme palette;
static int16_t *change_sound;
static int change_sound_frames, change_sound_rate;
static int has_console_icons;

enum { FONT_TITLE, FONT_HINT, FONT_LIST, FONT_GRID, FONT_BATTERY,
       FONT_SLOTS };
/* Fonts are opened on first use: a CJK list font can be several megabytes
 * and the home screen never needs it. -2 = not opened yet. */
static int font_handles[FONT_SLOTS];
static char font_paths[FONT_SLOTS][ONION_PATH_MAX];
static int battery_font_is_exo;

/* Icon cache (console icons, app icons). */
#define ICON_CACHE_N 20
typedef struct {
  char path[ONION_PATH_MAX];
  OImg img;
  unsigned used;
} IconSlot;
static IconSlot icon_cache[ICON_CACHE_N];
static unsigned icon_clock;
#define ICON_MISS_N 64
static char icon_missing[ICON_MISS_N][ONION_PATH_MAX];
static int icon_missing_count, icon_missing_next;

/* ------------------------------------------------------------------------ */
/* Resource lookup                                                           */
/* ------------------------------------------------------------------------ */

/* Onion order: profile override, theme skin, then the system fallback. Images
 * under extra/ fall back to the flat resource folder in Onion; ours keeps the
 * extra/ subfolder inside the fallback skin. */
static int skin_path_ex(const char *name, char *out, size_t size,
                        int fallback) {
  snprintf(out, size, ONION_OVERRIDES_DIR "/skin/%s.png", name);
  if (file_exists(out))
    return 1;
  if (theme_path[0]) {
    snprintf(out, size, "%sskin/%s.png", theme_path, name);
    if (file_exists(out))
      return 1;
  }
  if (!fallback)
    return 0;
  snprintf(out, size, ONION_FALLBACK_DIR "/skin/%s.png", name);
  return file_exists(out);
}

/* The GameSwitcher bars and legend are optional theme art: a theme without
 * them gets the plain header and footer, not another theme's bars. */
static int theme_only_image(int id) {
  return id == IMG_GS_TOP || id == IMG_GS_BOTTOM || id == IMG_GS_LEGEND;
}

static const OImg *image(int id) {
  if (id < 0 || id >= IMG_COUNT)
    return NULL;
  OImg *im = &images[id];
  if (!im->loaded) {
    char name[64], path[ONION_PATH_MAX];
    if (id >= IMG_HOME_FIRST) {
      int k = (id - IMG_HOME_FIRST) / 2;
      snprintf(name, sizeof name, "ic-%s-%c", home_image_keys[k],
               ((id - IMG_HOME_FIRST) & 1) ? 'f' : 'n');
    } else {
      snprintf(name, sizeof name, "%s", image_names[id]);
    }
    if (skin_path_ex(name, path, sizeof path, !theme_only_image(id)))
      img_load_file(im, path, 0, 0);
    /* Miyoo themes store background.png upside down (stock MainUI blits it
     * straight to the rotated panel); Onion turns it round on load too. */
    if (id == IMG_BACKGROUND && im->px) {
      int n = im->w * im->h;
      for (int i = 0, j = n - 1; i < j; i++, j--) {
        uint16_t p = im->px[i];
        im->px[i] = im->px[j];
        im->px[j] = p;
        if (im->a) {
          uint8_t t = im->a[i];
          im->a[i] = im->a[j];
          im->a[j] = t;
        }
      }
    }
    im->loaded = 1;
  }
  return im->px ? im : NULL;
}

/* Onion uses power-full-icon_back when the theme ships no full icon of its
 * own (a fallback skin artefact); keep the same choice. */
static const OImg *battery_image(int pct, int charging) {
  if (charging)
    return image(IMG_BATTERY_CHARGING);
  if (pct < 5)
    return image(IMG_BATTERY_0);
  if (pct < 30)
    return image(IMG_BATTERY_20);
  if (pct < 60)
    return image(IMG_BATTERY_50);
  if (pct < 90)
    return image(IMG_BATTERY_80);
  const OImg *full = image(IMG_BATTERY_100);
  return full ? full : image(IMG_BATTERY_100_BACK);
}

static int font_candidate(char *out, size_t size, const char *dir,
                          const char *name) {
  snprintf(out, size, "%s/%s", dir, name);
  return file_exists(out);
}

/* Resolve a config.json font like Onion's theme_loadFont: absolute paths are
 * used as given (with /mnt/SDCARD mapped to this card), relative paths are
 * relative to the theme. Fonts Onion keeps in /mnt/SDCARD/miyoo/app are
 * looked up in ONION_FALLBACK_DIR/fonts, and anything missing falls back to
 * Exo 2 Bold Italic exactly as Onion does. */
static void resolve_font(const char *spec, char *out, size_t size) {
  char tmp[ONION_PATH_MAX];
  if (spec && *spec) {
    if (spec[0] == '/') {
      if (!strncasecmp(spec, "/mnt/SDCARD/", 12))
        snprintf(tmp, sizeof tmp, "/mnt/sdcard/%s", spec + 12);
      else
        snprintf(tmp, sizeof tmp, "%s", spec);
    } else {
      snprintf(tmp, sizeof tmp, "%s%s", theme_path, spec);
    }
    if (file_exists(tmp)) {
      snprintf(out, size, "%s", tmp);
      return;
    }
    const char *base = path_basename(spec);
    char theme_dir[ONION_PATH_MAX];
    snprintf(theme_dir, sizeof theme_dir, "%.*s", (int)strlen(theme_path) - 1,
             theme_path);
    if ((theme_path[0] && font_candidate(out, size, theme_dir, base)) ||
        font_candidate(out, size, "/mnt/sdcard/miyoo/app", base) ||
        font_candidate(out, size, ONION_FALLBACK_DIR "/fonts", base) ||
        font_candidate(out, size, "/mnt/sdcard/frogui/fonts", base) ||
        font_candidate(out, size, "/mnt/sdcard/cubegm/fonts", base))
      return;
    /* Onion's bundled system fonts that cannot ship here map to the closest
     * face TreeFrogUI already carries. */
    static const struct {
      const char *onion;
      const char *local;
    } aliases[] = {
        {"wqy-microhei.ttc", "TreeFrogUnicode.ttf"},        /* WenQuanYi */
        {"MicrosoftYaHeiGB.ttf", "TreeFrogUnicode.ttf"},
        {"Exo-2-Bold-Italic_Universal.ttf", "Exo-2-Bold-Italic.ttf"},
        {"HENB.TTF", "TreeFrogLatin.ttf"},                   /* Helvetica */
        {"Helvetica-Neue-2.ttf", "TreeFrogLatin.ttf"},
    };
    for (size_t i = 0; i < sizeof aliases / sizeof aliases[0]; i++)
      if (!strcasecmp(base, aliases[i].onion) &&
          (font_candidate(out, size, ONION_FALLBACK_DIR "/fonts", aliases[i].local) ||
           font_candidate(out, size, "/mnt/sdcard/frogui/fonts", aliases[i].local) ||
           font_candidate(out, size, "/mnt/sdcard/cubegm/fonts", aliases[i].local) ||
           font_candidate(out, size, "fonts", aliases[i].local)))
        return;
  }
  snprintf(out, size, "%s", ONION_FALLBACK_FONT);
}

static void resolve_fonts(void) {
  const char *specs[FONT_SLOTS] = {cfg.title.font, cfg.hint.font,
                                   cfg.list.font, cfg.grid.font,
                                   cfg.battery.font};
  for (int i = 0; i < FONT_SLOTS; i++) {
    resolve_font(specs[i], font_paths[i], sizeof font_paths[i]);
    font_handles[i] = -2;
  }
  const char *battery = path_basename(font_paths[FONT_BATTERY]);
  battery_font_is_exo = !strncasecmp(battery, "Exo-2", 5) ||
                        !strncasecmp(battery, "Exo2", 4);
}

/* The theme's title font file, for picoarch's in-game menu (skin.txt). */
const char *onion_theme_menu_font(void) {
  const char *p = font_paths[FONT_TITLE];
  if (!p[0] || !file_exists(p))
    p = ONION_FALLBACK_FONT;
  return file_exists(p) ? p : NULL;
}

/* ------------------------------------------------------------------------ */
/* Text                                                                      */
/* ------------------------------------------------------------------------ */

static int text_height(void) {
  int asc, desc;
  font_line_metrics(&asc, &desc);
  return asc + desc + 1;
}

static void use_font(int slot, int size) {
  if (size < 6)
    size = 6;
  if (size > 96)
    size = 96;
  if (font_handles[slot] == -2) {
    font_handles[slot] = font_face_open(font_paths[slot]);
    if (font_handles[slot] < 0)
      font_handles[slot] = font_face_open(ONION_FALLBACK_FONT);
  }
  font_face_use(font_handles[slot], size);
}

/* Draw text with its line box vertically centred on center_y (Onion centres
 * the rendered TTF surface). Returns the text width. */
static int text_centered_y(uint16_t *fb, int x, int center_y, const char *s,
                           uint16_t color, int bold) {
  int w = font_measure_text_bold(s, bold);
  int y = center_y - text_height() / 2;
  font_draw_text_bold(fb, SCREEN_WIDTH, SCREEN_HEIGHT, x, y, s, color, bold);
  return w;
}

/* Copy `src` into `dst`, shortened with "..." to fit max_w pixels. */
static void fit_text(const char *src, char *dst, size_t size, int max_w,
                     int bold) {
  snprintf(dst, size, "%s", src);
  if (max_w <= 0 || font_measure_text_bold(dst, bold) <= max_w)
    return;
  size_t keep = strlen(src);
  if (keep > size - 4)
    keep = size - 4;
  while (keep > 0) {
    keep--;
    /* Do not cut through a UTF-8 sequence. */
    while (keep > 0 && ((unsigned char)src[keep] & 0xC0) == 0x80)
      keep--;
    snprintf(dst, size, "%.*s...", (int)keep, src);
    if (font_measure_text_bold(dst, bold) <= max_w)
      return;
  }
  snprintf(dst, size, "...");
}

/* ------------------------------------------------------------------------ */
/* Palette                                                                   */
/* ------------------------------------------------------------------------ */

static uint16_t average_over(const OImg *im, uint16_t under, int x0, int y0,
                             int w, int h) {
  if (!im)
    return under;
  unsigned long r = 0, g = 0, b = 0, n = 0;
  for (int y = y0; y < y0 + h && y < im->h; y += 2)
    for (int x = x0; x < x0 + w && x < im->w; x += 2) {
      size_t i = (size_t)y * im->w + x;
      uint16_t c = blend565(under, im->px[i], im->a[i]);
      r += (c >> 11) & 31;
      g += (c >> 5) & 63;
      b += c & 31;
      n++;
    }
  if (!n)
    return under;
  return (uint16_t)(((r / n) << 11) | ((g / n) << 5) | (b / n));
}

static uint16_t contrast_text(uint16_t bg, uint16_t preferred) {
  int d = (int)luma565(preferred) - (int)luma565(bg);
  if (d < 0)
    d = -d;
  if (d >= 90)
    return preferred;
  return luma565(bg) > 140 ? rgb565(0, 0, 0) : rgb565(255, 255, 255);
}

static uint16_t mix565(uint16_t a, uint16_t b, unsigned t) {
  return blend565(a, b, t);
}

static void build_palette(void) {
  const OImg *bg = image(IMG_BACKGROUND);
  uint16_t base = bg ? average_over(bg, 0, 0, 60, bg->w, bg->h - 120)
                     : rgb565(30, 30, 36);
  const OImg *sel = image(IMG_BG_LIST_S);
  uint16_t select_bg = sel ? average_over(sel, base, 0, 0, sel->w, sel->h)
                           : cfg.grid.selectedcolor;
  /* A nearly transparent highlight would vanish in palette-only screens. */
  int diff = (int)luma565(select_bg) - (int)luma565(base);
  if (diff < 0)
    diff = -diff;
  if (diff < 24)
    select_bg = mix565(base, cfg.grid.selectedcolor, 90);
  const OImg *foot = image(IMG_BG_FOOTER);
  uint16_t legend_bg = foot ? average_over(foot, base, 0, 0, foot->w, foot->h)
                            : mix565(base, 0, 80);
  if (legend_bg == base)
    legend_bg = mix565(base, luma565(base) > 128 ? 0 : 0xFFFF, 40);

  palette.name = "OnionOS";
  palette.bg = base;
  palette.text = contrast_text(base, cfg.list.color);
  palette.folder = palette.text;
  palette.select_bg = select_bg;
  palette.select_text = contrast_text(select_bg, cfg.list.color);
  palette.header = contrast_text(base, cfg.title.color);
  palette.legend = contrast_text(legend_bg, cfg.hint.color);
  palette.legend_bg = legend_bg;
  palette.disabled = cfg.grid.color;
}

/* ------------------------------------------------------------------------ */
/* Sound                                                                     */
/* ------------------------------------------------------------------------ */

static uint32_t le32(const unsigned char *p) {
  return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t le16(const unsigned char *p) { return p[0] | (p[1] << 8); }

/* Accept 8/16-bit PCM WAV, mono or stereo; keep at most half a second. */
static void load_change_sound(void) {
  char path[ONION_PATH_MAX];
  snprintf(path, sizeof path, "%ssound/change.wav", theme_path);
  if (!file_exists(path))
    return;
  long size;
  unsigned char *data = (unsigned char *)read_file(path, &size, 4 * 1024 * 1024);
  if (!data)
    return;
  if (size < 44 || memcmp(data, "RIFF", 4) || memcmp(data + 8, "WAVE", 4)) {
    free(data);
    return;
  }
  int channels = 0, bits = 0, format = 0;
  uint32_t rate = 0;
  const unsigned char *pcm = NULL;
  uint32_t pcm_len = 0;
  for (long off = 12; off + 8 <= size;) {
    uint32_t len = le32(data + off + 4);
    const unsigned char *body = data + off + 8;
    if (off + 8 + (long)len > size)
      len = (uint32_t)(size - off - 8);
    if (!memcmp(data + off, "fmt ", 4) && len >= 16) {
      format = le16(body);
      channels = le16(body + 2);
      rate = le32(body + 4);
      bits = le16(body + 14);
    } else if (!memcmp(data + off, "data", 4)) {
      pcm = body;
      pcm_len = len;
    }
    off += 8 + len + (len & 1);
  }
  if (format != 1 || (channels != 1 && channels != 2) ||
      (bits != 8 && bits != 16) || !pcm || rate < 4000) {
    free(data);
    return;
  }
  int frame_bytes = channels * bits / 8;
  int frames = (int)(pcm_len / (uint32_t)frame_bytes);
  if (frames > (int)rate / 2)
    frames = (int)rate / 2;
  change_sound = malloc((size_t)frames * 2 * sizeof(int16_t));
  if (change_sound) {
    for (int i = 0; i < frames; i++) {
      const unsigned char *f = pcm + (size_t)i * frame_bytes;
      int16_t l, r;
      if (bits == 16) {
        l = (int16_t)le16(f);
        r = channels == 2 ? (int16_t)le16(f + 2) : l;
      } else {
        l = (int16_t)((f[0] - 128) << 8);
        r = channels == 2 ? (int16_t)((f[1] - 128) << 8) : l;
      }
      change_sound[i * 2] = l;
      change_sound[i * 2 + 1] = r;
    }
    change_sound_frames = frames;
    change_sound_rate = (int)rate;
  }
  free(data);
}

int onion_theme_change_sound(int rate, const int16_t **frames) {
  static int16_t *resampled;
  static int resampled_rate, resampled_frames;
  if (!change_sound || rate <= 0 || !frames)
    return 0;
  if (rate == change_sound_rate) {
    *frames = change_sound;
    return change_sound_frames;
  }
  if (!resampled || resampled_rate != rate) {
    int n = (int)((long long)change_sound_frames * rate / change_sound_rate);
    int16_t *buf = realloc(resampled, (size_t)(n > 0 ? n : 1) * 2 * sizeof *buf);
    if (!buf)
      return 0;
    resampled = buf;
    for (int i = 0; i < n; i++) {
      int src = (int)((long long)i * change_sound_rate / rate);
      resampled[i * 2] = change_sound[src * 2];
      resampled[i * 2 + 1] = change_sound[src * 2 + 1];
    }
    resampled_rate = rate;
    resampled_frames = n;
  }
  *frames = resampled;
  return resampled_frames;
}

/* ------------------------------------------------------------------------ */
/* Zip install (Onion theme catalogue zips: stored or deflate entries)       */
/* ------------------------------------------------------------------------ */

static int zip_safe_name(const char *name) {
  if (!*name || name[0] == '/' || strstr(name, "..") || strchr(name, '\\') ||
      strchr(name, ':'))
    return 0;
  return 1;
}

/* Extract `zip_path` into ONION_THEMES_DIR. Entries normally sit inside one
 * top-level theme folder; zips with config.json at the root are unpacked into
 * a folder named after the zip. Writes the resulting folder to out_folder. */
static int zip_install(const char *zip_path, char *out_folder, size_t out_size) {
  long size;
  unsigned char *zip = (unsigned char *)read_file(zip_path, &size, 64L * 1024 * 1024);
  if (!zip)
    return 0;
  long eocd = -1;
  for (long i = size - 22; i >= 0 && i >= size - 22 - 65535; i--)
    if (le32(zip + i) == 0x06054b50) {
      eocd = i;
      break;
    }
  if (eocd < 0) {
    free(zip);
    return 0;
  }
  int entries = le16(zip + eocd + 10);
  uint32_t cd = le32(zip + eocd + 16);

  /* Pass 1: find whether every entry shares one top-level folder. */
  char top[256] = "";
  int shared_top = 1;
  uint32_t off = cd;
  for (int e = 0; e < entries; e++) {
    if (off + 46 > (uint32_t)size || le32(zip + off) != 0x02014b50) {
      free(zip);
      return 0;
    }
    int name_len = le16(zip + off + 28);
    int extra_len = le16(zip + off + 30);
    int comment_len = le16(zip + off + 32);
    char name[512];
    snprintf(name, sizeof name, "%.*s", name_len, (const char *)zip + off + 46);
    const char *slash = strchr(name, '/');
    if (!slash) {
      shared_top = 0;
    } else if (!top[0]) {
      snprintf(top, sizeof top, "%.*s", (int)(slash - name), name);
    } else if (strncmp(name, top, strlen(top)) || name[strlen(top)] != '/') {
      shared_top = 0;
    }
    off += 46 + name_len + extra_len + comment_len;
  }
  char prefix[300] = "";
  if (shared_top && top[0]) {
    snprintf(out_folder, out_size, "%s", top);
  } else {
    char stem[256];
    snprintf(stem, sizeof stem, "%s", path_basename(zip_path));
    char *dot = strrchr(stem, '.');
    if (dot)
      *dot = '\0';
    snprintf(out_folder, out_size, "%s", stem);
    snprintf(prefix, sizeof prefix, "%s/", stem);
  }

  /* Pass 2: extract. */
  int ok = 1;
  off = cd;
  for (int e = 0; e < entries && ok; e++) {
    int method = le16(zip + off + 10);
    uint32_t comp = le32(zip + off + 20);
    uint32_t uncomp = le32(zip + off + 24);
    int name_len = le16(zip + off + 28);
    int extra_len = le16(zip + off + 30);
    int comment_len = le16(zip + off + 32);
    uint32_t local = le32(zip + off + 42);
    char name[512];
    snprintf(name, sizeof name, "%.*s", name_len, (const char *)zip + off + 46);
    off += 46 + name_len + extra_len + comment_len;
    if (!zip_safe_name(name) || !strncmp(name, "__MACOSX/", 9))
      continue;
    char dest[ONION_PATH_MAX * 2];
    snprintf(dest, sizeof dest, ONION_THEMES_DIR "/%s%s", prefix, name);
    if (name[strlen(name) - 1] == '/') {
      mkdir_parents(dest);
      mkdir(dest, 0755);
      continue;
    }
    if (local + 30 > (uint32_t)size || le32(zip + local) != 0x04034b50) {
      ok = 0;
      break;
    }
    uint32_t data_off = local + 30 + le16(zip + local + 26) + le16(zip + local + 28);
    if (data_off + comp > (uint32_t)size) {
      ok = 0;
      break;
    }
    char *out = NULL;
    int out_len = 0;
    if (method == 0) {
      out = malloc(comp ? comp : 1);
      if (out)
        memcpy(out, zip + data_off, comp);
      out_len = (int)comp;
    } else if (method == 8) {
      out = stbi_zlib_decode_noheader_malloc((const char *)zip + data_off,
                                             (int)comp, &out_len);
    }
    if (!out || (uint32_t)out_len != uncomp) {
      free(out);
      continue; /* skip entries we cannot decode rather than abort */
    }
    mkdir_parents(dest);
    FILE *f = fopen(dest, "wb");
    if (f) {
      if (fwrite(out, 1, (size_t)out_len, f) != (size_t)out_len)
        ok = 0;
      fclose(f);
    } else {
      ok = 0;
    }
    free(out);
  }
  free(zip);
  if (ok)
    sync();
  return ok;
}

/* ------------------------------------------------------------------------ */
/* Discovery                                                                 */
/* ------------------------------------------------------------------------ */

static void theme_label_for(const char *dir_path, const char *fallback,
                            char *out, size_t size) {
  char cfg_path[ONION_PATH_MAX];
  snprintf(cfg_path, sizeof cfg_path, "%s/config.json", dir_path);
  JNode *root = j_parse_file(cfg_path);
  char name[128] = "", author[128] = "";
  if (root) {
    j_string_into(root, "name", name, sizeof name);
    j_string_into(root, "author", author, sizeof author);
    j_free(root);
  }
  /* Catalogue folders are "<Name> by <Author>"; keep them as they are. */
  if (name[0] && !contains_ci(fallback, name))
    snprintf(out, size, author[0] ? "%s by %s" : "%s", name, author);
  else
    snprintf(out, size, "%s", fallback);
}

static int theme_cmp(const void *a, const void *b) {
  return strcasecmp(((const ThemeEntry *)a)->label,
                    ((const ThemeEntry *)b)->label);
}

void onion_theme_scan(void) {
  char current[256] = "";
  if (theme_current >= 0 && theme_current < theme_list_count)
    snprintf(current, sizeof current, "%s", theme_list[theme_current].folder);
  theme_list_count = 0;
  DIR *dp = opendir(ONION_THEMES_DIR);
  if (!dp) {
    theme_current = -1;
    return;
  }
  struct dirent *e;
  while ((e = readdir(dp)) && theme_list_count < MAX_ONION_THEMES) {
    if (e->d_name[0] == '.')
      continue;
    char full[ONION_PATH_MAX];
    snprintf(full, sizeof full, ONION_THEMES_DIR "/%s", e->d_name);
    ThemeEntry *t = &theme_list[theme_list_count];
    if (dir_exists(full)) {
      char skin_dir[ONION_PATH_MAX + 16], cfg_path[ONION_PATH_MAX + 16];
      snprintf(skin_dir, sizeof skin_dir, "%s/skin", full);
      snprintf(cfg_path, sizeof cfg_path, "%s/config.json", full);
      if (!dir_exists(skin_dir) && !file_exists(cfg_path))
        continue;
      snprintf(t->folder, sizeof t->folder, "%s", e->d_name);
      theme_label_for(full, e->d_name, t->label, sizeof t->label);
      t->is_zip = 0;
      theme_list_count++;
    } else if (has_suffix_ci(e->d_name, ".zip")) {
      snprintf(t->folder, sizeof t->folder, "%s", e->d_name);
      snprintf(t->label, sizeof t->label, "%.*s (zip)",
               (int)strlen(e->d_name) - 4, e->d_name);
      t->is_zip = 1;
      theme_list_count++;
    }
  }
  closedir(dp);
  /* A zip that has already been unpacked is listed once, as its folder. */
  for (int i = 0; i < theme_list_count; i++) {
    if (!theme_list[i].is_zip)
      continue;
    char stem[256];
    snprintf(stem, sizeof stem, "%.*s", (int)strlen(theme_list[i].folder) - 4,
             theme_list[i].folder);
    for (int j = 0; j < theme_list_count; j++)
      if (!theme_list[j].is_zip && !strcasecmp(theme_list[j].folder, stem)) {
        theme_list[i] = theme_list[--theme_list_count];
        i--;
        break;
      }
  }
  qsort(theme_list, (size_t)theme_list_count, sizeof theme_list[0], theme_cmp);
  theme_current = current[0] ? onion_theme_index_of(current) : -1;
}

int onion_theme_count(void) { return theme_list_count; }

const char *onion_theme_folder(int index) {
  return index >= 0 && index < theme_list_count ? theme_list[index].folder : "";
}

const char *onion_theme_label(int index) {
  return index >= 0 && index < theme_list_count ? theme_list[index].label : "";
}

int onion_theme_index_of(const char *folder) {
  if (!folder || !*folder)
    return -1;
  for (int i = 0; i < theme_list_count; i++)
    if (!strcasecmp(theme_list[i].folder, folder))
      return i;
  return -1;
}

int onion_theme_active(void) { return theme_current >= 0; }
int onion_theme_current(void) { return theme_current; }
const Theme *onion_theme_palette(void) {
  return theme_current >= 0 ? &palette : NULL;
}
int onion_theme_has_console_icons(void) { return has_console_icons; }

void onion_theme_unload(void) {
  font_face_close_all();
  for (int i = 0; i < IMG_COUNT; i++)
    img_free(&images[i]);
  for (int i = 0; i < ICON_CACHE_N; i++) {
    img_free(&icon_cache[i].img);
    icon_cache[i].path[0] = '\0';
  }
  icon_missing_count = icon_missing_next = 0;
  free(bg_screen);
  bg_screen = NULL;
  bg_screen_w = bg_screen_h = 0;
  free(change_sound);
  change_sound = NULL;
  change_sound_frames = 0;
  for (int i = 0; i < FONT_SLOTS; i++)
    font_handles[i] = -1;
  theme_path[0] = '\0';
  theme_current = -1;
  has_console_icons = 0;
}

int onion_theme_select(int index) {
  onion_theme_unload();
  if (index < 0 || index >= theme_list_count)
    return index < 0;
  ThemeEntry *t = &theme_list[index];
  if (t->is_zip) {
    char zip_path[ONION_PATH_MAX], folder[256];
    snprintf(zip_path, sizeof zip_path, ONION_THEMES_DIR "/%s", t->folder);
    if (!zip_install(zip_path, folder, sizeof folder))
      return 0;
    /* Keep the zip: deleting user files is not ours to do. The scan lists
     * the unpacked folder from now on. */
    int existing = onion_theme_index_of(folder);
    if (existing >= 0 && existing != index) {
      /* The zip unpacked into a theme that is already listed. */
      memmove(&theme_list[index], &theme_list[index + 1],
              (size_t)(theme_list_count - index - 1) * sizeof theme_list[0]);
      theme_list_count--;
      index = existing > index ? existing - 1 : existing;
      t = &theme_list[index];
    } else {
      snprintf(t->folder, sizeof t->folder, "%s", folder);
      char full[ONION_PATH_MAX];
      snprintf(full, sizeof full, ONION_THEMES_DIR "/%s", folder);
      theme_label_for(full, folder, t->label, sizeof t->label);
      t->is_zip = 0;
    }
  }
  snprintf(theme_path, sizeof theme_path, ONION_THEMES_DIR "/%s/", t->folder);
  if (!dir_exists(theme_path)) {
    theme_path[0] = '\0';
    return 0;
  }
  config_defaults(&cfg);
  char path[ONION_PATH_MAX];
  snprintf(path, sizeof path, "%sconfig.json", theme_path);
  if (!config_apply(&cfg, path, 1))
    config_apply(&cfg, ONION_FALLBACK_DIR "/config.json", 1);
  config_apply(&cfg, ONION_OVERRIDES_DIR "/config.json", 0);
  theme_current = index;
  resolve_fonts();
  build_palette();
  load_change_sound();
  snprintf(path, sizeof path, "%sicons", theme_path);
  has_console_icons = dir_exists(path);
  return 1;
}

/* ------------------------------------------------------------------------ */
/* Icons                                                                     */
/* ------------------------------------------------------------------------ */

static const OImg *icon_load(const char *path, int max_w, int max_h) {
  if (!path)
    return NULL;
  for (int i = 0; i < ICON_CACHE_N; i++)
    if (icon_cache[i].img.px && !strcmp(icon_cache[i].path, path)) {
      icon_cache[i].used = ++icon_clock;
      return &icon_cache[i].img;
    }
  for (int i = 0; i < icon_missing_count; i++)
    if (!strcmp(icon_missing[i], path))
      return NULL;
  int victim = 0;
  for (int i = 1; i < ICON_CACHE_N; i++)
    if (!icon_cache[i].img.px || icon_cache[i].used < icon_cache[victim].used)
      victim = i;
  img_free(&icon_cache[victim].img);
  if (!file_exists(path) ||
      !img_load_file(&icon_cache[victim].img, path, max_w, max_h)) {
    snprintf(icon_missing[icon_missing_next], ONION_PATH_MAX, "%s", path);
    icon_missing_next = (icon_missing_next + 1) % ICON_MISS_N;
    if (icon_missing_count < ICON_MISS_N)
      icon_missing_count++;
    return NULL;
  }
  snprintf(icon_cache[victim].path, sizeof icon_cache[victim].path, "%s", path);
  icon_cache[victim].used = ++icon_clock;
  return &icon_cache[victim].img;
}

/* TreeFrogUI ROM folder -> Onion icon names (Onion's Roms/<SYS> naming). */
static const struct {
  const char *folder;
  const char *icons;
} console_aliases[] = {
    {"nes", "fc"},          {"nesq", "fc"},       {"nest", "fc"},
    {"fds", "fds"},         {"snes", "sfc"},      {"snes02", "sfc"},
    {"gb", "gb"},           {"gbgb", "gb"},       {"gbb", "gb"},
    {"dblcherrygb", "gb"},  {"gbc", "gbc"},       {"gba", "gba"},
    {"gbac", "gba"},        {"gbav", "gba"},      {"mgba", "gba"},
    {"gbaf", "gba"},        {"sega", "md"},       {"gpgx", "md"},
    {"segacd", "segacd"},   {"32x", "32X"},       {"gg", "gg"},
    {"sms", "ms"},          {"a26", "atari"},     {"a5200", "5200"},
    {"a78", "7800"},        {"a800", "atari"},    {"lnx", "lynx"},
    {"pce", "pce"},         {"pcesgx", "sgfx"},   {"ngpc", "ngp"},
    {"wswan", "ws"},        {"wsv", "supervision"}, {"vb", "vb"},
    {"msx", "msx"},         {"c64", "c64"},       {"c64sc", "c64"},
    {"c64f", "c64"},        {"c64fc", "c64"},     {"vic20", "c64"},
    {"amstrad", "cpc"},     {"amstradb", "cpc"},  {"spec", "zxs"},
    {"zx81", "zxs"},        {"col", "col"},       {"quake", "ports"},
    {"quake2", "ports"},    {"outrun", "ports"},  {"wolf3d", "ports"},
    {"prboom", "ports"},    {"cavestory", "ports"}, {"flashback", "ports"},
    {"xrick", "ports"},     {"jnb", "ports"},     {"classicube", "ports"},
    {"mcpe", "ports"},      {"openclaw", "ports"}, {"diablo", "ports"},
    {"scummvm", "scummvm"}, {"gw", "gw"},         {"pico8", "pico"},
    {"fake08", "pico"},     {"retro8", "pico"},   {"pico286", "dos"},
    {"nds", "nds"},         {"tic80", "tic"},     {"m2k", "arcade"},
    {"arcade", "arcade"},   {"cps1", "cps1"},     {"cps2", "cps2"},
    {"cps3", "cps3"},       {"neogeo", "neogeo"}, {"geolith", "neogeo"},
    {"pokem", "poke"},      {"int", "itv"},       {"fcf", "fairchild"},
    {"vec", "vectrex"},     {"o2em", "ody"},      {"amiga", "amiga"},
    {"atari-st", "atarist"}, {"ps1", "ps"},       {"psx", "ps"},
    {"ps1r", "ps"},         {"qpsx", "ps"},       {"psp", "psp"},
    {"md", "md"},           {"fc", "fc"},         {"sfc", "sfc"},
    {"ps", "ps"},
};

static const OImg *console_icon(const char *folder, int selected) {
  if (!has_console_icons || !folder || !*folder)
    return NULL;
  const char *names[4] = {folder, NULL, NULL, NULL};
  int n = 1;
  for (size_t i = 0; i < sizeof console_aliases / sizeof console_aliases[0]; i++)
    if (!strcasecmp(console_aliases[i].folder, folder)) {
      names[n++] = console_aliases[i].icons;
      break;
    }
  /* ShallotsOS (Recalbox) folder names: the system's Onion icon name. */
  const ShSystem *sys = systems_find_exact(folder);
  if (sys && n < 4 && strcasecmp(sys->icon, folder))
    names[n++] = sys->icon;
  /* A theme with no Game Boy Color art: its Game Boy icon keeps the style. */
  if (n < 4 && !strcasecmp(folder, "gbc"))
    names[n++] = "gb";
  char path[ONION_PATH_MAX];
  for (int s = selected ? 0 : 1; s < 2; s++) {
    for (int i = 0; i < n; i++) {
      snprintf(path, sizeof path, "%sicons/%s%s.png", theme_path,
               s == 0 ? "sel/" : "", names[i]);
      const OImg *im = icon_load(path, 150, 146);
      if (im)
        return im;
      /* Onion icon names are lower case; try that spelling too. */
      char lower[64];
      snprintf(lower, sizeof lower, "%s", names[i]);
      int changed = 0;
      for (char *p = lower; *p; p++)
        if (*p >= 'A' && *p <= 'Z') {
          *p += 32;
          changed = 1;
        }
      if (changed) {
        snprintf(path, sizeof path, "%sicons/%s%s.png", theme_path,
                 s == 0 ? "sel/" : "", lower);
        if ((im = icon_load(path, 150, 146)))
          return im;
      }
    }
  }
  return NULL;
}

const char *onion_theme_app_icon(const char *app_key) {
  static const struct {
    const char *key;
    const char *icon;
  } map[] = {
      {"activity", "activity"}, {"frogshell", "commander"},
      {"usbmode", "ftp"},       {"ebook", "ereader"},
      {"images", "gallery"},    {"videos", "ffplay"},
      {"music", "music"},       {"rockbox", "music"},
      {"themes", "themes"},
      {"shutdown", "tweaks_system"},
  };
  static char path[ONION_PATH_MAX];
  if (!onion_theme_active() || !app_key)
    return NULL;
  for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
    if (!strcmp(map[i].key, app_key)) {
      snprintf(path, sizeof path, "%sicons/app/%s.png", theme_path, map[i].icon);
      if (file_exists(path))
        return path;
      snprintf(path, sizeof path, ONION_FALLBACK_DIR "/icons/app/%s.png",
               map[i].icon);
      return file_exists(path) ? path : NULL;
    }
  return NULL;
}

/* ------------------------------------------------------------------------ */
/* Rendering                                                                 */
/* ------------------------------------------------------------------------ */

static int center_offset(void) { return (SCREEN_WIDTH - ONION_W) / 2; }
static int right_x(int x) { return x + SCREEN_WIDTH - ONION_W; }

void onion_render_background(uint16_t *fb) {
  if (!fb)
    return;
  const OImg *bg = image(IMG_BACKGROUND);
  int W = SCREEN_WIDTH, H = SCREEN_HEIGHT;
  if (!bg) {
    for (int i = 0; i < W * H; i++)
      fb[i] = palette.bg;
    return;
  }
  if (!bg_screen || bg_screen_w != W || bg_screen_h != H) {
    free(bg_screen);
    bg_screen = malloc((size_t)W * H * sizeof *bg_screen);
    if (!bg_screen) {
      for (int i = 0; i < W * H; i++)
        fb[i] = palette.bg;
      img_draw_ex(fb, bg, 0, 0, bg->w, bg->h, 0, 0, W, H, 255);
      return;
    }
    for (int i = 0; i < W * H; i++)
      bg_screen[i] = 0;
    img_draw_ex(bg_screen, bg, 0, 0, bg->w, bg->h, 0, 0, W, H, 255);
    bg_screen_w = W;
    bg_screen_h = H;
  }
  memcpy(fb, bg_screen, (size_t)W * H * sizeof *fb);
}

/* Compose Onion's battery widget (theme_batterySurfaceWithBg) centred on
 * (cx, cy). */
static void onion_draw_battery(uint16_t *fb, int cx, int cy) {
  extern int frogui_battery_pct(void);
  extern int frogui_battery_charging(void);
  int pct = frogui_battery_pct();
  int charging = frogui_battery_charging();
  if (pct < 0)
    return;
  if (pct > 100)
    pct = 100;
  const OImg *icon = battery_image(pct, charging);
  int visible = cfg.battery.visible && !charging;
  char text[8];
  snprintf(text, sizeof text, "%d%%", pct);
  use_font(FONT_BATTERY, cfg.battery.size);
  int text_w = font_measure_text_bold(text, 0);
  int text_h = text_height();
  int offset_y = cfg.battery.offset_y;
  if (battery_font_is_exo)
    offset_y -= (int)(0.075 * text_h);
  int icon_w = icon ? icon->w : 0, icon_h = icon ? icon->h : 0;
  if (icon_w > ONION_W)
    visible = 0;
  const int spacer = 5;
  int img_w = 2 * (text_w + spacer) + icon_w;
  int img_h = text_h > icon_h ? text_h : icon_w;
  if (!visible) {
    img_w = icon_w;
    img_h = icon_h;
  } else if (cfg.battery.fixed || cfg.battery.align == ALIGN_CENTER) {
    img_w = icon_w > text_w ? icon_w : text_w;
  }
  img_w += img_w & 1;
  img_h += img_h & 1;
  if (img_w < 48)
    img_w = 48;
  if (img_h < 48)
    img_h = 48;
  int icon_x = 0, icon_y = (img_h - icon_h) / 2;
  int text_x = 0, text_y = (img_h - text_h) / 2 + offset_y;
  if (visible) {
    if (cfg.battery.fixed) {
      text_x = cfg.battery.align == ALIGN_RIGHT    ? icon_w - text_w
               : cfg.battery.align == ALIGN_CENTER ? (icon_w - text_w) / 2
                                                   : 0;
      text_x += cfg.battery.offset_x;
    } else if (cfg.battery.align == ALIGN_RIGHT) {
      icon_x = text_w + spacer;
      text_x = cfg.battery.offset_x;
    } else if (cfg.battery.align == ALIGN_CENTER) {
      text_x = (icon_w - text_w) / 2 + cfg.battery.offset_x;
    } else {
      text_x = icon_w + spacer + cfg.battery.offset_x;
    }
  }
  int ox = cx - img_w / 2, oy = cy - img_h / 2;
  if (icon)
    img_draw(fb, icon, ox + icon_x, oy + icon_y);
  if (visible)
    font_draw_text_bold(fb, SCREEN_WIDTH, SCREEN_HEIGHT, ox + text_x,
                        oy + text_y, text, cfg.battery.color, 0);
  font_face_use(-1, 0);
}

void onion_render_header(uint16_t *fb, const char *title, int show_logo) {
  if (!fb)
    return;
  const OImg *bar = image(IMG_BG_TITLE);
  if (bar)
    img_draw_wide(fb, bar, 0, 60);
  if (show_logo) {
    const OImg *logo = image(IMG_LOGO);
    if (logo)
      img_draw(fb, logo, 20, 30 - logo->h / 2);
  }
  if (title && *title) {
    use_font(FONT_TITLE, cfg.title.size);
    char fitted[256];
    fit_text(title, fitted, sizeof fitted, SCREEN_WIDTH - 2 * 130, 0);
    int w = font_measure_text_bold(fitted, 0);
    text_centered_y(fb, (SCREEN_WIDTH - w) / 2, 29, fitted, cfg.title.color, 0);
  }
  onion_draw_battery(fb, right_x(596), 30);
  font_face_use(-1, 0);
}

void onion_render_footer(uint16_t *fb, const char *a_label,
                         const char *b_label, int current, int total) {
  if (!fb)
    return;
  int H = SCREEN_HEIGHT;
  const OImg *bar = image(IMG_BG_FOOTER);
  if (bar)
    img_draw_wide(fb, bar, H - 60, 0);
  int x = 20, cy = H - 30;
  use_font(FONT_HINT, cfg.hint.size);
  const char *labels[2] = {a_label, b_label};
  const int buttons[2] = {IMG_BUTTON_A, IMG_BUTTON_B};
  for (int i = 0; i < 2; i++) {
    if (!labels[i])
      continue;
    const OImg *btn = image(buttons[i]);
    if (btn) {
      img_draw(fb, btn, x, cy - btn->h / 2);
      x += btn->w + 5;
    }
    if (!cfg.hide_hint_labels && labels[i][0]) {
      x += text_centered_y(fb, x, cy - 1, labels[i], cfg.hint.color, 0);
    }
    x += 30;
  }
  if (total > 0) {
    char cur_s[16], tot_s[16];
    snprintf(cur_s, sizeof cur_s, "%d/", current);
    snprintf(tot_s, sizeof tot_s, "%d", total);
    int tw = font_measure_text_bold(tot_s, 0);
    int cw = font_measure_text_bold(cur_s, 0);
    int tx = right_x(620) - tw;
    text_centered_y(fb, tx, cy - 1, tot_s, cfg.total.color, 0);
    text_centered_y(fb, tx - cw, cy - 1, cur_s, cfg.currentpage.color, 0);
  }
  font_face_use(-1, 0);
}

/* Procedural stand-in used only if neither the theme nor the fallback skin
 * provides a tile. */
static void draw_card(uint16_t *fb, int x, int y, int w, int h,
                      uint16_t color, unsigned alpha) {
  int r = 14;
  for (int py = 0; py < h; py++) {
    for (int px = 0; px < w; px++) {
      int dx = px < r ? r - px : (px >= w - r ? px - (w - r - 1) : 0);
      int dy = py < r ? r - py : (py >= h - r ? py - (h - r - 1) : 0);
      if (dx * dx + dy * dy > r * r)
        continue;
      int sx = x + px, sy = y + py;
      if (sx < 0 || sy < 0 || sx >= SCREEN_WIDTH || sy >= SCREEN_HEIGHT)
        continue;
      uint16_t *d = &fb[sy * SCREEN_WIDTH + sx];
      *d = blend565(*d, color, alpha);
    }
  }
}

/* A short message above the hint bar: dark rounded card, theme hint font. */
void onion_render_toast(uint16_t *fb, const char *text) {
  if (!fb || !text || !*text)
    return;
  use_font(FONT_HINT, cfg.hint.size * 4 / 5 > 16 ? cfg.hint.size * 4 / 5 : 16);
  char fitted[160];
  fit_text(text, fitted, sizeof fitted, SCREEN_WIDTH - 80, 0);
  int tw = font_measure_text_bold(fitted, 0);
  int w = tw + 40, h = 44;
  int x = (SCREEN_WIDTH - w) / 2, y = SCREEN_HEIGHT - 60 - 12 - h;
  draw_card(fb, x, y, w, h, 0x0000, 210);
  text_centered_y(fb, (SCREEN_WIDTH - tw) / 2, y + h / 2, fitted, 0xFFFF, 0);
  font_face_use(-1, 0);
}

void onion_render_home(uint16_t *fb, const OnionHomeItem *items, int count,
                       int selected) {
  if (!fb || !items || count <= 0)
    return;
  onion_render_background(fb);
  onion_render_header(fb, NULL, 1);

  static int first;
  if (selected < first)
    first = selected;
  if (selected >= first + 4)
    first = selected - 3;
  if (first > count - 4)
    first = count - 4;
  if (first < 0)
    first = 0;

  int ox = center_offset();
  for (int slot = 0; slot < 4 && first + slot < count; slot++) {
    int idx = first + slot;
    int active = idx == selected;
    int kind = items[idx].kind;
    if (kind < 0 || kind > ONION_HOME_EXPERT)
      kind = ONION_HOME_GAME;
    const OImg *tile = image(IMG_HOME_FIRST + kind * 2 + (active ? 1 : 0));
    int x = ox + 10 + slot * 155;
    if (tile) {
      /* MainUI centres each tile image on its 156 px slot at y = 230,
       * whatever its size (measured from theme previews: 156x278 tiles at
       * y 91, 156x328 at 66, BMO Noire's 794x500 scene layers at -20). */
      img_draw(fb, tile, x + (156 - tile->w) / 2, 230 - tile->h / 2);
    } else {
      draw_card(fb, x + 8, 110, 140, 260, active ? palette.select_bg : palette.legend_bg, 255);
    }
    if (!cfg.hide_icon_labels && items[idx].label) {
      use_font(FONT_GRID, cfg.grid.grid1x4);
      char label[96];
      fit_text(items[idx].label, label, sizeof label, 150, 0);
      int w = font_measure_text_bold(label, 0);
      text_centered_y(fb, x + 78 - w / 2, 285, label,
                      active ? cfg.grid.selectedcolor : cfg.grid.color, 0);
    }
  }

  /* One page dot per item, like MainUI. */
  const OImg *dot_a = image(IMG_DOT_ACTIVE), *dot_n = image(IMG_DOT_NEUTRAL);
  if (count > 1 && dot_a && dot_n) {
    int step = (dot_a->w > dot_n->w ? dot_a->w : dot_n->w) + 4;
    int total_w = step * count - 4;
    int x = SCREEN_WIDTH / 2 - total_w / 2;
    for (int i = 0; i < count; i++, x += step) {
      const OImg *d = i == selected ? dot_a : dot_n;
      img_draw(fb, d, x + (step - 4 - d->w) / 2, 396 - d->h / 2);
    }
  }
  onion_render_footer(fb, tr_or("onion.select", "SELECT"),
                      tr_or("onion.back", "BACK"), 0, 0);
}

void onion_render_grid(uint16_t *fb, const OnionGridItem *items, int count,
                       int selected) {
  if (!fb)
    return;
  enum { COLS = 4, ROWS = 2, PAGE = COLS * ROWS };
  if (selected < 0)
    selected = 0;
  int page = count > 0 ? selected / PAGE : 0;
  int pages = count > 0 ? (count + PAGE - 1) / PAGE : 0;
  int ox = center_offset();
  const OImg *cell_n = image(IMG_GAME_ITEM_N), *cell_f = image(IMG_GAME_ITEM_F);
  for (int slot = 0; slot < PAGE; slot++) {
    int idx = page * PAGE + slot;
    if (idx >= count)
      break;
    int active = idx == selected;
    int x = ox + 10 + (slot % COLS) * 155;
    int y = 70 + (slot / COLS) * 170;
    const OImg *cell = active ? cell_f : cell_n;
    if (cell)
      img_draw(fb, cell, x, y);
    else if (active)
      draw_card(fb, x, y, 154, 170, palette.select_bg, 255);
    const OImg *icon = console_icon(items[idx].folder, active);
    if (!icon) {
      /* TreeFrogUI icon packs cover systems the theme has no art for. */
      char path[ONION_PATH_MAX];
      if (frogui_fallback_system_icon(items[idx].folder, path, sizeof path))
        icon = icon_load(path, 120, 110);
    }
    if (icon) {
      img_draw(fb, icon, x + (150 - icon->w) / 2, y + (146 - icon->h) / 2);
    } else if (items[idx].label) {
      use_font(FONT_GRID, cfg.grid.grid1x4);
      char initials[8] = {0};
      int n = 0;
      for (const char *p = items[idx].label; *p && n < 2; p++)
        if (isalnum((unsigned char)*p))
          initials[n++] = (char)toupper((unsigned char)*p);
      int w = font_measure_text_bold(initials, 0);
      text_centered_y(fb, x + 75 - w / 2, y + 73, initials,
                      active ? cfg.grid.selectedcolor : cfg.grid.color, 0);
    }
    if (!cfg.hide_icon_labels && items[idx].label) {
      use_font(FONT_GRID, cfg.grid.grid3x4);
      char label[96];
      fit_text(items[idx].label, label, sizeof label, 146, 0);
      int w = font_measure_text_bold(label, 0);
      text_centered_y(fb, x + 77 - w / 2, y + 156, label,
                      active ? cfg.grid.selectedcolor : cfg.grid.color, 0);
    }
  }
  font_face_use(-1, 0);
  onion_render_footer(fb, tr_or("onion.open", "OPEN"),
                      tr_or("onion.back", "BACK"), pages ? page + 1 : 0, pages);
}

int onion_list_rows(int large) { return large ? 4 : 6; }

void onion_render_list(uint16_t *fb, const OnionListItem *items, int visible,
                       int selected_row, int large, int label_right_limit) {
  if (!fb || !items)
    return;
  int W = SCREEN_WIDTH;
  int item_h = large ? 90 : 60;
  int bg_h = large ? 90 : 56;
  int label_y = large ? 37 : 27;
  int padding = large ? 0 : 4;
  int rows = onion_list_rows(large);
  if (visible > rows)
    visible = rows;
  int top = 420 - rows * item_h;
  const OImg *row_bg = image(large ? IMG_BG_LIST_L : IMG_BG_LIST_S);
  const OImg *divider = image(IMG_DIVIDER);
  const OImg *toggle_on = image(IMG_TOGGLE_ON), *toggle_off = image(IMG_TOGGLE_OFF);
  const OImg *arrow_l = image(IMG_ARROW_LEFT), *arrow_r = image(IMG_ARROW_RIGHT);
  const OImg *fav = image(IMG_FAVORITE_MARK);
  const OImg *folder_icon = image(IMG_FOLDER);
  const unsigned hidden_alpha = 60;
  int limit = label_right_limit > 0 ? label_right_limit : W;

  for (int i = 0; i < visible; i++) {
    const OnionListItem *it = &items[i];
    int y = top + i * item_h;
    int active = i == selected_row;
    if (divider && !large)
      img_draw_ex(fb, divider, 0, 0, divider->w, 4 < divider->h ? 4 : divider->h,
                  0, y, divider->w >= ONION_W ? W * divider->w / ONION_W : divider->w,
                  4 < divider->h ? 4 : divider->h, 255);
    y += padding;
    if (active) {
      if (row_bg) {
        int src_h = bg_h < row_bg->h ? bg_h : row_bg->h;
        int dw = row_bg->w >= ONION_W ? W * row_bg->w / ONION_W : row_bg->w;
        img_draw_ex(fb, row_bg, 0, 0, row_bg->w, src_h, 0, y, dw, src_h, 255);
      } else {
        draw_card(fb, 6, y, W - 12, bg_h, palette.select_bg, 255);
      }
    }
    int center_y = y + bg_h / 2;
    int offset_x = 20;
    int label_end = limit;
    unsigned alpha = it->disabled ? hidden_alpha : 255;

    const OImg *icon = it->icon_path
                           ? icon_load(it->icon_path, large ? 100 : 48, large ? 84 : 48)
                           : (it->folder ? folder_icon : NULL);
    if (icon) {
      int ih = icon->h > bg_h - 8 ? bg_h - 8 : icon->h;
      int iw = icon->w * ih / icon->h;
      img_draw_ex(fb, icon, 0, 0, icon->w, icon->h, offset_x, center_y - ih / 2,
                  iw, ih, alpha);
      offset_x += iw + 17;
    }
    if (it->favourite && fav) {
      img_draw_alpha(fb, fav, offset_x, center_y - fav->h / 2, alpha);
      offset_x += fav->w + 8;
    }

    use_font(FONT_LIST, cfg.list.size);
    uint16_t color = cfg.list.color;
    if (it->disabled)
      color = blend565(palette.bg, color, hidden_alpha);
    if (it->kind == ONION_ROW_TOGGLE) {
      const OImg *t = it->toggle_on ? toggle_on : toggle_off;
      if (t) {
        int tx = right_x(620) - t->w;
        img_draw_alpha(fb, t, tx, center_y - t->h / 2, alpha);
        label_end = tx;
      } else {
        const char *v = it->toggle_on ? tr_or("value.on", "On") : tr_or("value.off", "Off");
        int vw = font_measure_text_bold(v, 1);
        text_centered_y(fb, right_x(620) - vw, y + label_y, v, color, 1);
        label_end = right_x(620) - vw - 10;
      }
    } else if (it->kind == ONION_ROW_VALUE || it->kind == ONION_ROW_INFO) {
      const int value_w = 226;
      int right_edge = right_x(640) - 20;
      int arrows = it->kind == ONION_ROW_VALUE && arrow_l && arrow_r;
      int value_left = right_edge - value_w - (arrows ? arrow_r->w + arrow_l->w : 0);
      if (arrows) {
        img_draw_alpha(fb, arrow_l, value_left, center_y - arrow_l->h / 2, alpha);
        img_draw_alpha(fb, arrow_r, right_edge - arrow_r->w,
                       center_y - arrow_r->h / 2, alpha);
      }
      if (it->value) {
        char value[128];
        fit_text(it->value, value, sizeof value, value_w, 1);
        int vw = font_measure_text_bold(value, 1);
        int vcenter = right_edge - (arrows ? arrow_r->w : 0) - value_w / 2;
        text_centered_y(fb, vcenter - vw / 2, y + label_y, value, color, 1);
      }
      label_end = value_left;
    }
    if (it->label) {
      char label[256];
      fit_text(it->label, label, sizeof label, label_end - offset_x - 10, 1);
      uint16_t label_color = color;
      if (it->kind == ONION_ROW_HEADER && !active)
        label_color = cfg.grid.selectedcolor == cfg.list.color
                          ? cfg.title.color
                          : cfg.grid.selectedcolor;
      text_centered_y(fb, offset_x, y + label_y, label, label_color, 1);
    }
  }
  font_face_use(-1, 0);
}

int onion_render_preview(uint16_t *fb, const Thumbnail *art) {
  if (!fb || !art || !art->data)
    return 0;
  const OImg *bg = image(IMG_PREVIEW_BG);
  int preview_w = 250;
  int left = right_x(640) - preview_w;
  if (bg) {
    left = right_x(640) - bg->w;
    img_draw(fb, bg, left, 60);
    preview_w = bg->w;
  }
  int w = art->width, h = art->height;
  if (w > preview_w) {
    h = h * preview_w / w;
    w = preview_w;
  }
  if (h > 340) {
    w = w * 340 / h;
    h = 340;
  }
  int x0 = left + (preview_w - w) / 2, y0 = 240 - h / 2;
  for (int y = 0; y < h; y++) {
    int py = y0 + y;
    if (py < 0 || py >= SCREEN_HEIGHT)
      continue;
    int sy = y * art->height / h;
    for (int x = 0; x < w; x++) {
      int px = x0 + x;
      if (px < 0 || px >= SCREEN_WIDTH)
        continue;
      int si = sy * art->width + x * art->width / w;
      unsigned a = art->alpha ? art->alpha[si] : 255;
      uint16_t *d = &fb[py * SCREEN_WIDTH + px];
      *d = blend565(*d, art->data[si], a);
    }
  }
  return left;
}

/* Yes/no prompt over the current screen, like Onion's theme_renderInfoPanel
 * with its A/B hint: the screen is dimmed and the lines centred. */
void onion_render_prompt(uint16_t *fb, const char *message, const char *a_label,
                         const char *b_label) {
  if (!fb || !message)
    return;
  int W = SCREEN_WIDTH, H = SCREEN_HEIGHT;
  for (int i = 0; i < W * H; i++)
    fb[i] = blend565(fb[i], 0, 0xBE);
  use_font(FONT_TITLE, cfg.title.size);
  int lines = 1;
  for (const char *p = message; *p; p++)
    lines += *p == '\n';
  int line_h = text_height() + 4;
  int y = H / 2 - (lines * line_h) / 2 + line_h / 2;
  const char *p = message;
  while (*p) {
    char line[160];
    size_t len = strcspn(p, "\n");
    snprintf(line, sizeof line, "%.*s", (int)len, p);
    int w = font_measure_text_bold(line, 0);
    text_centered_y(fb, (W - w) / 2, y, line, 0xFFFF, 0);
    y += line_h;
    p += len;
    if (*p == '\n')
      p++;
  }
  font_face_use(-1, 0);
  onion_render_footer(fb, a_label, b_label, 0, 0);
}

/* GameSwitcher bars hold artwork at both ends (logo, button legend), so a
 * wider panel repeats a plain stretch instead of scaling the art: the 4 px
 * tile nearest the middle that matches the bar's edge. Images are dithered
 * on load, so tiles keep the 4 px pattern and colours compare loosely. */
static int near565(uint16_t a, uint16_t b) {
  int dr = (a >> 11) - (b >> 11), dg = ((a >> 5) & 63) - ((b >> 5) & 63);
  int db = (a & 31) - (b & 31);
  return dr * dr <= 1 && dg * dg <= 4 && db * db <= 1;
}

static int plain_tile(const OImg *im) {
  int mid = im->w / 8 * 4;
  for (int d = 0; d < im->w / 2; d += 4) {
    for (int k = -1; k <= 1; k += 2) {
      int c = mid + d * k;
      if (c < 4 || c + 4 > im->w)
        continue;
      int same = 1;
      for (int y = 0; y < im->h && same; y++)
        for (int i = 0; i < 4 && same; i++) {
          size_t a = (size_t)y * im->w + c + i, b = (size_t)y * im->w + 4 + i;
          same = im->a[a] == im->a[b] && near565(im->px[a], im->px[b]);
        }
      if (same)
        return c;
    }
  }
  return mid;
}

static void img_draw_bar(uint16_t *fb, const OImg *im, int y) {
  if (!im || !im->px)
    return;
  int extra = SCREEN_WIDTH - im->w;
  if (extra <= 0 || im->w < 16) {
    img_draw(fb, im, 0, y);
    return;
  }
  int c = plain_tile(im);
  img_draw_ex(fb, im, 0, 0, c, im->h, 0, y, c, im->h, 255);
  for (int x = 0; x < extra; x += 4) {
    int w = extra - x < 4 ? extra - x : 4;
    img_draw_ex(fb, im, c, 0, w, im->h, c + x, y, w, im->h, 255);
  }
  img_draw_ex(fb, im, c, 0, im->w - c, im->h, c + extra, y, im->w - c,
              im->h, 255);
}

/* Onion's GameSwitcher (src/gameSwitcher/gs_render.h): a 60 px game-name bar
 * at 75% black with arrows, above the footer in the normal view and at the
 * bottom in the minimal one. The theme's extra/gs-top-bar and
 * extra/gs-bottom-bar replace the header and footer when present; a custom
 * footer is drawn as-is, so its button legend is part of the image. */
void onion_render_switcher(uint16_t *fb, int view, const char *header,
                           const char *game, int has_prev, int has_next,
                           int current, int total, int show_legend,
                           const char *a_label, const char *b_label) {
  if (!fb)
    return;
  int W = SCREEN_WIDTH, H = SCREEN_HEIGHT;
  const OImg *top = image(IMG_GS_TOP), *bottom = image(IMG_GS_BOTTOM);
  int header_h = top ? (top->h > 1 ? top->h : 0) : 60;
  int footer_h = bottom ? (bottom->h > 1 ? bottom->h : 0) : 60;

  if (view != ONION_GS_FULLSCREEN && game) {
    int bar_h = 60;
    int y0 = H - bar_h - (view == ONION_GS_NORMAL ? footer_h : 0);
    for (int y = y0 < 0 ? 0 : y0; y < y0 + bar_h && y < H; y++) {
      uint16_t *row = fb + (size_t)y * W;
      for (int x = 0; x < W; x++)
        row[x] = blend565(row[x], 0, 0xBE);
    }
    const OImg *al = image(IMG_GS_ARROW_LEFT), *ar = image(IMG_GS_ARROW_RIGHT);
    if (!al)
      al = image(IMG_ARROW_LEFT);
    if (!ar)
      ar = image(IMG_ARROW_RIGHT);
    int padding = (al ? al->w : 24) + 20;
    if (has_prev && al)
      img_draw(fb, al, 10, y0 + 30 - al->h / 2);
    if (has_next && ar)
      img_draw(fb, ar, right_x(630) - ar->w, y0 + 30 - ar->h / 2);
    use_font(FONT_TITLE, cfg.title.size);
    char fitted[256];
    fit_text(game, fitted, sizeof fitted, W - 2 * padding, 0);
    int w = font_measure_text_bold(fitted, 0);
    int x = (W - w) / 2;
    if (x < padding)
      x = padding;
    text_centered_y(fb, x, y0 + 30, fitted, 0xFFFF, 0);
    font_face_use(-1, 0);
  }

  if (view == ONION_GS_NORMAL) {
    if (bottom) {
      if (footer_h)
        img_draw_bar(fb, bottom, H - bottom->h);
    } else {
      onion_render_footer(fb, a_label, b_label, current, total);
    }
    if (top) {
      if (header_h) {
        img_draw_bar(fb, top, 0);
        if (header && *header) {
          use_font(FONT_TITLE, cfg.title.size);
          char fitted[256];
          fit_text(header, fitted, sizeof fitted, W - 2 * 130, 0);
          int w = font_measure_text_bold(fitted, 0);
          text_centered_y(fb, (W - w) / 2, header_h / 2, fitted,
                          cfg.title.color, 0);
          font_face_use(-1, 0);
        }
        onion_draw_battery(fb, right_x(596), header_h / 2);
      }
    } else {
      onion_render_header(fb, header, 0);
    }
  }

  const OImg *legend = image(IMG_GS_LEGEND);
  if (show_legend && legend && view != ONION_GS_FULLSCREEN)
    img_draw(fb, legend, W - legend->w,
             view == ONION_GS_NORMAL ? header_h : 0);
}
