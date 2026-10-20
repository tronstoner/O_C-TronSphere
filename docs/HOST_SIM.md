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
                        (`run_sim.sh --ui ...` runs the SDL2 window; no arguments = scenarios)
  gen.sh                writes host-patched copies of 2 firmware files to build/gen
  tests/test_hello_core.cpp
  render/render_main.cpp, render/oled_dump.h   frame -> PBM / 4x PGM / ASCII
  render/png.h          dependency-free PNG writer (stored deflate)
  sim/sim_core.h/.cpp   THE tick loop + render + controls + clock source +
                        output observation, shared by sim and ui; the header
                        has no firmware includes
  sim/host_applets.h    host applet registry (name -> factory) for --left/--right
  sim/applets/ClockEcho.h  host-only sanity applet (TR1 -> ClockOut A, TR2 -> GateOut B)
  sim/sim_main.cpp      tier 3: CLI script runner on top of sim_core
  sim/ui_main.cpp       tier 4: SDL2 window + panel on top of sim_core
  sim/host_runtime.cpp  globals declared by stubs (time, pins, CV, usbMIDI)
  sim/host_firmware.cpp replacements for hardware-bound firmware definitions
  sim/host_clocksetup.cpp the real ClockSetup applet + DigitalInputs/OC::ui stand-ins
  stub/                 Arduino.h, FS.h, avr/pgmspace.h, oc_host_prelude.h
  compile_flags.txt     clangd flags (same includes/prelude as the build; SDL2 from brew)
  build/                output (ignored by the root .gitignore `build/`)
```

## Running

```sh
task host:test       # tier 1, exit code != 0 on failure
task host:render     # tier 2, writes host/build/out/render.pbm, render_x4.pgm
task host:sim         # no arguments: runs the scenarios (see "Scenarios"), exit 1 on any FAIL
task host:sim -- --in0 1536 --press --encoder +3 --seconds 2
task host:ui         # tier 4, interactive window; task host:ui -- --scale 8
task host:clean      # removes host/build (both binaries, libsimcore.a, outputs)
```

`.pbm`/`.pgm` open in Preview (`open host/build/out/sim_x4.pgm`).

### Sim script

Boot options are position-independent and resolved before the script; the
remaining arguments run left to right against the left applet (or the right
one if the left hemisphere is empty).

| boot option | effect |
|---|---|
| `--left NAME\|none` | left applet from the host registry (default `Hello`), case-insensitive |
| `--right NAME\|none` | right applet (default `none`; uses inputs/outputs 3,4) |
| `--side L\|R\|LR` | legacy shorthand: `Hello` left, right or both; `--left`/`--right` win |
| `--clock BPM[:DIV]` | periodic clock source, DIV pulses per beat (default 4): `120:4` = 8 pulses/s, `120:6` = 16th triplets; running from t=0 |
| `--clock-tr N` | TR input 1..4 the clock drives (default 1 = TR1 = `Clock(0)` of the left applet) |
| `--trig N@T` | one trigger on TR N (1..4) at simulated time T s; repeatable |
| `--log-outputs FILE` | CSV, see "Output logging" (relative paths are under `host/build/out`) |
| `--log-events` | one stdout line per output trigger |

| script arg | effect |
|---|---|
| `--seconds S` / `--ticks N` | run the ISR loop at `OC_CORE_ISR_FREQ` (16666 Hz) |
| `--inK V` | CV input K (0..3), pitch units: 128 = 1 semitone, 1536 = 1 V |
| `--gateK 0\|1` | trigger input K (0..3) |
| `--encoder N` | N detents through `OnEncoderMove` |
| `--press` | `OnButtonPress` (toggles edit mode) |
| `--to L\|R` | following `--encoder` / `--press` / `--expect-state` act on the left / right applet (default: left if present) |
| `--out DIR` / `--quiet` | output dir (default host/build/out) / no ASCII dump |
| `--int-start` / `--int-arm` / `--int-stop` | internal clock: `clock_m.Start()` (running now; applets get `Reset()` via auto_reset) / `Start(true)` (armed, starts on the next TR1 pulse) / `Stop()` |
| `--int-bpm N` | internal tempo (`SetTempoBPM`) |
| `--mult S:M` | multiplier of input slot S (1..4 = left in 1, in 2, right in 1, in 2): `x1`..`x24`, `/2`..`/32`, `off` |
| `--ppqn N` | external sync: TR1 pulses per beat (`SetClockPPQN`, 0..24) |
| `--segment` | start a new segment of recorded output rises (segment 0 starts at boot) |
| `--expect-rises K N` | output K (0..3) rose from 0 to non-zero N times (`N` or `LO..HI`) in the current segment; prints PASS/FAIL |
| `--expect-same K A B N` | the first N rises of output K in segments A and B show identical values on all four outputs |
| `--expect-bpm N` | internal clock tempo (`N` or `LO..HI`) |
| `--expect-state TEXT` | the final state line (`t=... mult=x3,off,...`) contains `TEXT` |
| `--keys LIST` | panel keys as in the SDL window, tap or `down:`/`up:` prefix: `q`/`e` Up/Down (`down:q,down:e,up:q,up:e` opens ClockSetup), `g` toggle ClockSetup, `a`/`d`, `j`/`l` encoders, `s`/`k` pushes |
| `--png FILE` | write the current 128x64 frame as PNG (relative to `--out`) |

Any FAIL makes the harness exit 1. The final line also prints the internal clock state:
`int=running bpm=120 ppqn=4 mult=x4,off,off,off`.

At the end it prints simulated time, edit state, `frame.ViewOut(0..3)`, writes
`sim.pbm` + `sim_x4.pgm` and dumps the frame as half-block ASCII. Example:
`--in0 1536 --in1 -768 --press --encoder +3 --seconds 0.5` gives
`out0=1920 out1=-384`; `--encoder -70` clamps at `HEMISPHERE_MIN_CV` (-4608).

### Internal clock (ClockSetup)

The real upstream clock runs on the host: `HS::clock_m` (`HSClockManager.h`, defined in
`HemisphereApplet.cpp`) and the real `applets/ClockSetup.h`, compiled unmodified in
`host/sim/host_clocksetup.cpp` (`ClockSetup ClockSetup_instance`). Per tick `sim_core` mirrors
the hardware order: pin edge → `OC::tr1..4_ISR()` → `DigitalInputs::Scan()` →
`ClockSetup_instance.Controller()` (start on TR1 when armed, `SyncTrig()`) → `frame.Load()`
→ for each applet: `Reset()` if `clock_m.auto_reset`, then `Controller()` → clear
`auto_reset` (as `HemisphereManager::Controller`, `APP_HEMISPHERE.h:450-458`).

Stand-ins in `host_clocksetup.cpp`: `OC::DigitalInputs` statics + `Scan()` (same code as
`OC_digital_inputs.cpp`) and the four pin ISRs, called by `sim_core` on the rising trigger
edge; `OC::ui` with a no-op `_Poke()`. `usbMIDI.sendRealTime` is the existing no-op stub,
`HS::PokePopup` is the real one (`HSUtils.cpp`, popups are never drawn).

In: tempo, start / arm / stop, per-slot multiplier (x1..x24, ÷2..÷32, off), Input PPQN,
external sync on TR1 (averaging, nudge, auto-stop are upstream code and run), auto_reset.
**ClockSetup screen.** The real `ClockSetup_instance` screen is shown and operated like
`APP_HEMISPHERE.h` does: open with the Up+Down dual press (`DelegateSelectButtonPush`,
`APP_HEMISPHERE.h:669-676`: second button down while the other is held, both releases
ignored), close with any later single Up/Down click (`APP_HEMISPHERE.h:663-665`). While open,
`Render()` draws `ClockSetup_instance.View()` over the applets (`APP_HEMISPHERE.h:569-570`),
left encoder → `OnLeftEncoderMove()`, right encoder → `OnEncoderMove()`
(`APP_HEMISPHERE.h:739-744`), either encoder push → `OnButtonPress()` (`APP_HEMISPHERE.h:595`;
the sim fires it on release, hardware on button down). Hooks: `host_clocksetup.cpp`
(`HostClockSetupView/Encoder/Press`), state + button gesture in `sim_core` (`ButtonDown/Up`,
`ClockSetupShow`). While open, `sim_core` calls `View()` into a scratch frame every
`kViewKeepaliveTicks` (500) ticks, because `View()` restarts its slide-in animation when it
was not called for > 1000 ticks (the real display redraws every frame). Screen shortcut: `G`.
Operable through the screen: play/stop/arm, tempo (incl. tap tempo by repeated pushes on
Tempo), shuffle, Sync PPQN, multipliers (right enc ±1, left enc ×2/÷2), trigger-map source,
boops, in/out skip. ClockSetup leaves edit mode on its own after a few hundred ms, as on
hardware. Not in: MIDI clock (no MIDI input), popups (`HS::PokePopup` is real but
`DrawPopup` is never called), the clock indicator icon (`DrawIndicator()`) while the screen
is closed, help/select/config screens. Multipliers start at 0 (off) like a fresh
`ClockManager`; on hardware they come from the preset.

### Scenarios

`run_sim.sh` with no arguments (`task host:sim`) runs these and prints `SIM SCENARIOS OK`
or exits 1. All use Likembe left with Dens 100 % and Var 0 % so Out A (output 0) rises once
per engine step.

| scenario | checks |
|---|---|
| `likembe_int_both_sides` | Likembe left and right (both Dens 100 / Var 0), Src Int, internal clock 120 BPM, all slot multipliers off at boot: each applet sets x4 on its own slot; 0 steps on outputs 0 and 2 in 0.5 s before start, 16 steps each in 1.99 s after `--int-start` (8 steps/s); final state `mult=x4,off,x4,off` |
| `likembe_int_stopped_tr1_ignored` | Src Int, internal clock stopped, TR1 pulsing 8 per second → 0 steps in 2 s |
| `likembe_int_div_from_clocksetup` | `--mult 1:x3` after load (ClockSetup value), Src Int, 120 BPM → 12 steps in 1.99 s (6 steps/s), state `mult=x3,` |
| `likembe_int_div_from_applet` | Clock page Div x4 → x2 with the applet encoder (writes slot 1), 120 BPM → 8 steps in 1.99 s (4 steps/s), state `mult=x2,off` |
| `likembe_src_tr1_div3` | Clock page Src TR1, Div 3; 7 scripted TR1 pulses → 3 steps (pulses 1, 4, 7) |
| `likembe_src_tr1_jack_override` | Src TR1, internal clock running at 120 BPM with x4 on slot 1, scripted TR1 pulses every 500 ms → exactly 4 steps in 2 s (physical jack, internal clock ignored) |
| `likembe_rst_clockstart` | Src Int (slot x4), 120 BPM, Rst Off: run 1.3 s (11 steps), stop, start again → the first 10 steps repeat the first run exactly (clock start restarts at bar start) |
| `likembe_rst_tr2` | Rst TR2, Roll Off: run 1.3 s (11 steps), one TR2 pulse → the next 10 steps repeat the first run exactly |
| `clocksetup_screen` | Hello left; Up+Down opens ClockSetup, cursor → Tempo, edit, 120 → 100, cursor → slot-1 multiplier, off → x3 (list steps), cursor → trigger map, Up click closes; `--expect-bpm 100`, `--expect-state "mult=x3,"`; PNGs `host/build/out/clocksetup_{tempo,mult,trigmap}.png`. off → x3 = 6 right-encoder steps through the shared list |
| `clocksetup_mult_list` | Shared multiplier list in ClockSetup (`CUSTOM_BUILD` edit): cursor → slot-1 multiplier, edit, right encoder 13 steps from off → `mult=x12,` (clamps at x12; PNG `clocksetup_mult_x12.png`), left encoder 13 steps → `mult=off,` |
| `likembe_ext_sync_tr1` | armed clock at 120 BPM, TR1 pulses at 150 BPM x 4 PPQN: starts on TR1, tempo 149..151 BPM, Src Int, slot 1 x4 → 18..20 steps in 1.9 s (10 steps/s at 150 BPM) |

### Clock source and triggers

`--clock`, `--trig` and the UI's `T` drive the *physical* TR pins
(`host_pin_level[]`, active low), so the firmware's own path runs:
`frame.Load()` -> `trigmap` -> `CVInputMap::Clock()` edge detection ->
`frame.clocked[]` -> `Clock(ch)`. Each pulse is high for 17 ticks (~1 ms,
`HEMISPHERE_CLOCK_TICKS`) and is seen exactly once (one rising edge). The
pin level is manual gate (`--gateK`, keys) OR any pulse. Pulse starts are
scheduled on a fractional tick grid (period = 16666*60/(BPM*DIV) ticks,
2083.25 at 120:4), so there is no long-term drift; individual edges land on
the nearest following tick (jitter < 60 us). The first clock pulse fires on
the first tick (t = 0.000060 s).

### Output logging

After every tick `sim_core` compares `frame.ViewOut(0..3)` with the previous
tick and reports to a listener (`simcore::SetOutListener`):

- **change**: any value change. `--log-outputs FILE` writes
  `t_seconds,out_index,value_units,value_volts` (index 0..3 = A..D, 6 / 4
  decimals), starting with one row per output at t=0 with the initial value.
- **trigger**: `ClockOut()` fired (the frame's `clock_countdown` started or
  restarted) -> `ClockOut`; or the value jumped up to exactly the `GateOut`
  level (`PULSE_VOLTAGE * ONE_OCTAVE`, 6 V with the default `octave_max`) ->
  `GateOut`. A pitch CV that lands exactly on 6 V would also count.
  `--log-events` prints `event t=0.125105 out=A trig=ClockOut`.

Sanity check with the host-only `ClockEcho` applet:

```sh
task host:sim -- --left ClockEcho --clock 120:4 --seconds 2 --log-events --quiet
# 16 lines: event t=0.000060 out=A trig=ClockOut ... t=1.875075, 0.125 s apart
task host:sim -- --left ClockEcho --trig 2@0.5 --seconds 1 --log-events --quiet
# event t=0.500000 out=B trig=GateOut
```

### Adding an applet to the host registry

`host/sim/host_applets.h`: include the applet header and add one row to
`kHostApplets` below the comment `add new applets here`, e.g.
`{"Likembe", MakeApplet<Likembe>},`. Both front ends pick it up for
`--left`/`--right`; `run_sim.sh` rebuilds `libsimcore.a` when `custom/`,
`host_applets.h` or `host/sim/applets/` change. Host-only helper applets go
to `host/sim/applets/` (never into the firmware build).

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
`--left NAME|none`, `--right NAME|none` (both default `Hello`),
`--clock BPM[:DIV]` (starts the clock source running; without it the source
is configured 120:4 but stopped), `--clock-tr N`, `--trig N@T`, `--keys LIST`,
`--screenshot-after S`, `--headless`, `--out DIR` (default `host/build/out`).
`--screenshot-after` works with `--clock` (the clock runs in simulated time).

### Keys

| key | action | firmware equivalent |
|---|---|---|
| `A` / `D` | left encoder -/+ (key repeat spins) | `OnEncoderMove(-1/+1)` |
| `S` | left encoder push (on key release) | `OnButtonPress()` on release: toggles edit mode |
| `J` / `L` | right encoder -/+ | as above, right hemisphere |
| `K` | right encoder push | as above |
| `Q` | Up button (A, left), on release | while editing: `AuxButton()` (cancels edit); otherwise select mode, which needs `HemisphereManager` -> hint only |
| `E` | Down button (B, right), on release | same for the right hemisphere |
| `Q`+`E` held together | open the ClockSetup screen | dual press, `APP_HEMISPHERE.h:669-676`; both releases ignored (`SetButtonIgnoreMask`) |
| `Q` or `E` click while ClockSetup is open | close it | `APP_HEMISPHERE.h:663-665` |
| `G` | toggle the ClockSetup screen (shortcut) | |
| encoders / pushes while ClockSetup is open | left enc `OnLeftEncoderMove`, right enc `OnEncoderMove`, push `OnButtonPress` | `APP_HEMISPHERE.h:595,739-744` |
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
| `T` | clock source on/off (first pulse on the next tick) | |
| `[` / `]` | clock BPM -/+ 5 (5..600), phase kept | |
| `I` | internal clock start / stop | ClockSetup PLAY_STOP (start = `clock_m.Start()`, running at once) |
| `Shift`+`I` | arm internal clock (paused, starts on the next TR1 pulse) | `clock_m.Start(true)` |
| `-` / `=` | internal BPM -/+ 1, `Shift` -/+ 10 (5..600) | ClockSetup TEMPO |
| `M` / `Shift`+`M` | multiplier of the selected input slot (`1`..`4`) +1 / -1 (÷32..x24, 0 = off) | ClockSetup MULT1..4 |
| `N` / `Shift`+`N` | sync PPQN +1 / -1 (0..24) | ClockSetup EXT_PPQN |
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
- **OUTPUTS**: A..D from `frame.ViewOut()`, volts + raw, then a trigger
  marker: `*` (green) for 50 ms of simulated time after an output trigger
  (ClockOut/GateOut, same detection as `--log-events`), `.` otherwise.
- **CLOCK**: the external TR pulse source: BPM, pulses per beat, target TR, `RUNNING`/`stopped`.
- **INT**: the internal clock (`clock_m`): BPM, sync PPQN, multiplier per input slot
  (selected slot highlighted, `fmt::Mult`: `x4`, `/2`, `off`), `RUNNING` / `ARMED (TR1)` / `stopped`.
- Applet per hemisphere with edit state (`edit ON`/`edit off`).
- Simulated time (s and ticks), `PAUSED` or `RUN <ratio>x`; the ratio is
  simulated/wall time over 0.5 s and turns red with `BEHIND` below 0.98.
- Key legend and the last message (also echoed to stdout with the sim time).

All numbers go through one formatter (`fmt::` in `ui_main.cpp`): volts =
units / 1536 with 2 decimals and an explicit sign (`+0.00 V`, never `-0.00`),
raw as signed integer, time in seconds with 3 decimals, BPM via `fmt::Bpm`.
The scripted harness has its own single formatter (`fmt::` in `sim_main.cpp`,
`kTimeDecimals`/`kVoltDecimals`) for CSV and event lines. Change
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
explicit press/release, `lbracket`/`rbracket` for `[`/`]`) and `--headless`:

```sh
host/run_sim.sh --ui --headless --screenshot-after 0.5 --keys s,d,d,d,1,shift+up
# -> edit mode on, offset +3, CV1 +1.00 V, output A +1.25 V (+1920)
```

### Limits / adding applets

- Applets: whatever `host/sim/host_applets.h` lists (see "Adding an applet to the host registry").
- No select mode, help/config screens, presets, MIDI, EEPROM (the internal
  clock runs, see "Internal clock").


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
`applets/ClockSetup.h` (via `host/sim/host_clocksetup.cpp`),
plus all headers they include (`OC_core.h`, `OC_DAC.h`, `OC_digital_inputs.h`,
`HSClockManager.h`, `HSMIDI.h`, `CVInputMap.h`, ...). Patched copy: `weegfx.cpp`.
Tier 2 compiles only `weegfx.cpp`.

Replaced by `host/sim/host_firmware.cpp` (real `.cpp` is hardware-bound):
`OC::CORE::ticks`, `FreeRam()` (OC_core.cpp); `graphics` (display.cpp);
`TR1..TR4` (OC_gpio.cpp); `ADC_CHANNEL_1..4`, `FreqMeasure` (OC_ADC.cpp);
`DAC_CHANNEL_A..D`, `DAC::calibration_data_` (linear dummy), `values_`,
`get_voltage_scaling()` (OC_DAC.cpp); `HS::applet_is_hidden/get_applet_name/get_applet_icon`
(hemisphere_config.h registry, not compiled). Replaced in `host/sim/host_clocksetup.cpp`:
`ClockSetup_instance` (OC_apps.cpp), `OC::ui` + `Ui::_Poke()` (OC_ui.cpp),
`OC::DigitalInputs` statics, `Scan()` and `tr1..4_ISR()` (OC_digital_inputs.cpp).

## Likembe lab parity

`task host:parity` (`host/tools/parity.sh`) builds `host/tools/likembe_dump.cpp` (includes
`custom/core/likembe_core.h`, same g++ flags as the tests) and runs it and
`host/tools/likembe_lab_parity.mjs` (loads `docs/demo/likembe_engine.js`, the browser lab
engine) for a fixed set of seed/part/grid/Oct/Var/Pass combinations, diffing the event streams
(`bar pulse thumb degree rest accent pitch_units`). Exit non-zero on any mismatch. Needs `node`
(nvm's `nvm.sh` is sourced if `node` is not on PATH). Outputs in `host/build/parity/`.

## Adding a test for a new core

1. Keep the core in `custom/core/<name>_core.h`, plain C++17, no Arduino/O_C includes.
2. Add `host/tests/test_<name>_core.cpp` with `#include "custom/core/<name>_core.h"`;
   copy the `TEST`/`CHECK`/`RUN` macros from `test_hello_core.cpp`, return non-zero on failure.
3. `task host:test` picks it up (glob), flags `-O2 -std=c++17 -Wall -Wextra -Werror -I<repo>`.

To simulate a new applet: add it to `host/sim/host_applets.h` (see "Adding an applet to the host registry").

## Known gaps

- Host registry only (`host_applets.h`), one applet per hemisphere; no `HemisphereManager`,
  presets, config menu, MIDI in/out, EEPROM (the ClockSetup screen is available).
- Internal clock: no MIDI clock; shuffle, boops, trigger-map sources and skips only via the
  ClockSetup screen (no script options).
- Outputs are read from `frame.ViewOut()` (pitch units), not DAC codes; the
  DAC calibration is a dummy.
- `OC::ADC` returns the scripted value directly: no smoothing, offset, noise.
- `millis()` advances only via the ISR loop (60 us per tick, integer).
- Firmware TUs (incl. `sim_core.cpp`) compile with `-w` (upstream code is noisy
  on clang); `sim_main.cpp`/`ui_main.cpp` use `-Wall -Wextra`; tests use `-Werror`.
- Interactive mode: no mouse, no encoder long-press (left long release = clock run on
  hardware; use `I`), no select mode.

## Next steps

- Tick `HemisphereManager` for select mode in the UI.
- Golden-image tests: compare `sim.pbm` against checked-in references.
