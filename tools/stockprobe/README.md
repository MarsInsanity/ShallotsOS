# stockprobe

A pass-through libretro core that measures the frontend it runs in. It
compares the stock firmware's frontend (rkgame) with ShallotsOS's picoarch
running the same stock core.

## How it works

stockprobe takes the place of a stock core file, for example
`cubegm/cores/libemu_sfc.so`. It loads the real core from `cubegm/cores/real/`
under the same name and forwards every libretro call to it. The stock Snes9x
2005's `SetFrameSkip` and `init_sfc_setting` are forwarded too. Nothing of the
real core is copied or changed.

It appends to `/mnt/sdcard/logs/stockprobe.log`:

- **Once, at load:**
  - the frontend's path and CRC32, plus `cubegm/rkgame`, the real core and
    `config.xml` (to compare with the public firmware backups);
  - the kernel and OS version, scheduling policy, nice and CPU affinity;
  - a CPU speed loop;
  - the libraries the frontend has loaded and the `/dev` nodes;
  - every core option with the value the frontend gives, the environment
    calls, and the stock settings calls.
- **Every 5 s of play:**
  - retro_run calls per second, and their average and maximum time;
  - frames drawn vs duplicated (skipped), and the largest gap between runs;
  - time spent in the frontend's video and audio callbacks, and audio frames
    per second;
  - CPU use of this process vs everything else, and the busiest threads and
    other processes;
  - the speed loop again.

The same file works in both frontends, so the logs compare directly.

## Use

1. Run `./build.sh` (needs the SF3000 toolchain in `~/sf3000-work`), which
   writes `build/stockprobe.so`.
2. Copy `stockprobe.so` and the two `.bat` files to the SD card root.
3. Run `install_stockprobe.bat`. It moves `libemu_sfc.so`, `libemu_snes9x.so`
   and `libemu_snes9x2010.so` into `cubegm/cores/real/` and puts stockprobe in
   their place.
4. Play, then read `logs/stockprobe.log`.
5. `uninstall_stockprobe.bat` puts the cores back.

Public R36HD backup, for comparison (CRC32):

| File | Bytes | CRC32 |
|---|---|---|
| `cubegm/rkgame` | 6208380 | `AC82CCED` |
| `cubegm/cores/config.xml` | 3530 | `AFFB29A7` |
| `libemu_sfc.so` | 804508 | `B601A0BF` |
| `libemu_snes9x.so` | 2718028 | `A9E8F70B` |
| `libemu_snes9x2010.so` | 2111400 | `788281C2` |

Note: the TreeFrog/ShallotsOS card's `rkgame` is a different build:
1178732 bytes, CRC32 `9A1BD663` (first stockprobe log, R36SX).

## First results (stock frontend, ShallotsOS card, Yoshi's Island)

- One CPU core online (`cpus=1`, 74Kc, BogoMIPS 589.82).
- rkgame calls `SetFrameSkip` before every frame: `1` skips drawing the next
  frame, `0` draws it. The core's default, without these calls, is every
  other frame. In heavy scenes it drew 150-200 of ~295 frames per 5 s, but
  kept ~59.5 runs/s and steady audio (~43,700 frames/s).
- retro_run averaged ~11 ms (light scenes ~5 ms); the video and audio
  callbacks don't block (~0.6 ms and ~0.05 ms).
- Other CPU use: cubevol 3-4%. (`mmcqd/0` at 12-15% came from the first
  probe logging every SetFrameSkip call; fixed.)

## Second run (probe v2, no per-call logging)

- Light scenes: 300 of 300 frames drawn per 5 s at ~12-13 ms per frame; the
  frontend sleeps to 16.7 ms between frames.
- Heavy Yoshi's Island scenes: 165-200 frames drawn per 5 s, still ~59.6
  emulated per second. Frames run back to back.
- **A drawn frame costs 17-22 ms there; a skipped one 7-9 ms.** Drawing (the
  PPU renderer) is over half of a heavy frame.
- The skip rule, read from the frame traces: draw until about one whole frame
  (~16 ms) behind the schedule, then skip; at most 3 skips in a row.
- Other processes: ~5-6% (cubevol 3-4%).

## Third run (probe v3): what ShallotsOS costs on top

Same game, same scene, stock Snes9x 2005 core:

| | stock frontend | ShallotsOS picoarch |
|---|---|---|
| kernel (sys) | 6% | 18-20% |
| new processes / s | 0.8 | ~24 |
| CPU used by other processes | 5-6% | 24-25% |
| SD card interrupts / s | ~30 | 2,500-17,000 in the first 2 minutes, then low |

The ~24 processes a second came from the boot script (`cubegm/zhijack.sh`).
It polled for FrogUI's launch file with `sleep 0.05` while the picoarch it
started was alive. picoarch execs between FrogUI and games in the same
process, so the poll ran during every game.

- Fixed: the template now polls once a second, and FrogUI patches installed
  scripts in place at startup, taking effect from the next boot.
- The game monitor now runs only with `log.txt`, every 10 s.

`STOCKPROBE_LOG=<path>` sends the log elsewhere, for tests off the device.
