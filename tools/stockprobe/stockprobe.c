/* stockprobe: a pass-through libretro core that measures its frontend.
 *
 * It takes the place of a stock core file (cubegm/cores/libemu_sfc.so, ...),
 * loads the real core from the "real" folder next to it and forwards every
 * call, timing each one. It logs to /mnt/sdcard/logs/stockprobe.log:
 *   - once: which frontend loaded it, the checksums of the frontend and the
 *     real core (to compare with the public firmware backups), the scheduling
 *     it runs with, the libraries the frontend has loaded, the options and
 *     environment calls the frontend answers, and the stock settings calls;
 *   - every 5 s of play: retro_run rate and time, frames drawn vs skipped,
 *     time spent in the frontend's video and audio callbacks, CPU use of this
 *     process's threads and of the busiest other processes, and a CPU speed
 *     loop.
 * The same file runs under the stock firmware and under ShallotsOS's picoarch,
 * so both logs compare like for like. Nothing of the real core is copied. */
#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "libretro.h"

#define LOG_DIR "/mnt/sdcard/logs"
#define LOG_PATH LOG_DIR "/stockprobe.log"
#define WINDOW_US 5000000LL

static FILE *logf;
static void *real;
static char self_path[PATH_MAX], real_path[PATH_MAX];

static void plog(const char *fmt, ...) {
	va_list ap;
	if (!logf) {
		const char *path = getenv("STOCKPROBE_LOG");   /* for tests off the device */
		if (!path) { mkdir(LOG_DIR, 0777); path = LOG_PATH; }
		logf = fopen(path, "a");
		if (!logf) return;
	}
	va_start(ap, fmt);
	vfprintf(logf, fmt, ap);
	va_end(ap);
}

static void plog_flush(void) {
	if (logf) { fflush(logf); fsync(fileno(logf)); }
}

static long long now_us(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (long long)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}

/* ---- once: identity ------------------------------------------------------ */

static uint32_t crc_table[256];

static uint32_t file_crc32(const char *path, long long *size) {
	static unsigned char buf[65536];
	uint32_t c = 0xFFFFFFFFu;
	size_t n;
	FILE *f = fopen(path, "rb");
	*size = -1;
	if (!crc_table[1]) {
		for (uint32_t i = 0; i < 256; i++) {
			uint32_t r = i;
			for (int k = 0; k < 8; k++) r = (r & 1) ? 0xEDB88320u ^ (r >> 1) : r >> 1;
			crc_table[i] = r;
		}
	}
	if (!f) return 0;
	*size = 0;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
		for (size_t i = 0; i < n; i++) c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
		*size += n;
	}
	fclose(f);
	return c ^ 0xFFFFFFFFu;
}

static void log_file_id(const char *label, const char *path) {
	long long size;
	uint32_t crc = file_crc32(path, &size);
	if (size < 0) plog("  %-10s %s: missing\n", label, path);
	else plog("  %-10s %s: %lld bytes, CRC32 %08X\n", label, path, size, crc);
}

static void log_text_file(const char *label, const char *path, int max_lines, const char *filter) {
	char line[256];
	int n = 0;
	FILE *f = fopen(path, "r");
	if (!f) return;
	while (n < max_lines && fgets(line, sizeof line, f)) {
		if (filter && !strstr(line, filter)) continue;
		line[strcspn(line, "\n")] = '\0';
		plog("  %s: %s\n", label, line);
		n++;
	}
	fclose(f);
}

/* A fixed loop: its time only changes with the CPU clock. */
static long cpu_loop_us(void) {
	long long a = now_us();
	unsigned n = 1000000;
	__asm__ volatile("1: addiu %0, %0, -1\n\tbnez %0, 1b\n\tnop\n" : "+r"(n));
	return (long)(now_us() - a);
}

static void log_identity(void) {
	char exe[PATH_MAX] = "";
	char cmd[256] = "";
	ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
	FILE *f;
	if (n > 0) exe[n] = '\0';
	if ((f = fopen("/proc/self/cmdline", "r"))) {
		size_t r = fread(cmd, 1, sizeof cmd - 1, f);
		for (size_t i = 0; i + 1 < r; i++) if (!cmd[i]) cmd[i] = ' ';
		cmd[r] = '\0';
		fclose(f);
	}
	time_t t = time(NULL);
	plog("\n==== stockprobe %s  pid %d  %s", self_path, (int)getpid(), ctime(&t));
	plog("  frontend   %s (%s)\n", exe, cmd);
	log_file_id("frontend", exe);
	log_file_id("rkgame", "/mnt/sdcard/cubegm/rkgame");
	log_file_id("core", real_path);
	log_file_id("config", "/mnt/sdcard/cubegm/cores/config.xml");
	log_text_file("kernel", "/proc/version", 1, NULL);
	log_text_file("os", "/etc/os-release", 6, NULL);
	log_text_file("cpu", "/proc/cpuinfo", 4, "BogoMIPS");
	log_text_file("cpu", "/proc/cpuinfo", 1, "cpu model");

	struct sched_param sp;
	int pol = sched_getscheduler(0);
	sched_getparam(0, &sp);
	cpu_set_t set;
	CPU_ZERO(&set);
	sched_getaffinity(0, sizeof set, &set);
	char aff[64] = "";
	for (int i = 0; i < 16 && i < (int)sizeof aff - 1; i++) aff[i] = CPU_ISSET(i, &set) ? '1' : '0';
	plog("  sched      policy=%d prio=%d nice=%d affinity=%s cpus=%ld\n", pol, sp.sched_priority,
	     getpriority(PRIO_PROCESS, 0), aff, sysconf(_SC_NPROCESSORS_ONLN));
	plog("  loop       %ldus\n", cpu_loop_us());

	/* Libraries the frontend has loaded: what it draws and plays sound with. */
	if ((f = fopen("/proc/self/maps", "r"))) {
		char line[512], seen[8192] = "";
		while (fgets(line, sizeof line, f)) {
			char *p = strchr(line, '/');
			if (!p) continue;
			p[strcspn(p, "\n")] = '\0';
			if (!strstr(p, ".so") || strstr(seen, p)) continue;
			if (strlen(seen) + strlen(p) + 2 >= sizeof seen) break;
			strcat(seen, p);
			strcat(seen, "\n");
			plog("  lib        %s\n", p);
		}
		fclose(f);
	}
	/* Device nodes: a clock or performance driver would show up here. */
	DIR *d = opendir("/dev");
	if (d) {
		struct dirent *e;
		plog("  dev       ");
		while ((e = readdir(d))) {
			if (e->d_name[0] == '.' || !strncmp(e->d_name, "tty", 3) || !strncmp(e->d_name, "pty", 3)) continue;
			plog(" %s", e->d_name);
		}
		plog("\n");
		closedir(d);
	}
	plog_flush();
}

/* ---- the real core ---------------------------------------------------------- */

#define REAL_FNS(X) \
	X(void, retro_set_environment, (retro_environment_t)) \
	X(void, retro_set_video_refresh, (retro_video_refresh_t)) \
	X(void, retro_set_audio_sample, (retro_audio_sample_t)) \
	X(void, retro_set_audio_sample_batch, (retro_audio_sample_batch_t)) \
	X(void, retro_set_input_poll, (retro_input_poll_t)) \
	X(void, retro_set_input_state, (retro_input_state_t)) \
	X(void, retro_init, (void)) \
	X(void, retro_deinit, (void)) \
	X(unsigned, retro_api_version, (void)) \
	X(void, retro_get_system_info, (struct retro_system_info *)) \
	X(void, retro_get_system_av_info, (struct retro_system_av_info *)) \
	X(void, retro_set_controller_port_device, (unsigned, unsigned)) \
	X(void, retro_reset, (void)) \
	X(void, retro_run, (void)) \
	X(size_t, retro_serialize_size, (void)) \
	X(bool, retro_serialize, (void *, size_t)) \
	X(bool, retro_unserialize, (const void *, size_t)) \
	X(void, retro_cheat_reset, (void)) \
	X(void, retro_cheat_set, (unsigned, bool, const char *)) \
	X(bool, retro_load_game, (const struct retro_game_info *)) \
	X(bool, retro_load_game_special, (unsigned, const struct retro_game_info *, size_t)) \
	X(void, retro_unload_game, (void)) \
	X(unsigned, retro_get_region, (void)) \
	X(void *, retro_get_memory_data, (unsigned)) \
	X(size_t, retro_get_memory_size, (unsigned)) \
	X(void, SetFrameSkip, (int)) \
	X(void, init_sfc_setting, (void))

#define DECL(ret, name, args) static ret (*r_##name) args;
REAL_FNS(DECL)

__attribute__((constructor)) static void probe_load(void) {
	Dl_info info;
	if (!dladdr((void *)probe_load, &info) || !info.dli_fname) return;
	snprintf(self_path, sizeof self_path, "%s", info.dli_fname);
	const char *slash = strrchr(self_path, '/');
	const char *base = slash ? slash + 1 : self_path;
	int dirlen = slash ? (int)(slash - self_path) : 0;
	snprintf(real_path, sizeof real_path, "%.*s%sreal/%s", dirlen, self_path, slash ? "/" : "", base);

	int flags = RTLD_NOW | RTLD_LOCAL;
#ifdef RTLD_DEEPBIND
	flags |= RTLD_DEEPBIND;
#endif
	real = dlopen(real_path, flags);
	log_identity();
	if (!real) {
		plog("  ERROR      cannot load the real core: %s\n", dlerror());
		plog_flush();
		return;
	}
#define LOOK(ret, name, args) r_##name = (ret (*) args)dlsym(real, #name);
	REAL_FNS(LOOK)
}

/* ---- callbacks: what the frontend does with each frame ------------------------ */

static retro_environment_t f_env;
static retro_video_refresh_t f_video;
static retro_audio_sample_t f_sample;
static retro_audio_sample_batch_t f_batch;
static retro_input_poll_t f_poll;
static retro_input_state_t f_state;

static struct {
	long long start, last_run, cpu_total, cpu_busy, cpu_self;
	long long run_sum, run_max, gap_max, video_sum, video_max, audio_sum, audio_max;
	unsigned runs, drawn, dupes, gaps_over_20ms, polls, env_calls;
	unsigned skip_on, skip_off, dupe_run, dupe_run_max;
	unsigned long audio_frames, single_samples;
	char trace[400];   /* the window's first frames: run ms/gap ms, "s" = not drawn */
	int trace_len, trace_n;
	unsigned w, h;
	size_t pitch;
	int in_run;
} st;

static int env_seen[128];

static bool env_hook(unsigned cmd, void *data) {
	bool ok = f_env ? f_env(cmd, data) : false;
	unsigned base = cmd & 0xFFFF;
	if (st.in_run) st.env_calls++;
	if (base == RETRO_ENVIRONMENT_GET_VARIABLE && data) {
		struct retro_variable *v = data;
		/* Every option once, with the value the frontend gives. */
		static char seen[4096];
		if (v->key && !strstr(seen, v->key) && strlen(seen) + strlen(v->key) + 2 < sizeof seen) {
			strcat(seen, v->key);
			strcat(seen, ";");
			plog("  option     %s = %s\n", v->key, ok && v->value ? v->value : "(none)");
		}
		return ok;
	}
	if (base < 128 && env_seen[base]++) return ok;
	switch (base) {
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		plog("  env        SET_PIXEL_FORMAT %u -> %d\n", data ? *(unsigned *)data : 0, ok);
		break;
	case RETRO_ENVIRONMENT_GET_CAN_DUPE:
		plog("  env        GET_CAN_DUPE -> %d (%d)\n", ok, ok && data ? *(bool *)data : 0);
		break;
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
		plog("  env        cmd %u -> %d %s\n", cmd, ok, ok && data && *(const char **)data ? *(const char **)data : "");
		break;
	default:
		plog("  env        cmd %u (0x%x) -> %d\n", base, cmd, ok);
	}
	return ok;
}

static void video_hook(const void *data, unsigned w, unsigned h, size_t pitch) {
	long long t = now_us();
	if (data) { st.drawn++; st.w = w; st.h = h; st.pitch = pitch; st.dupe_run = 0; }
	else {
		st.dupes++;
		if (++st.dupe_run > st.dupe_run_max) st.dupe_run_max = st.dupe_run;
	}
	if (f_video) f_video(data, w, h, pitch);
	t = now_us() - t;
	st.video_sum += t;
	if (t > st.video_max) st.video_max = t;
}

static size_t batch_hook(const int16_t *data, size_t frames) {
	long long t = now_us();
	size_t r = f_batch ? f_batch(data, frames) : frames;
	t = now_us() - t;
	st.audio_frames += frames;
	st.audio_sum += t;
	if (t > st.audio_max) st.audio_max = t;
	return r;
}

static void sample_hook(int16_t l, int16_t r) {
	st.single_samples++;
	if (f_sample) f_sample(l, r);
}

static void poll_hook(void) {
	st.polls++;
	if (f_poll) f_poll();
}

static int16_t state_hook(unsigned port, unsigned dev, unsigned idx, unsigned id) {
	return f_state ? f_state(port, dev, idx, id) : 0;
}

/* ---- CPU use ------------------------------------------------------------------- */

static long long proc_ticks(const char *path, char *comm, size_t commlen) {
	char buf[512];
	FILE *f = fopen(path, "r");
	if (!f) return -1;
	size_t r = fread(buf, 1, sizeof buf - 1, f);
	fclose(f);
	buf[r] = '\0';
	char *l = strchr(buf, '('), *rp = strrchr(buf, ')');
	if (!l || !rp) return -1;
	if (comm) snprintf(comm, commlen, "%.*s", (int)(rp - l - 1), l + 1);
	unsigned long long ut = 0, stt = 0;
	/* After ")": state, then fields 4..; utime and stime are fields 14, 15. */
	if (sscanf(rp + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu", &ut, &stt) != 2) return -1;
	return (long long)(ut + stt);
}

#define MAX_TRACK 256
struct tracked { int id; long long ticks; char comm[24]; };
static struct tracked prev_procs[MAX_TRACK], prev_threads[MAX_TRACK];
static int n_prev_procs, n_prev_threads;

/* Ticks used since the last call by each entry of a /proc directory, busiest
 * first, printed as percent of one CPU. */
static void log_busiest(const char *label, const char *dir, struct tracked *prev, int *nprev, double elapsed_s,
                        int skip_self, int show) {
	struct tracked cur[MAX_TRACK];
	struct { char comm[24]; int id; long long d; } top[MAX_TRACK];
	int ncur = 0, ntop = 0;
	long hz = sysconf(_SC_CLK_TCK);
	DIR *d = opendir(dir);
	struct dirent *e;
	if (!d) return;
	while ((e = readdir(d)) && ncur < MAX_TRACK) {
		char path[300];
		int id = atoi(e->d_name);
		if (id <= 0 || (skip_self && id == getpid())) continue;
		snprintf(path, sizeof path, "%s/%d/stat", dir, id);
		long long t = proc_ticks(path, cur[ncur].comm, sizeof cur[ncur].comm);
		if (t < 0) continue;
		cur[ncur].id = id;
		cur[ncur].ticks = t;
		for (int i = 0; i < *nprev; i++) {
			if (prev[i].id == id && t > prev[i].ticks) {
				snprintf(top[ntop].comm, sizeof top[ntop].comm, "%s", cur[ncur].comm);
				top[ntop].id = id;
				top[ntop].d = t - prev[i].ticks;
				ntop++;
				break;
			}
		}
		ncur++;
	}
	closedir(d);
	memcpy(prev, cur, sizeof cur[0] * ncur);
	*nprev = ncur;
	if (!show) return;
	plog("  %s", label);
	for (int k = 0; k < 5 && ntop; k++) {
		int best = 0;
		for (int i = 1; i < ntop; i++) if (top[i].d > top[best].d) best = i;
		plog(" %s[%d]=%.0f%%", top[best].comm, top[best].id, top[best].d * 100.0 / hz / elapsed_s);
		top[best] = top[--ntop];
	}
	plog("\n");
}

static void cpu_totals(long long *total, long long *busy, long long *self) {
	unsigned long long u = 0, n = 0, sy = 0, idle = 0, iow = 0, irq = 0, sirq = 0;
	FILE *f = fopen("/proc/stat", "r");
	*total = *busy = 0;
	if (f) {
		if (fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu", &u, &n, &sy, &idle, &iow, &irq, &sirq) >= 4) {
			*busy = u + n + sy + irq + sirq;
			*total = *busy + idle + iow;
		}
		fclose(f);
	}
	*self = proc_ticks("/proc/self/stat", NULL, 0);
}

/* System-wide counters: where CPU time no process owns goes (interrupts,
 * softirqs), how many processes are started, and context switches. */
#define MAX_IRQ 64
static struct { char name[40]; unsigned long long n; } irq_prev[MAX_IRQ];
static int n_irq_prev;
static unsigned long long sys_prev[7], forks_prev, ctxt_prev, intr_prev;
static int sys_have;

static void log_system(double secs, int show) {
	unsigned long long f[7] = {0}, forks = 0, ctxt = 0, intr = 0;
	char line[512];
	FILE *fp = fopen("/proc/stat", "r");
	if (fp) {
		while (fgets(line, sizeof line, fp)) {
			if (!strncmp(line, "cpu ", 4))
				sscanf(line + 4, "%llu %llu %llu %llu %llu %llu %llu", &f[0], &f[1], &f[2], &f[3], &f[4], &f[5], &f[6]);
			else if (!strncmp(line, "intr ", 5)) sscanf(line + 5, "%llu", &intr);
			else if (!strncmp(line, "ctxt ", 5)) sscanf(line + 5, "%llu", &ctxt);
			else if (!strncmp(line, "processes ", 10)) sscanf(line + 10, "%llu", &forks);
		}
		fclose(fp);
	}
	if (show && sys_have) {
		unsigned long long d[7], tot = 0;
		for (int i = 0; i < 7; i++) { d[i] = f[i] - sys_prev[i]; tot += d[i]; }
		if (!tot) tot = 1;
		plog("  system     user=%.0f%% nice=%.0f%% sys=%.0f%% idle=%.0f%% iowait=%.0f%% irq=%.0f%% softirq=%.0f%% | "
		     "new processes/s=%.1f switches/s=%.0f interrupts/s=%.0f\n",
		     d[0] * 100.0 / tot, d[1] * 100.0 / tot, d[2] * 100.0 / tot, d[3] * 100.0 / tot, d[4] * 100.0 / tot,
		     d[5] * 100.0 / tot, d[6] * 100.0 / tot, (forks - forks_prev) / secs, (ctxt - ctxt_prev) / secs,
		     (intr - intr_prev) / secs);
	}
	memcpy(sys_prev, f, sizeof f);
	forks_prev = forks; ctxt_prev = ctxt; intr_prev = intr; sys_have = 1;

	/* Busiest interrupt lines. */
	struct { char name[40]; unsigned long long n, d; } cur[MAX_IRQ];
	int ncur = 0;
	if ((fp = fopen("/proc/interrupts", "r"))) {
		while (ncur < MAX_IRQ && fgets(line, sizeof line, fp)) {
			char *colon = strchr(line, ':');
			unsigned long long n;
			if (!colon || sscanf(colon + 1, "%llu", &n) != 1) continue;
			char *id = line;
			while (*id == ' ') id++;
			char *desc = colon + 1;
			while (*desc == ' ' || (*desc >= '0' && *desc <= '9')) desc++;
			desc[strcspn(desc, "\n")] = '\0';
			snprintf(cur[ncur].name, sizeof cur[ncur].name, "%.*s:%s", (int)(colon - id), id, desc);
			cur[ncur].n = n;
			cur[ncur].d = 0;
			for (int i = 0; i < n_irq_prev; i++)
				if (!strcmp(irq_prev[i].name, cur[ncur].name) && n >= irq_prev[i].n) cur[ncur].d = n - irq_prev[i].n;
			ncur++;
		}
		fclose(fp);
	}
	if (show && n_irq_prev) {
		plog("  irqs/s    ");
		for (int k = 0; k < 5; k++) {
			int best = -1;
			for (int i = 0; i < ncur; i++) if (cur[i].d && (best < 0 || cur[i].d > cur[best].d)) best = i;
			if (best < 0) break;
			char *nm = cur[best].name;
			while (*nm == ' ') nm++;
			plog(" [%s]=%.0f", nm, cur[best].d / secs);
			cur[best].d = 0;
		}
		plog("\n");
	}
	for (int i = 0; i < ncur; i++) { snprintf(irq_prev[i].name, sizeof irq_prev[i].name, "%s", cur[i].name); irq_prev[i].n = cur[i].n; }
	n_irq_prev = ncur;
}

static void window_report(long long now) {
	double secs = (now - st.start) / 1e6;
	long long total, busy, self;
	cpu_totals(&total, &busy, &self);
	long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	double dt = total - st.cpu_total, self_pct = 0, other_pct = 0;
	if (dt > 0 && ncpu > 0) {
		self_pct = (self - st.cpu_self) * 100.0 * ncpu / dt;
		other_pct = ((busy - st.cpu_busy) - (self - st.cpu_self)) * 100.0 * ncpu / dt;
	}
	plog("[%.0fs] runs/s=%.1f drawn=%u dupes=%u | run avg/max=%.1f/%.1fms gap max=%.1fms >20ms=%u | "
	     "video cb avg/max=%.2f/%.1fms | audio %lu frames/s cb avg/max=%.2f/%.1fms single=%lu | "
	     "polls=%u env=%u | %ux%u pitch %zu | cpu self/other=%.0f%%/%.0f%% loop=%ldus | "
	     "SetFrameSkip 1/0=%u/%u skips in a row max=%u\n",
	     now / 1e6, st.runs / secs, st.drawn, st.dupes, st.run_sum / 1000.0 / (st.runs ? st.runs : 1),
	     st.run_max / 1000.0, st.gap_max / 1000.0, st.gaps_over_20ms,
	     st.video_sum / 1000.0 / ((st.drawn + st.dupes) ? st.drawn + st.dupes : 1), st.video_max / 1000.0,
	     (unsigned long)(st.audio_frames / secs), st.audio_sum / 1000.0 / (st.runs ? st.runs : 1),
	     st.audio_max / 1000.0, st.single_samples, st.polls, st.env_calls, st.w, st.h, st.pitch, self_pct,
	     other_pct, cpu_loop_us(), st.skip_on, st.skip_off, st.dupe_run_max);
	if (st.trace_len) plog("  frames     %s\n", st.trace);
	log_busiest("threads   ", "/proc/self/task", prev_threads, &n_prev_threads, secs, 0, 1);
	log_busiest("others    ", "/proc", prev_procs, &n_prev_procs, secs, 1, 1);
	log_system(secs, 1);
	plog_flush();
	memset(&st.run_sum, 0, sizeof st - offsetof(typeof(st), run_sum));
	st.start = now;
	st.cpu_total = total;
	st.cpu_busy = busy;
	st.cpu_self = self;
}

/* ---- exported libretro API ------------------------------------------------------ */

#define EXPORT __attribute__((visibility("default")))

EXPORT void retro_set_environment(retro_environment_t cb) {
	f_env = cb;
	if (r_retro_set_environment) r_retro_set_environment(env_hook);
}
EXPORT void retro_set_video_refresh(retro_video_refresh_t cb) {
	f_video = cb;
	if (r_retro_set_video_refresh) r_retro_set_video_refresh(video_hook);
}
EXPORT void retro_set_audio_sample(retro_audio_sample_t cb) {
	f_sample = cb;
	if (r_retro_set_audio_sample) r_retro_set_audio_sample(sample_hook);
}
EXPORT void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
	f_batch = cb;
	if (r_retro_set_audio_sample_batch) r_retro_set_audio_sample_batch(batch_hook);
}
EXPORT void retro_set_input_poll(retro_input_poll_t cb) {
	f_poll = cb;
	if (r_retro_set_input_poll) r_retro_set_input_poll(poll_hook);
}
EXPORT void retro_set_input_state(retro_input_state_t cb) {
	f_state = cb;
	if (r_retro_set_input_state) r_retro_set_input_state(state_hook);
}
EXPORT void retro_init(void) {
	plog("  call       retro_init\n");
	if (r_retro_init) r_retro_init();
}
EXPORT void retro_deinit(void) {
	plog("  call       retro_deinit\n");
	plog_flush();
	if (r_retro_deinit) r_retro_deinit();
}
EXPORT unsigned retro_api_version(void) {
	return r_retro_api_version ? r_retro_api_version() : RETRO_API_VERSION;
}
EXPORT void retro_get_system_info(struct retro_system_info *info) {
	if (r_retro_get_system_info) r_retro_get_system_info(info);
	static int once;
	if (!once++) plog("  core       %s %s\n", info->library_name, info->library_version);
}
EXPORT void retro_get_system_av_info(struct retro_system_av_info *info) {
	if (r_retro_get_system_av_info) r_retro_get_system_av_info(info);
	plog("  av         %ux%u (max %ux%u) fps=%.3f rate=%.0f\n", info->geometry.base_width,
	     info->geometry.base_height, info->geometry.max_width, info->geometry.max_height, info->timing.fps,
	     info->timing.sample_rate);
}
EXPORT void retro_set_controller_port_device(unsigned port, unsigned device) {
	if (r_retro_set_controller_port_device) r_retro_set_controller_port_device(port, device);
}
EXPORT void retro_reset(void) {
	plog("  call       retro_reset\n");
	if (r_retro_reset) r_retro_reset();
}
EXPORT void retro_run(void) {
	long long t = now_us();
	if (!st.start) {
		st.start = t;
		cpu_totals(&st.cpu_total, &st.cpu_busy, &st.cpu_self);
		log_busiest("", "/proc/self/task", prev_threads, &n_prev_threads, 1, 0, 0);
		log_busiest("", "/proc", prev_procs, &n_prev_procs, 1, 1, 0);
		log_system(1, 0);
	} else if (st.last_run) {
		long long gap = t - st.last_run;
		if (gap > 1000000) {          /* the frontend's menu: start a new window */
			st.start = t;
			memset(&st.run_sum, 0, sizeof st - offsetof(typeof(st), run_sum));
		} else {
			if (gap > st.gap_max) st.gap_max = gap;
			if (gap > 20000) st.gaps_over_20ms++;
		}
	}
	long long gap = st.last_run ? t - st.last_run : 0;
	unsigned drawn_before = st.drawn;
	st.last_run = t;
	st.in_run = 1;
	if (r_retro_run) r_retro_run();
	st.in_run = 0;
	long long d = now_us() - t;
	if (st.trace_n < 40 && st.trace_len < (int)sizeof st.trace - 16) {
		st.trace_len += snprintf(st.trace + st.trace_len, sizeof st.trace - st.trace_len, "%s%lld/%lld%s",
		                         st.trace_n ? " " : "", d / 1000, gap / 1000, st.drawn == drawn_before ? "s" : "");
		st.trace_n++;
	}
	st.run_sum += d;
	if (d > st.run_max) st.run_max = d;
	st.runs++;
	if (t + d - st.start >= WINDOW_US) window_report(t + d);
}
EXPORT size_t retro_serialize_size(void) {
	return r_retro_serialize_size ? r_retro_serialize_size() : 0;
}
EXPORT bool retro_serialize(void *data, size_t size) {
	return r_retro_serialize ? r_retro_serialize(data, size) : false;
}
EXPORT bool retro_unserialize(const void *data, size_t size) {
	return r_retro_unserialize ? r_retro_unserialize(data, size) : false;
}
EXPORT void retro_cheat_reset(void) {
	if (r_retro_cheat_reset) r_retro_cheat_reset();
}
EXPORT void retro_cheat_set(unsigned index, bool enabled, const char *code) {
	if (r_retro_cheat_set) r_retro_cheat_set(index, enabled, code);
}
EXPORT bool retro_load_game(const struct retro_game_info *game) {
	bool ok = r_retro_load_game ? r_retro_load_game(game) : false;
	plog("  call       retro_load_game %s -> %d\n", game && game->path ? game->path : "(null)", ok);
	plog_flush();
	return ok;
}
EXPORT bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) {
	return r_retro_load_game_special ? r_retro_load_game_special(type, info, num) : false;
}
EXPORT void retro_unload_game(void) {
	plog("  call       retro_unload_game\n");
	plog_flush();
	if (r_retro_unload_game) r_retro_unload_game();
}
EXPORT unsigned retro_get_region(void) {
	return r_retro_get_region ? r_retro_get_region() : RETRO_REGION_NTSC;
}
EXPORT void *retro_get_memory_data(unsigned id) {
	return r_retro_get_memory_data ? r_retro_get_memory_data(id) : NULL;
}
EXPORT size_t retro_get_memory_size(unsigned id) {
	return r_retro_get_memory_size ? r_retro_get_memory_size(id) : 0;
}

/* The stock Snes9x 2005's own settings calls; logged, then passed on. */
EXPORT void SetFrameSkip(int on) {
	/* Called before every frame: count, and log only the first calls (writing
	 * each one to the card would slow the frontend down). */
	static int logged;
	if (on) st.skip_on++; else st.skip_off++;
	if (logged < 3) {
		logged++;
		plog("  call       SetFrameSkip(%d)%s\n", on, r_SetFrameSkip ? "" : " (not in this core)");
	}
	if (r_SetFrameSkip) r_SetFrameSkip(on);
}
EXPORT void init_sfc_setting(void) {
	plog("  call       init_sfc_setting()%s\n", r_init_sfc_setting ? "" : " (not in this core)");
	plog_flush();
	if (r_init_sfc_setting) r_init_sfc_setting();
}
