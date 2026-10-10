# Host tests and simulation (macOS)

`host/` compiles our pure cores and a slice of the real Phazerville firmware
with the system `g++` (Apple clang). Tiers 1-3 need no extra packages; the
interactive window (tier 4) needs SDL2 (`brew install sdl2`).

**Purpose:** fast feedback on logic and on what an applet draws, without
flashing. **Limits:** memory layout, flash/RAM budget, ISR timing, DMA, real
ADC noise/calibration and the feel of the encoders are invisible on the host.
The hardware is the source of truth; `task build` + `task size` still apply.

## Layout

```
host/
  Taskfile.yml          tasks test/render/sim/ui/clean (root: task host:<name>)
  run_tests.sh          tier 1: build+run every tests/test_*.cpp
  run_render.sh         tier 2: real weegfx.cpp -> PBM/PGM/ASCII
  run_sim.sh            tier 3/4: builds libsimcore.a + sim / ui on demand, then runs
                        (`run_sim.sh --ui ...` runs the SDL2 window)
  gen.sh                writes host-patched copies of 2 firmware files to build/gen
  tests/test_hello_core.cpp
  render/render_main.cpp, render/oled_dump.h   frame -> PBM / 4x PGM / ASCII
  render/png.h          dependency-free PNG writer (stored deflate)
  sim/sim_core.h/.cpp   THE tick loop + render + controls, shared by sim and ui;
                        the header has no firmware includes
  sim/sim_main.cpp      tier 3: CLI script runner on top of sim_core
  sim/ui_main.cpp       tier 4: SDL2 window + panel on top of sim_core
  sim/host_runtime.cpp  globals declared by stubs (time, pins, CV, usbMIDI)
  sim/host_firmware.cpp replacements for hardware-bound firmware definitions
  stub/                 Arduino.h, FS.h, avr/pgmspace.h, oc_host_prelude.h
  compile_flags.txt     clangd flags (same includes/prelude as the build; SDL2 from brew)
  build/                output (ignored by the root .gitignore `build/`)
```

## Running

```sh
task host:test       # tier 1, exit code != 0 on failure
task host:render     # tier 2, writes host/build/out/render.pbm, render_x4.pgm
task host:sim -- --in0 1536 --press --encoder +3 --seconds 2
task host:ui         # tier 4, interactive window; task host:ui -- --scale 8
task host:clean      # removes host/build (both binaries, libsimcore.a, outputs)
```

`.pbm`/`.pgm` open in Preview (`open host/build/out/sim_x4.pgm`).

### Sim script

Arguments run left to right against `Hello` (left hemisphere by default):

| arg | effect |
|---|---|
| `--seconds S` / `--ticks N` | run the ISR loop at `OC_CORE_ISR_FREQ` (16666 Hz) |
| `--inK V` | CV input K (0..3), pitch units: 128 = 1 semitone, 1536 = 1 V |
| `--gateK 0\|1` | trigger input K (0..3) |
| `--encoder N` | N detents through `OnEncoderMove` |
| `--press` | `OnButtonPress` (toggles edit mode) |
| `--side L\|R\|LR` | boot option: Hello on the left, right (inputs/outputs 2,3) or both; with `LR`, `--encoder`/`--press` address the left one |
| `--out DIR` / `--quiet` | output dir (default host/build/out) / no ASCII dump |

At the end it prints simulated time, edit state, `frame.ViewOut(0..3)`, writes
`sim.pbm` + `sim_x4.pgm` and dumps the frame as half-block ASCII. Example:
`--in0 1536 --in1 -768 --press --encoder +3 --seconds 0.5` gives
`out0=1920 out1=-384`; `--encoder -70` clamps at `HEMISPHERE_MIN_CV` (-4608).

Both front ends call the same `simcore::Tick()` / `simcore::Render()`
(`host/sim/sim_core.cpp`). Per tick it mirrors `HSApplication::BaseController` + `HemisphereManager::Controller`:
`++OC::CORE::ticks; frame.Load(); <each active applet>.Controller(); ProcessCursors(); frame.Send();`.
Render mirrors the Hemisphere app: `graphics.Begin(); <each active applet>.BaseView(); graphics.End();`
(header + `View()`, each applet draws only the 64 px half it owns).

## Interactive simulator

```sh
task host:ui                         # 6x scale, Hello left + Hello right
task host:ui -- --scale 8 --right none
```

A native window shows the OLED (scaled, with a pixel grid) and a panel below
it. Same `sim_core` as the scripted harness, so what you see is the real
`HemisphereApplet` code. Options: `--scale N` (1..16, default 6),
`--right hello|none`, `--keys LIST`, `--screenshot-after S`, `--headless`,
`--out DIR` (default `host/build/out`).

### Keys

| key | action | firmware equivalent |
|---|---|---|
| `A` / `D` | left encoder -/+ (key repeat spins) | `OnEncoderMove(-1/+1)` |
| `S` | left encoder push (on key release) | `OnButtonPress()` on release: toggles edit mode |
| `J` / `L` | right encoder -/+ | as above, right hemisphere |
| `K` | right encoder push | as above |
| `Q` | Up button (A, left), on release | while editing: `AuxButton()` (cancels edit); otherwise select mode, which needs `HemisphereManager` -> hint only |
| `E` | Down button (B, right), on release | same for the right hemisphere |
| `Q`+`E` held together | Clock Setup | not simulated (ClockSetup is not ticked) -> hint; both releases are swallowed like the firmware's ignore mask |
| `1`..`4` | select CV input (highlighted `>` row) | |
| `Up` / `Down` | selected CV +/- 1 semitone (128 units) | |
| `Shift`+`Up`/`Down` | selected CV +/- 1 octave (1536 units) | clamped to +/-10 V |
| `0` | selected CV = 0 | |
| `Z X C V` | TR1..TR4 gate high while held | trigger inputs (active low pins) |
| `Shift`+`Z X C V` | toggle latch for that gate (`L` in the panel); gate = held OR latched | |
| `Space` | pause / resume simulated time | |
| `.` | single step 1 ms (17 ticks), works while paused | |
| `P` | screenshot | |
| `R` | `Reset()` then `Start()` on every applet | |
| `Esc` | quit | |

Encoder long-press is not mapped: the Hemisphere app uses it for app-level
actions only (long release L = clock run/stop, R = main menu, long press of
button B = config menu), none of which exist in the harness.

Losing window focus releases momentary gates and held buttons (key-ups would
otherwise be lost); latches, CVs, edit mode and pause are untouched. No
control resets unrelated state.

### Panel

- **INPUTS**: CV1..4 as volts and raw pitch units (selected row highlighted),
  TR1..4 `HIGH`/`low`, `L` when latched.
- **OUTPUTS**: A..D from `frame.ViewOut()`, volts + raw.
- Applet per hemisphere with edit state (`edit ON`/`edit off`).
- Simulated time (s and ticks), `PAUSED` or `RUN <ratio>x`; the ratio is
  simulated/wall time over 0.5 s and turns red with `BEHIND` below 0.98.
- Key legend and the last message (also echoed to stdout with the sim time).

All numbers go through one formatter (`fmt::` in `ui_main.cpp`): volts =
units / 1536 with 2 decimals and an explicit sign (`+0.00 V`, never `-0.00`),
raw as signed integer, time in seconds with 3 decimals. Change
`fmt::kVoltDecimals` there. The panel text uses the firmware's own 6x8 font
(`src/extern/gfx_font_6x8.h`), no font dependency.

### How time is simulated

Each video frame (vsync, ~60 fps) measures elapsed wall time and runs
`wall_dt * 16666` ticks (~278 per frame), carrying the fractional remainder,
so simulated time tracks the wall clock. If a frame would need more than
100 ms of ticks (debugger stop, window drag, slow applet), the backlog is
dropped instead of fast-forwarded and the ratio shows how far behind it ran.
`Space` stops ticking (display and inputs keep updating; held gates stay
high); `.` runs exactly 17 ticks. `--headless` skips SDL entirely and runs
278-tick frames as fast as possible.

### Screenshots

`P` (or `--screenshot-after S`, which then quits) writes to `host/build/out/`
with the next free number `NNN`:

- `ui_NNN_oled.png` 128x64 gray, `ui_NNN_oled.pbm`
- `ui_NNN_oled_xS.png` scaled OLED as shown in the window
- `ui_NNN_window.png` the full window including the panel

For checks without touching the keyboard, combine with `--keys` (comma list,
applied at t=0 through the same key handler; `shift+up`, `down:z`/`up:z` for
explicit press/release) and `--headless`:

```sh
host/run_sim.sh --ui --headless --screenshot-after 0.5 --keys s,d,d,d,1,shift+up
# -> edit mode on, offset +3, CV1 +1.00 V, output A +1.25 V (+1920)
```

### Limits / adding applets

- Applet is `Hello` only; right hemisphere is a second `Hello` instance or empty.
- No clock setup, select mode, help/config screens, presets, MIDI, EEPROM.

To add an applet: include its header in `sim_core.cpp`, replace the
`Hello applets[2]` array with `HemisphereApplet *applets[2]` pointing at
instances chosen by name, and pass the names through `simcore::Init()` from
an `--applet NAME` / `--right NAME` option in both front ends.

## Display mapping

weegfx (`weegfx.h` `get_frame_ptr`) and `SH1106_128x64_driver.cpp::SendPage(index, frame + index*128)`
share one layout: byte `frame[(y>>3)*128 + x]`, pixel = bit `y&7` (LSB = top).
`render_main.cpp` asserts this (`PASS layout_check`). `host/render/oled_dump.h`
decodes it; reuse `host::Pixel()` for any new view.

## How the stubs work

Two mechanisms, because C++ resolves includes differently:

1. **Angle includes** (`<Arduino.h>`, `<FS.h>`, `"avr/pgmspace.h"`) are shadowed
   by putting `-Ihost/stub` first on the include path.
2. **Quoted sibling includes** (`"OC_ADC.h"` from `HemisphereApplet.h`) are found
   in the including file's own directory first, so `-I` cannot shadow them.
   `stub/oc_host_prelude.h` is force-included (`-include`) into every TU; it
   defines the replacement class **and** the real header's include guard
   (`OC_ADC_H_`, `OC_FREQMEASURE_H`, `UTIL_MATH_H_` via the gen copy), so the
   real header becomes a no-op.
3. **Patched copies** (`gen.sh` -> `host/build/gen/`, `firmware/` untouched):
   - `weegfx.cpp`: line 533 defines `print(uint32_t, size_t)` but the header
     declares `(uint32_t, unsigned)` — same type on 32-bit ARM, not on arm64.
   - `util/util_math.h`: ARM inline asm (`usat`, `umull`) replaced by C.

### Stubbed headers

| header | how | provides |
|---|---|---|
| `Arduino.h` | shadow | int types, `PROGMEM`/`pgm_read_*`, `constrain`, `min/max`, `millis/micros` from `host_micros`, `digitalReadFast` from `host_pin_level[]`, `random`, `elapsedMillis/Micros`, `F_CPU`, `ARM_DWT_*` dummies, `usbMIDI` (constants + no-op send/read) |
| `FS.h` | shadow | empty `File`, minimal `String` (for `OC_scales.cpp` Scala I/O) |
| `avr/pgmspace.h` | shadow | forwards to `Arduino.h` |
| `OC_ADC.h` | prelude + guard | `OC::ADC` reading `host_adc_pitch[]` |
| `OC_FreqMeasure.h` | prelude + guard | no-op `FreqMeasureClass` |
| `util/util_math.h` | gen copy + guard | asm-free version |

### Firmware files compiled (sim)

Real, unmodified: `HemisphereApplet.cpp`, `HSUtils.cpp`, `HSIOFrame.cpp`,
`OC_strings.cpp`, `OC_scales.cpp`, `braids_quantizer.cpp`, `bjorklund.cpp`,
plus all headers they include (`OC_core.h`, `OC_DAC.h`, `OC_digital_inputs.h`,
`HSClockManager.h`, `HSMIDI.h`, `CVInputMap.h`, ...). Patched copy: `weegfx.cpp`.
Tier 2 compiles only `weegfx.cpp`.

Replaced by `host/sim/host_firmware.cpp` (real `.cpp` is hardware-bound):
`OC::CORE::ticks`, `FreeRam()` (OC_core.cpp); `graphics` (display.cpp);
`TR1..TR4` (OC_gpio.cpp); `ADC_CHANNEL_1..4`, `FreqMeasure` (OC_ADC.cpp);
`DAC_CHANNEL_A..D`, `DAC::calibration_data_` (linear dummy), `values_`,
`get_voltage_scaling()` (OC_DAC.cpp); `HS::applet_is_hidden/get_applet_name/get_applet_icon`
(hemisphere_config.h registry, not compiled).

## Adding a test for a new core

1. Keep the core in `custom/core/<name>_core.h`, plain C++17, no Arduino/O_C includes.
2. Add `host/tests/test_<name>_core.cpp` with `#include "custom/core/<name>_core.h"`;
   copy the `TEST`/`CHECK`/`RUN` macros from `test_hello_core.cpp`, return non-zero on failure.
3. `task host:test` picks it up (glob), flags `-O2 -std=c++17 -Wall -Wextra -Werror -I<repo>`.

To simulate a new applet: see "Limits / adding applets" above (`sim_core.cpp`).

## Known gaps

- One applet type (`Hello`, one per hemisphere); no `HemisphereManager`, no applet registry,
  presets, config menu, clock setup (`ClockSetup_instance.Controller()` is
  skipped, so the internal clock never ticks), MIDI in/out, EEPROM.
- Outputs are read from `frame.ViewOut()` (pitch units), not DAC codes; the
  DAC calibration is a dummy.
- `OC::ADC` returns the scripted value directly: no smoothing, offset, noise.
- `millis()` advances only via the ISR loop (60 us per tick, integer).
- Firmware TUs (incl. `sim_core.cpp`) compile with `-w` (upstream code is noisy
  on clang); `sim_main.cpp`/`ui_main.cpp` use `-Wall -Wextra`; tests use `-Werror`.
- Interactive mode: no mouse, no encoder long-press, no select mode / clock setup.

## Next steps

- `--applet NAME` switch, CSV trace of outputs per tick.
- Tick `ClockSetup`/`HemisphereManager` for select mode and clock setup in the UI.
- Golden-image tests: compare `sim.pbm` against checked-in references.
