# Host tests and simulation (macOS)

`host/` compiles our pure cores and a slice of the real Phazerville firmware
with the system `g++` (Apple clang), no frameworks, no extra packages.

**Purpose:** fast feedback on logic and on what an applet draws, without
flashing. **Limits:** memory layout, flash/RAM budget, ISR timing, DMA, real
ADC noise/calibration and the feel of the encoders are invisible on the host.
The hardware is the source of truth; `task build` + `task size` still apply.

## Layout

```
host/
  Taskfile.yml          tasks test/render/sim/clean (root: task host:<name>)
  run_tests.sh          tier 1: build+run every tests/test_*.cpp
  run_render.sh         tier 2: real weegfx.cpp -> PBM/PGM/ASCII
  run_sim.sh            tier 3: applet harness (builds on demand, then runs)
  gen.sh                writes host-patched copies of 2 firmware files to build/gen
  tests/test_hello_core.cpp
  render/render_main.cpp, render/oled_dump.h   frame -> PBM / 4x PGM / ASCII
  sim/sim_main.cpp      CLI script runner, ISR loop, View() render
  sim/host_runtime.cpp  globals declared by stubs (time, pins, CV, usbMIDI)
  sim/host_firmware.cpp replacements for hardware-bound firmware definitions
  stub/                 Arduino.h, FS.h, avr/pgmspace.h, oc_host_prelude.h
  build/                output (ignored by the root .gitignore `build/`)
```

## Running

```sh
task host:test       # tier 1, exit code != 0 on failure
task host:render     # tier 2, writes host/build/out/render.pbm, render_x4.pgm
task host:sim -- --in0 1536 --press --encoder +3 --seconds 2
task host:clean
```

`.pbm`/`.pgm` open in Preview (`open host/build/out/sim_x4.pgm`).

### Sim script

Arguments run left to right against one `Hello` instance (left hemisphere):

| arg | effect |
|---|---|
| `--seconds S` / `--ticks N` | run the ISR loop at `OC_CORE_ISR_FREQ` (16666 Hz) |
| `--inK V` | CV input K (0..3), pitch units: 128 = 1 semitone, 1536 = 1 V |
| `--gateK 0\|1` | trigger input K (0..3) |
| `--encoder N` | N detents through `OnEncoderMove` |
| `--press` | `OnButtonPress` (toggles edit mode) |
| `--side L\|R` | hemisphere (R uses inputs/outputs 2,3) |
| `--out DIR` / `--quiet` | output dir (default host/build/out) / no ASCII dump |

At the end it prints simulated time, edit state, `frame.ViewOut(0..3)`, writes
`sim.pbm` + `sim_x4.pgm` and dumps the frame as half-block ASCII. Example:
`--in0 1536 --in1 -768 --press --encoder +3 --seconds 0.5` gives
`out0=1920 out1=-384`; `--encoder -70` clamps at `HEMISPHERE_MIN_CV` (-4608).

Per tick it mirrors `HSApplication::BaseController` + `HemisphereManager::Controller`:
`++OC::CORE::ticks; frame.Load(); applet.Controller(); ProcessCursors(); frame.Send();`.
Render mirrors the Hemisphere app: `graphics.Begin(); applet.BaseView(); graphics.End();`
(header + `View()`, only the 64 px half the applet owns is drawn).

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

To simulate a new applet: include its header in `host/sim/sim_main.cpp` and
change `Hello applet;` (an `--applet` switch is the next step once there are two).

## Known gaps

- One applet, one hemisphere; no `HemisphereManager`, no applet registry,
  presets, config menu, clock setup (`ClockSetup_instance.Controller()` is
  skipped, so the internal clock never ticks), MIDI in/out, EEPROM.
- Outputs are read from `frame.ViewOut()` (pitch units), not DAC codes; the
  DAC calibration is a dummy.
- `OC::ADC` returns the scripted value directly: no smoothing, offset, noise.
- `millis()` advances only via the ISR loop (60 us per tick, integer).
- Sim compiles with `-w` (upstream code is noisy on clang); tests use `-Werror`.
- No interactive mode yet.

## Next steps

- Tier 4: interactive SDL2 window (`brew install sdl2`): 128x64 at 6x, arrows /
  q,w,e,r for encoders, space/enter for buttons, number keys to step CV inputs,
  render every ~33 ms while running ticks in real time. Reuses `host_tick`/`host_render`.
- `--applet NAME` switch, both hemispheres, CSV trace of outputs per tick.
- Golden-image tests: compare `sim.pbm` against checked-in references.
