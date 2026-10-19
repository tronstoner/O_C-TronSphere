# Build, flash, and select applets

Target: Ornament & Crime on **Teensy 3.2** (MK20DX256, 256 KB flash, 64 KB RAM), built with PlatformIO from `firmware/software/platformio.ini`. All commands go through `Taskfile.yml` at the repo root.

## Prerequisites (macOS)

```sh
brew install platformio go-task    # pipx install platformio also works
task setup                         # checks pio, prefetches the Teensy toolchain (~/.platformio/packages)
```

The first build downloads `platform teensy @ 5.2.0`, `toolchain-gccarmnoneeabi-teensy`, `framework-arduinoteensy` and `tool-teensy`.

## Tasks

`ENV` defaults to `custom_min`. Override it per call: `task build ENV=custom_min_vor`.

| Task | What it does |
|---|---|
| `task` | Lists tasks |
| `task setup` | Checks for `pio` (prints install hint), runs `pio pkg install -e $ENV` |
| `task build` | `pio run -e $ENV` in `firmware/software`, copies hex to `build/<env>-<git short sha>.hex`, prints size |
| `task size` | Flash/RAM of last `$ENV` build (`arm-none-eabi-size -A` on the newest `.elf`) |
| `task flash` | `build`, then `teensy_loader_cli --mcu=TEENSY32 -w -v build/<env>-<sha>.hex` (waits for the program button). If the loader is missing it falls back to `pio run -e $ENV -t upload` (teensy-gui) |
| `task catch` | Retries the loader on the already-built hex every 0.5 s, so you can press the button whenever. Ctrl-C aborts |
| `task clean` | `pio run -e $ENV -t clean` and `rm -rf build/` |
| `task monitor` | `pio device monitor` (see USB serial below) |
| `task upstream-fetch` | `git fetch upstream T32` and lists upstream commits since the last subtree pull. Read-only. See `docs/UPSTREAM.md` |
| `task host:*` | Included from `host/Taskfile.yml` if it exists (host tests/simulator) |

The hex name embeds `git rev-parse --short HEAD`. Uncommitted changes are not reflected in the build/ name; the firmware's internal build tag (`res/progname.py`) gets a `dirty` suffix.

## Env matrix

| Env | Flags on top of `env:T3` | Purpose |
|---|---|---|
| `custom_min` | `-DCUSTOM_BUILD -I../.. -DOC_VERSION_EXTRA="\"_custom\""` | Our firmware: Hemisphere shell + our applets only |
| `custom_min_vor` | `custom_min` + `-DVOR` | Plum Audio VOR hardware variant (not yet tested) |
| `T32` (upstream) | full-screen apps, `PEWPEWPEW` applet pick, Grids2 | Reference only. Don't modify it |

`-I../..` is relative to `firmware/software/`, so it points at the repo root. Firmware code includes our files as `#include "custom/applets/Hello.h"`.

To add an env, add it to the `; --- custom build ---` block in `firmware/software/platformio.ini`:

```ini
[env:custom_foo]
extends = env:T3
build_flags =
    ${env:custom_min.build_flags}
    -DSOME_FLAG
```

`res/progname.py` needs no changes. It adds `OC_VERSION_EXTRA` (and `+VOR`) to the output file name.

### Full-screen apps

`custom_min` defines no `ENABLE_APP_*`. The app menu (`src/OC_apps.cpp`, `available_apps[]`) then still has the apps that are always compiled in: **Setup / About** (includes calibration), **Hemispheres**, **Scale Editor**, **Waveform Editor**, **Backup / Restore**. Those are unconditional upstream, and Setup/calibration is required, so they stay.

## Applet selection

- Registry: `custom/hemisphere_config_custom.h`. Each line is `DeclareApplet<Class>{id, categories},`. It is included inside `AppletRegistry reg{...}` in `firmware/software/src/hemisphere_config.h` when `CUSTOM_BUILD` is defined. Only listed applets are linked.
- Headers for our applets go in the `CUSTOM_APPLET_INCLUDES` section of the same file. Upstream applet headers are already included by `hemisphere_config.h`.
- IDs: our applets use **200+**, are never reused, and are never renumbered, because presets store the id. Upstream applets keep their upstream id (copy it from the `PEWPEWPEW` block in `hemisphere_config.h`).
- Principle: **one applet at a time**. Upstream applets are opted out by default. Add one only when explicitly chosen, then rebuild and check `task size`.
- Ponglet (id 99) is **not** registered under `CUSTOM_BUILD`. The registry works with a single applet. Unknown preset ids fall back to index 0 (`HS::get_applet_index_by_id`), and the defaults in `APP_HEMISPHERE.h` (ids 18/15) resolve to index 0 as well. Caveat: in Setup > applet show/hide, hiding *every* applet makes `HS::get_next_applet_index` loop forever (upstream behaviour). Don't hide the last visible applet.

### Code layout

```
custom/
  core/          pure C++17, no Arduino includes -> host-testable logic (e.g. hello_core.h)
  applets/       thin HemisphereApplet glue (e.g. Hello.h, id 200)
  hemisphere_config_custom.h   applet registry for CUSTOM_BUILD
```

`custom/applets/Hello.h` is the template for new applets: logic in a `custom::XxxCore` struct, glue maps `In/Out`, encoder, `View`, `OnDataRequest/OnDataReceive` (64-bit pack), `SetHelp`. The API reference is `firmware/software/src/HemisphereApplet.h` and `src/applets/Boilerplate.h.txt`. The ISR runs at 16666 Hz, and each hemisphere draws in 64x64 local coordinates.

## Flash/RAM budget (measured 2026-10-10, upstream 9c87050)

| Env | Flash (of 262144) | RAM (of 65536) |
|---|---|---|
| `custom_min` (Hello + Likembe, immediate settings + `Clk` source, 2026-10-19; `task size`) | 98 660 B (37.6 %) | 20 784 B (31.7 %) |
| `custom_min` (Hello only) | 92 828 B (35.4 %) | 20 472 B (31.2 %) |
| upstream `T32` | 225 340 B (86.0 %) | 38 156 B (58.2 %) |

Figures are PlatformIO's "Checking size" output. `task size` sums ELF sections and can differ by a few bytes.

## Flashing / bootloader

1. Connect the Teensy USB (on the back of the O_C module) to the Mac. If the Teensy's VUSB/VIN pads are *not* cut on your unit, power the module from USB only (eurorack power off) to avoid back-feeding. Most O_C builds have them cut, so eurorack power can stay on.
2. `task flash` (or `task build && task catch`).
3. When it prints "Press the program button", press the small **program button on the Teensy 3.2** board, reachable from the back of the module. The loader writes the hex and reboots the Teensy.

`teensy_loader_cli` comes from PlatformIO's `tool-teensy` package: `~/.platformio/packages/tool-teensy/teensy_loader_cli` (a bash wrapper setting `DYLD_LIBRARY_PATH` for the x86_64 `teensy_loader_cli_bin`, which runs under Rosetta on Apple Silicon). Call the wrapper, not `_bin` directly, because the binary alone fails with a libusb dyld error. MCU name: `TEENSY32`.

Flashing has **not** been verified on hardware yet.

## USB serial / monitor

All T3 envs build with `-DUSB_MIDI`. With that USB type Teensyduino exposes `Serial` only as HID "seremu", not as a `/dev/cu.usbmodem*` tty, so `pio device monitor` sees nothing. Options:
- For a debug build, add an env with `-DUSB_MIDI_SERIAL` replacing `-DUSB_MIDI` (the base `[env]` build_flags contain `-DUSB_MIDI`, so that env must list its build_flags explicitly instead of using `${env.build_flags}`). Then use `task monitor`.
- Or use `~/.platformio/packages/tool-teensy/teensy_serialmon`, which talks seremu (not wired into a task yet).

## Troubleshooting

- `custom/...: No such file`: check that the env has `-I../..`. Paths are relative to `firmware/software/`.
- Hex shows up in `build/` with an old sha: you built with uncommitted changes. The sha is HEAD's.
- Upstream warnings (`SmoothedOut deprecated`, `EnvSeq -fpermissive`) come from upstream applet headers that are always *compiled* (all headers are included) even when not registered. Harmless.
- The first build takes about 40 s including the toolchain download, and incremental builds take a few seconds.
