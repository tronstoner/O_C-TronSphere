# Clock

How clock pulses reach an applet in this build, and the rules our applets follow.
Upstream is used as is (decision 2026-10-19, no clock rework). Line numbers refer to
`firmware/software/src/` at the vendored `T32` subtree.

## 1. Upstream: two layers

Every ISR tick (16666 Hz) `HSApplication::BaseController()` runs
`ClockSetup_instance.Controller()` first (`HSApplication.h:79`), then `HS::frame.Load()`.
`IOFrame::Load()` builds `frame.clocked[ch]` for the four hemisphere input slots
(`HemisphereApplet.cpp:117-140`; slot = `io_offset + ch`, left 0, 1, right 2, 3).
`HemisphereApplet::Clock(ch)` returns `frame.clocked[io_offset + ch]` (`HemisphereApplet.h:176`).

### 1.1 Trigger map (per input slot)

`HS::trigmap[slot]` is a `DigitalInputMap` (`CVInputMap.h:96`):

- **Source** (`source`, `CVInputMap.h:113`, `ChangeSource` :121): `-2` = internal clock
  1 PPQN (`CL1`), `-1` = internal clock 4 PPQN (`CL4`), `0` = none, then TR1..TR4,
  CV1..CV4 (gate threshold), outputs A..D, MIDI mappings (names `InputName()` :205,
  `OC_strings.cpp:75` `trigger_input_names_none`). Default: slot n ← TR(n+1)
  (`HSUtils.cpp:82`).
- **ClkDivMult** (`div_mult`, `util/clkdivmult.h`): `steps > 0` divides (counter, fires on the
  first of every `steps` clocks), `steps < 0` multiplies by interpolating from the last
  input interval. Range `CLOCKDIV_MIN = -24` .. `CLOCKDIV_MAX = 64`.
- **Euclidean filter** (`e_length`, `e_beats`, `e_rotate`; applied in `Clock()` :169-187).
- `Clock()` = rising edge of `Gate()`, then div/mult, then the Euclidean filter. It
  calls `Reset()` when `clock_m.auto_reset` is set (clock start).

Edited in the Hemisphere input config (zoom/config page, `APP_HEMISPHERE.h:759-767`) and on
ClockSetup's TRIG rows (`applets/ClockSetup.h:185-189`).

### 1.2 ClockManager (`HS::clock_m`, `HSClockManager.h`, instance `HemisphereApplet.cpp:4`)

- **Tempo**: `SetTempoBPM` (:108; ticks per beat = 1 000 000 / BPM, i.e. the ISR is treated
  as 1 M ticks/min), set by encoder on ClockSetup's TEMPO row (`ClockSetup.h:217-219`) or by
  **tap tempo** (button presses on TEMPO, average of taps, `ClockSetup.h:154-169`,
  `SetTempoFromTaps` :114).
- **External sync** on **TR1 only** (`ClockSetup.h:66`: `DigitalInputs::clocked<DIGITAL_INPUT_1>()`),
  or MIDI clock (2 PPQN). Expected pulses per beat = **Input PPQN** (`clock_ppqn`, default 4,
  :80, `SetClockPPQN` :100, ClockSetup row EXT_PPQN). In `SyncTrig()` (:190-288): with two
  previous pulses the tempo becomes `ppqn x` the **average of the last two intervals**
  (:257-261), and the beat is **nudged** towards the pulse when it lands within half a pulse
  of a beat (:263-273, `Nudge` :183). A gap > 2 beats / ppqn **auto-stops** and re-arms
  (paused) (:283-287). Armed (paused) clock starts on the next TR1 pulse (`ClockSetup.h:91-95`).
- **Start/Stop**: ClockSetup PLAY_STOP: stopped → armed (paused, waits for TR1) → running
  (`PlayStop`, `ClockSetup.h:332-339`); long release of the left encoder toggles run
  (`APP_HEMISPHERE.h:875`). `Start()` (:290) calls `Reset()` and sets `auto_reset`;
  the Hemisphere app then calls `Reset()` on **both applets** on the next tick
  (`APP_HEMISPHERE.h:450-458`) and the trigger maps reset their counters.
- **Per-slot multiplier** `tocks_per_beat[slot]` (`SetMultiply` :94): `1..24` = x1..x24,
  `-1..-31` = ÷2..÷32 (`CLOCK_MAX_MULTIPLE`/`CLOCK_MIN_MULTIPLE` :43-44), `0` = off.
  Multiplied tocks fall on a grid from the beat start (`:209-221`, optional shuffle on odd
  tocks); divisions fire on every (1 − m)-th beat (`:222-237`). Edited on ClockSetup's MULT
  rows (`ClockSetup.h:224-228`, left encoder doubles/halves :234-243) or in the input config
  while the clock runs (`APP_HEMISPHERE.h:763-765`).
- **Which one wins** (`HemisphereApplet.cpp:123-129`): while `clock_m.IsRunning()` and the
  slot's multiplier is not 0, the slot gets the **internal tocks**; otherwise the slot gets
  its **trigger map** `Clock()`. Either way the input skip filter (`CheckSkip`) applies.
- **Manual boops**: ClockSetup BOOP1..4 (`ClockSetup.h:146-149`, `Boop` :323) inject one
  pulse into a slot; `Load()` ORs it in (`HemisphereApplet.cpp:132`).

### 1.3 Multiplier → note value (one beat = a quarter note)

| Multiplier | Note | Multiplier | Note |
|---|---|---|---|
| x1 | 1/4 | ÷2 | 1/2 |
| x2 | 1/8 | ÷3 | 1/2. (dotted) |
| x3 | 1/8T | ÷4 | 1/1 |
| x4 | 1/16 | ÷6 | 1/1. (dotted) |
| x6 | 1/16T | ÷8 | 2/1 (2 bars) |
| x8 | 1/32 | ÷16 | 4/1 |
| x12 | 1/32T | ÷32 | 8/1 |
| x16 | 1/64 | | |
| x24 | 1/64T | | |

Other values have no common note name. The browser lab uses the same labels.

## 2. Our rule for applets (decided 2026-10-20)

Intent: consistent, predictable clock behaviour, identical on both hemispheres,
independent of the ClockSetup slot multipliers and the trigger map.

- **`Src Int`** = the internal clock's **24-pulses-per-beat** tock:
  `HS::clock_m.IsRunning() && HS::clock_m.Tock(ClockManager::MIDI_CLOCK)`
  (`HSClockManager.h:57`, `tocks_per_beat[MIDI_CLOCK] = MIDI_OUT_PPQN = 24` :42, :75). `SyncTrig()`
  computes it every tick while the clock runs, independent of the slot multipliers and of
  `midi_out_enabled` (only `MIDITock()` checks that). Runs whenever the internal clock runs
  (also when it follows external sync on TR1), on both sides; stops when it stops.
  `Clock(0)` / `frame.clocked[]` are **not** used. **ClockSetup slot multipliers (MULT rows)
  and the trigger map do not affect our applets.**
- **`Src TRn`** = the physical jack (decided 2026-10-20). `Src TRn`, `Roll TRn`, `Rst TRn`
  read the rising edge of `HS::frame.gate_high[n-1]`, the raw level of jack TRn read every
  tick with `OC::DigitalInputs::read_immediate` (`HemisphereApplet.cpp:87-90`), edge state
  kept in the applet.
- **`Div`** = pulses per engine step, for every source (decrement counter, fires on the first
  pulse after a reset; `custom::PulseDivider`, `custom/core/likembe_clock.h`). Stored per
  source kind: `div_int` (1..24, default 6 = 1/16) and `div_tr` (1..16, default 1); the `Div`
  row edits the one for the current `Src`, so switching `Src` keeps each tempo.
- **Note names** with `Src Int` (one beat = a quarter note, 24 pulses):

  | Div | 24 | 16 | 12 | 8 | 6 | 4 | 3 | 2 |
  |---|---|---|---|---|---|---|---|---|
  | Note | 1/4 | 1/4T | 1/8 | 1/8T | 1/16 | 1/16T | 1/32 | 1/32T |

  Other values show only the number. Constants: `kDivNotes`, `kIntPpqn` (`likembe_constants.h`).
- **Roll / Rst inputs per applet**: an applet may offer its own trigger inputs for actions
  (Likembe: `Roll` re-roll, `Rst` restart at bar start; values `Off`, `TR1`..`TR4`). Several
  actions on the same input all fire. Defaults identical on both sides.
- Applets may read `HS::clock_m` (`IsRunning()`, `Tock()`), never write it.
- **Clock start always restarts the sequence**: upstream calls `Reset()` on every applet when
  the clock starts (auto_reset, 1.2); kept as is. `Rst` adds a trigger input for restarts.
- Likembe is the reference implementation: Clock page (`docs/modules/likembe.md`).

Sections 1.1-1.3 describe upstream routing for upstream applets; our applets bypass it.

## 3. Host simulator coverage

`host/sim` compiles the real `HSClockManager.h` (`clock_m`) and the real
`applets/ClockSetup.h` and runs `ClockSetup::Controller()` every tick before `frame.Load()`,
with the Hemisphere `auto_reset` → `Reset()` step (`docs/HOST_SIM.md` "Internal clock").
Covered: tempo, start / arm / stop, per-slot multiplier, Input PPQN and external sync on TR1
(tempo averaging, nudge, auto-stop are upstream code paths that run), trigger map on the
default sources. Not covered: the ClockSetup **UI** (its `View()`/encoder handling is compiled
but not reachable: no Up+Down overlay), tap tempo, shuffle and boops (no sim commands), MIDI
clock, skip filters (default off), trigger-map source/div/Euclid editing (defaults only).
Scenarios: `task host:sim` (no arguments) runs `likembe_int_both_sides`,
`likembe_int_stopped_tr1_ignored`, `likembe_int_div_note`, `likembe_src_tr1_div3`,
`likembe_src_tr1_jack_override`, `likembe_rst_clockstart`, `likembe_rst_tr2`,
`likembe_ext_sync_tr1`.
