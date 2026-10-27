/*
 * shallots_setup: put the console's own display/audio driver where ShallotsOS
 * loads it, on the first boot of a full ShallotsOS install.
 *
 * ShallotsOS doesn't ship the stock drivers TreeFrogUI ships as
 * cubegm/driver_<device>.so. Each console already has its own:
 *   - R36SX, SF3000, GB350: the stock cubegm/driver.so on the card is a plain
 *     ELF, byte for byte the file TreeFrogUI ships;
 *   - SF3500, SF3000 HD, SF3100: the card's copy is encrypted, and the
 *     firmware decrypts it into /tmp/cubegm/driver.so at boot.
 * For R36SX it also makes driver_r36sx27.so, TreeFrogUI's variant for v2.7
 * kernels, from driver_r36sx.so with TreeFrogUI's 8-byte change (one routine
 * made to return 0), only when the source is the exact known driver.
 *
 * Usage: shallots_setup <driver file>...  (driver_r36sx.so, driver_sf3000.so,
 *        driver_sf3500.so, driver_gb350.so; the one /tmp/tfdevice.env names)
 * Existing files are never replaced. Writes are atomic (temp + rename).
 * Exit status: 0 when every named driver is in place afterwards.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef SD
#define SD "/mnt/sdcard"
#endif
#ifndef TMPCUBE
#define TMPCUBE "/tmp/cubegm"
#endif
#define CUBEGM SD "/cubegm"
#define LOG_FILE SD "/cubegm/logs/shallots_setup.log"
#define MAX_DRIVER (4u << 20)

static void logf_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void logf_(const char *fmt, ...) {
	mkdir(SD "/cubegm/logs", 0777);
	FILE *f = fopen(LOG_FILE, "a");
	if (!f)
		return;
	time_t t = time(NULL);
	char ts[32];
	strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&t));
	fprintf(f, "%s ", ts);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fclose(f);
}

/* ---- MD5 ------------------------------------------------------------------ */
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
static void md5_hex(const unsigned char *p, size_t n, char out[33]) {
	Md5 m = {{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476}, 0, {0}, 0};
	md5_add(&m, p, n);
	uint64_t bits = m.n * 8;
	unsigned char pad = 0x80, z = 0, len[8];
	md5_add(&m, &pad, 1);
	while (m.bl != 56) md5_add(&m, &z, 1);
	for (int i = 0; i < 8; i++) len[i] = (unsigned char)(bits >> (8*i));
	md5_add(&m, len, 8);
	for (int i = 0; i < 16; i++)
		sprintf(out + i*2, "%02x", (m.h[i/4] >> (8*(i%4))) & 255);
}

/* ---- Files ---------------------------------------------------------------- */
/* The whole file (up to MAX_DRIVER), or NULL. */
static unsigned char *read_file(const char *path, size_t *len) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	unsigned char *buf = malloc(MAX_DRIVER);
	size_t n = buf ? fread(buf, 1, MAX_DRIVER, f) : 0;
	int more = buf && fgetc(f) != EOF;
	fclose(f);
	if (!buf || !n || more) {
		free(buf);
		return NULL;
	}
	*len = n;
	return buf;
}

static int is_elf(const unsigned char *p, size_t n) {
	return n > 52 && !memcmp(p, "\177ELF", 4);
}

static int exists(const char *path) {
	struct stat st;
	return stat(path, &st) == 0 && st.st_size > 0;
}

/* Writes path atomically; never replaces an existing file. */
static int write_new(const char *path, const unsigned char *p, size_t n) {
	if (exists(path))
		return 0;
	char tmp[512];
	snprintf(tmp, sizeof tmp, "%s.setup-new", path);
	int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return -1;
	size_t off = 0;
	while (off < n) {
		ssize_t w = write(fd, p + off, n - off);
		if (w <= 0) {
			close(fd);
			unlink(tmp);
			return -1;
		}
		off += (size_t)w;
	}
	if (fsync(fd) != 0 || close(fd) != 0 || rename(tmp, path) != 0) {
		unlink(tmp);
		return -1;
	}
	sync();
	return 0;
}

/* ---- Known drivers --------------------------------------------------------- */
#define MD5_R36SX "f19ff1cebbb57a9a657084e1ce27b02c"    /* = SF3500 card driver */
#define MD5_R36SX27 "b8655fbf58c4e61fc705b2ff77c0d8bd"
#define MD5_SF3500 "ee629cce4710d501a7feb682e2850def"   /* SF3500, decrypted */
#define MD5_SF3000 "83ceee0dfc442eb684147eebd33024c4"
#define MD5_GB350 "19377667bf2af1c55845f85290e1a85a"

/* TreeFrogUI's driver_r36sx27.so: the routine at 0x8c10 returns 0 at once
 * ("the crashing 2D engine stubbed out" for v2.7-class kernels). */
#define R36SX27_AT 0x8c10
static const unsigned char r36sx27_old[8] = {0x02, 0x00, 0x1c, 0x3c, 0x40, 0x27, 0x9c, 0x27};
static const unsigned char r36sx27_new[8] = {0x08, 0x00, 0xe0, 0x03, 0x25, 0x10, 0x00, 0x00};

static const char *known(const char *md5) {
	if (!strcmp(md5, MD5_R36SX)) return "TreeFrogUI driver_r36sx.so";
	if (!strcmp(md5, MD5_R36SX27)) return "TreeFrogUI driver_r36sx27.so";
	if (!strcmp(md5, MD5_SF3500)) return "TreeFrogUI driver_sf3500.so";
	if (!strcmp(md5, MD5_SF3000)) return "TreeFrogUI driver_sf3000.so";
	if (!strcmp(md5, MD5_GB350)) return "TreeFrogUI driver_gb350.so";
	return "not a known build: ShallotsOS may need updating for it";
}

/* Copy the first plain-ELF source to dst. A source with a list in only[]
 * (NULL-terminated MD5s) is used only when it is one of those builds. */
typedef struct { const char *path; const char *const *only; } Source;
static int allowed(const char *const *only, const char *md5) {
	if (!only)
		return 1;
	for (; *only; only++)
		if (!strcmp(*only, md5))
			return 1;
	return 0;
}
static int derive(const char *dst, const Source *srcs, int nsrc) {
	if (exists(dst))
		return 0;
	for (int i = 0; i < nsrc; i++) {
		size_t n;
		unsigned char *p = read_file(srcs[i].path, &n);
		if (!p)
			continue;
		if (!is_elf(p, n)) {
			logf_("%s: not a plain ELF (encrypted), skipped", srcs[i].path);
			free(p);
			continue;
		}
		char md5[33];
		md5_hex(p, n, md5);
		if (!allowed(srcs[i].only, md5)) {
			logf_("%s: md5 %s is not a build that can stand in for %s, skipped", srcs[i].path,
			      md5, dst);
			free(p);
			continue;
		}
		int rc = write_new(dst, p, n);
		logf_("%s <- %s (%zu bytes, md5 %s, %s): %s", dst, srcs[i].path, n, md5, known(md5),
		      rc ? strerror(errno) : "ok");
		free(p);
		return rc;
	}
	logf_("%s: no usable source", dst);
	return -1;
}

static int derive_r36sx27(void) {
	const char *dst = CUBEGM "/driver_r36sx27.so";
	if (exists(dst))
		return 0;
	size_t n;
	unsigned char *p = read_file(CUBEGM "/driver_r36sx.so", &n);
	if (!p)
		return -1;
	char md5[33];
	md5_hex(p, n, md5);
	int rc = -1;
	if (strcmp(md5, MD5_R36SX) != 0 || n < R36SX27_AT + 8 ||
	    memcmp(p + R36SX27_AT, r36sx27_old, 8) != 0) {
		logf_("%s: driver_r36sx.so is not the known build (md5 %s); v2.7 variant not made", dst, md5);
	} else {
		memcpy(p + R36SX27_AT, r36sx27_new, 8);
		md5_hex(p, n, md5);
		if (strcmp(md5, MD5_R36SX27) != 0)
			logf_("%s: patched md5 %s does not match; not written", dst, md5);
		else {
			rc = write_new(dst, p, n);
			logf_("%s: made from driver_r36sx.so: %s", dst, rc ? strerror(errno) : "ok");
		}
	}
	free(p);
	return rc;
}

/* Sources for each driver, best first. driver_r36sx.so is the plain driver
 * on R36SX (and SF3500 backup) cards; the SF3500 firmware's decrypted copy
 * differs from it only in where it looks for a helper script. */
static int derive_named(const char *name) {
	/* The SF3500 build, plain on the card (R36SX, SF3500 backups) or as the
	 * SF3500 firmware decrypts it (it differs only in where it looks for a
	 * helper script). A card's own driver is otherwise its device's. */
	static const char *const sf3500_builds[] = {MD5_R36SX, MD5_SF3500, NULL};
	const char *card = CUBEGM "/driver.so", *dec = TMPCUBE "/driver.so";
	char dst[256];
	snprintf(dst, sizeof dst, CUBEGM "/%s", name);
	if (!strcmp(name, "driver_r36sx.so")) {
		const Source s[] = {{card, NULL}, {dec, sf3500_builds}};
		int rc = derive(dst, s, 2);
		if (rc == 0)
			derive_r36sx27();   /* optional: only the SIGBUS fallback uses it */
		return rc;
	}
	if (!strcmp(name, "driver_r36sx27.so"))
		return derive_r36sx27();
	if (!strcmp(name, "driver_sf3500.so")) {
		const Source s[] = {{dec, NULL}, {card, sf3500_builds}};
		return derive(dst, s, 2);
	}
	if (!strcmp(name, "driver_sf3000.so") || !strcmp(name, "driver_gb350.so")) {
		const Source s[] = {{card, NULL}};
		return derive(dst, s, 1);
	}
	logf_("unknown driver '%s'", name);
	return -1;
}

int main(int argc, char **argv) {
	int rc = 0;
	for (int i = 1; i < argc; i++)
		if (derive_named(argv[i]) != 0)
			rc = 1;
	return argc > 1 ? rc : 2;
}
