#include "gamelist.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* A gamelist is looked for in the ROM's folder and up to two folders above
 * it (multi-disc games often sit in a sub-folder of the system folder). */
#define GL_LEVELS 3
#define GL_LISTS 6          /* parsed lists kept in memory */
#define GL_DIRS 24          /* folder -> list lookups remembered */
#define GL_RECHECK_MS 3000  /* how often a folder's gamelist is stat()ed */
#define GL_MAX_FILE (48u << 20)

static const char *const gl_files[] = {"gamelist.xml", "miyoogamelist.xml", NULL};

typedef struct {
  uint32_t key, name, image, thumb; /* offsets into pool; 0 = none */
} GlEntry;

typedef struct {
  char dir[512];   /* folder holding the gamelist */
  char file[576];  /* its path */
  time_t mtime;
  off_t size;
  unsigned used;   /* LRU clock */
  int n;
  GlEntry *e;
  char *pool;
} GlList;

typedef struct {
  char dir[512];   /* a ROM folder */
  int list;        /* index into lists, -1 = none above it */
  long long checked;
  unsigned gen;    /* lists generation it was resolved against */
} GlDir;

static GlList lists[GL_LISTS];
static GlDir dirs[GL_DIRS];
static unsigned clock_n, gen = 1;
static int next_dir;

static long long now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void list_free(GlList *l) {
  free(l->e);
  free(l->pool);
  memset(l, 0, sizeof *l);
}

void gamelist_forget(void) {
  for (int i = 0; i < GL_LISTS; i++)
    list_free(&lists[i]);
  memset(dirs, 0, sizeof dirs);
  gen++;
}

/* ---- parsing ------------------------------------------------------------- */

typedef struct {
  char *buf;
  size_t len, cap;
} Pool;

static uint32_t pool_add(Pool *p, const char *s, size_t n) {
  if (p->len + n + 1 > p->cap) {
    size_t cap = p->cap ? p->cap * 2 : 65536;
    while (cap < p->len + n + 1)
      cap *= 2;
    char *b = realloc(p->buf, cap);
    if (!b)
      return 0;
    p->buf = b;
    p->cap = cap;
  }
  uint32_t at = (uint32_t)p->len;
  memcpy(p->buf + at, s, n);
  p->buf[at + n] = '\0';
  p->len += n + 1;
  return at;
}

/* Decode XML entities in place; returns the new length. */
static size_t xml_unescape(char *s, size_t n) {
  size_t w = 0;
  for (size_t r = 0; r < n;) {
    if (s[r] != '&') {
      s[w++] = s[r++];
      continue;
    }
    const char *semi = memchr(s + r, ';', n - r < 12 ? n - r : 12);
    if (!semi) {
      s[w++] = s[r++];
      continue;
    }
    size_t elen = (size_t)(semi - (s + r)) + 1;
    unsigned long cp = 0;
    if (!strncmp(s + r, "&amp;", 5)) cp = '&';
    else if (!strncmp(s + r, "&lt;", 4)) cp = '<';
    else if (!strncmp(s + r, "&gt;", 4)) cp = '>';
    else if (!strncmp(s + r, "&quot;", 6)) cp = '"';
    else if (!strncmp(s + r, "&apos;", 6)) cp = '\'';
    else if (s[r + 1] == '#')
      cp = s[r + 2] == 'x' || s[r + 2] == 'X' ? strtoul(s + r + 3, NULL, 16)
                                              : strtoul(s + r + 2, NULL, 10);
    if (!cp) {
      s[w++] = s[r++];
      continue;
    }
    /* UTF-8 encode (the decoded form is never longer than the entity). */
    if (cp < 0x80) s[w++] = (char)cp;
    else if (cp < 0x800) {
      s[w++] = (char)(0xC0 | (cp >> 6));
      s[w++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      s[w++] = (char)(0xE0 | (cp >> 12));
      s[w++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      s[w++] = (char)(0x80 | (cp & 0x3F));
    } else {
      s[w++] = (char)(0xF0 | (cp >> 18));
      s[w++] = (char)(0x80 | ((cp >> 12) & 0x3F));
      s[w++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      s[w++] = (char)(0x80 | (cp & 0x3F));
    }
    r += elen;
  }
  return w;
}

/* The text of <tag>...</tag> inside [from, to), trimmed and unescaped, added
 * to the pool. 0 when absent or empty. */
static uint32_t tag_text(Pool *p, char *from, char *to, const char *tag) {
  char open[24], close[24];
  snprintf(open, sizeof open, "<%s>", tag);
  snprintf(close, sizeof close, "</%s>", tag);
  size_t ol = strlen(open);
  for (char *s = from; s < to; s++) {
    s = memchr(s, '<', (size_t)(to - s));
    if (!s)
      return 0;
    if ((size_t)(to - s) < ol || strncmp(s, open, ol))
      continue;
    char *v = s + ol, *end = v;
    size_t cl = strlen(close);
    while (end + cl <= to && strncmp(end, close, cl))
      end++;
    if (end + cl > to)
      return 0;
    while (v < end && (*v == ' ' || *v == '\t' || *v == '\r' || *v == '\n'))
      v++;
    while (end > v && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
      end--;
    if (end == v)
      return 0;
    if (end - v >= 9 && !strncmp(v, "<![CDATA[", 9)) {
      char *c = strstr(v, "]]>");
      if (c && c < end) {
        v += 9;
        end = c;
      }
    }
    size_t n = xml_unescape(v, (size_t)(end - v));
    return pool_add(p, v, n);
  }
  return 0;
}

/* A <path> made relative to the gamelist's folder: "./a/b.sfc" -> "a/b.sfc".
 * Absolute paths (written on another machine) keep what follows the
 * folder's own name, or just the file name. */
static const char *relative_key(const char *path, const char *dir) {
  while (path[0] == '.' && path[1] == '/')
    path += 2;
  if (path[0] != '/')
    return path;
  size_t dl = strlen(dir);
  if (!strncmp(path, dir, dl) && path[dl] == '/')
    return path + dl + 1;
  const char *base = strrchr(dir, '/');
  if (base) {
    char probe[160];
    snprintf(probe, sizeof probe, "%s/", base);
    const char *at = strstr(path, probe);
    if (at)
      return at + strlen(probe);
  }
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

static const char *sort_pool;
static int entry_cmp(const void *a, const void *b) {
  return strcasecmp(sort_pool + ((const GlEntry *)a)->key,
                    sort_pool + ((const GlEntry *)b)->key);
}

static int list_parse(GlList *l) {
  FILE *f = fopen(l->file, "rb");
  if (!f)
    return 0;
  char *xml = NULL;
  if (l->size > 0 && (size_t)l->size < GL_MAX_FILE)
    xml = malloc((size_t)l->size + 1);
  size_t len = xml ? fread(xml, 1, (size_t)l->size, f) : 0;
  fclose(f);
  if (!xml)
    return 0;
  xml[len] = '\0';

  Pool pool = {0};
  pool_add(&pool, "", 0); /* offset 0 = none */
  int cap = 256, n = 0;
  GlEntry *e = malloc((size_t)cap * sizeof *e);
  char *end = xml + len;
  for (char *g = strstr(xml, "<game"); e && g; g = strstr(g, "<game")) {
    char *open_end = strchr(g, '>');
    if (!open_end)
      break;
    if (g[5] != '>' && g[5] != ' ' && g[5] != '\t' && g[5] != '\n' && g[5] != '\r') {
      g += 5; /* <gameList> */
      continue;
    }
    char *close = strstr(open_end, "</game>");
    if (!close)
      close = end;
    uint32_t path = tag_text(&pool, open_end, close, "path");
    if (path) {
      const char *key = relative_key(pool.buf + path, l->dir);
      uint32_t k = pool_add(&pool, key, strlen(key));
      if (n == cap) {
        cap *= 2;
        GlEntry *grown = realloc(e, (size_t)cap * sizeof *e);
        if (!grown)
          break;
        e = grown;
      }
      e[n].key = k;
      e[n].name = tag_text(&pool, open_end, close, "name");
      e[n].image = tag_text(&pool, open_end, close, "image");
      e[n].thumb = tag_text(&pool, open_end, close, "thumbnail");
      n++;
    }
    g = close;
  }
  free(xml);
  if (!e || !pool.buf) {
    free(e);
    free(pool.buf);
    return 0;
  }
  sort_pool = pool.buf;
  qsort(e, (size_t)n, sizeof *e, entry_cmp);
  l->e = e;
  l->n = n;
  l->pool = pool.buf;
  return 1;
}

/* ---- lookup -------------------------------------------------------------- */

/* The loaded list for the gamelist file in dir, loading or reloading it. */
static int list_for_dir(const char *dir) {
  for (int k = 0; gl_files[k]; k++) {
    char file[576];
    struct stat st;
    snprintf(file, sizeof file, "%s/%s", dir, gl_files[k]);
    if (stat(file, &st) != 0 || !S_ISREG(st.st_mode))
      continue;
    int slot = -1, oldest = 0;
    for (int i = 0; i < GL_LISTS; i++) {
      if (lists[i].pool && !strcmp(lists[i].file, file)) {
        if (lists[i].mtime == st.st_mtime && lists[i].size == st.st_size) {
          lists[i].used = ++clock_n;
          return i;
        }
        slot = i; /* changed on the card: read it again */
        break;
      }
      if (lists[i].used < lists[oldest].used)
        oldest = i;
    }
    if (slot < 0)
      slot = oldest;
    list_free(&lists[slot]);
    gen++;
    GlList *l = &lists[slot];
    snprintf(l->dir, sizeof l->dir, "%s", dir);
    snprintf(l->file, sizeof l->file, "%s", file);
    l->mtime = st.st_mtime;
    l->size = st.st_size;
    if (!list_parse(l)) {
      list_free(l);
      return -1;
    }
    l->used = ++clock_n;
    return slot;
  }
  return -1;
}

/* The entry for rom_path, or NULL. *list_out gets its list. */
static const GlEntry *find(const char *rom_path, const GlList **list_out) {
  if (!rom_path || rom_path[0] != '/')
    return NULL;
  char dir[512];
  snprintf(dir, sizeof dir, "%s", rom_path);
  char *slash = strrchr(dir, '/');
  if (!slash || slash == dir)
    return NULL;
  *slash = '\0';

  /* Which list covers this folder: remembered, re-checked now and then. */
  long long now = now_ms();
  GlDir *d = NULL;
  for (int i = 0; i < GL_DIRS; i++)
    if (dirs[i].dir[0] && !strcmp(dirs[i].dir, dir)) {
      d = &dirs[i];
      break;
    }
  if (!d) {
    d = &dirs[next_dir];
    next_dir = (next_dir + 1) % GL_DIRS;
    snprintf(d->dir, sizeof d->dir, "%s", dir);
    d->checked = 0;
  }
  if (!d->checked || now - d->checked > GL_RECHECK_MS || d->gen != gen) {
    char probe[512];
    snprintf(probe, sizeof probe, "%s", dir);
    d->list = -1;
    for (int level = 0; level < GL_LEVELS && probe[0]; level++) {
      int li = list_for_dir(probe);
      if (li >= 0) {
        d->list = li;
        break;
      }
      char *up = strrchr(probe, '/');
      if (!up || up == probe)
        break;
      *up = '\0';
      if (!strcmp(probe, "/mnt/sdcard") || !strcmp(probe, "/mnt"))
        break;
    }
    d->checked = now;
    d->gen = gen;
  }
  if (d->list < 0 || !lists[d->list].pool)
    return NULL;
  const GlList *l = &lists[d->list];
  size_t dl = strlen(l->dir);
  if (strncmp(rom_path, l->dir, dl) || rom_path[dl] != '/')
    return NULL;
  const char *key = rom_path + dl + 1;
  int lo = 0, hi = l->n - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    int c = strcasecmp(key, l->pool + l->e[mid].key);
    if (!c) {
      *list_out = l;
      return &l->e[mid];
    }
    if (c < 0)
      hi = mid - 1;
    else
      lo = mid + 1;
  }
  return NULL;
}

const char *gamelist_name(const char *rom_path) {
  const GlList *l = NULL;
  const GlEntry *e = find(rom_path, &l);
  return e && e->name ? l->pool + e->name : NULL;
}

/* An image path from the gamelist as a file on this card. */
static int resolve_image(const GlList *l, const char *img, char *out, size_t size) {
  if (!img || !img[0])
    return 0;
  if (img[0] == '/') {
    if (access(img, R_OK) == 0) {
      snprintf(out, size, "%s", img);
      return 1;
    }
    /* Written elsewhere (Recalbox, a PC): keep what follows "/media/". */
    const char *m = strstr(img, "/media/");
    if (m) {
      snprintf(out, size, "%s%s", l->dir, m);
      if (access(out, R_OK) == 0)
        return 1;
    }
    const char *base = strrchr(img, '/');
    snprintf(out, size, "%s/%s", l->dir, base + 1);
    return access(out, R_OK) == 0;
  }
  while (img[0] == '.' && img[1] == '/')
    img += 2;
  snprintf(out, size, "%s/%s", l->dir, img);
  return access(out, R_OK) == 0;
}

int gamelist_image(const char *rom_path, char *out, size_t size) {
  const GlList *l = NULL;
  const GlEntry *e = find(rom_path, &l);
  if (!e)
    return 0;
  if (e->image && resolve_image(l, l->pool + e->image, out, size))
    return 1;
  return e->thumb && resolve_image(l, l->pool + e->thumb, out, size);
}
