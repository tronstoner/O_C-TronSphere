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

## 2. Our rule for applets (decided 2026-10-20, revised the same day)

Intent: **one clock division, not two layers.** With `Src Int` the applet's `Div` row *is*
the ClockSetup slot multiplier: one value, shown and edited in both places. Both applets
start when the internal clock starts. Few, musical values.

- **`Src Int`** = `Clock(0)` semantics for the internal clock only: step when
  `HS::clock_m.IsRunning() && HS::clock_m.Tock(io_offset)` — the tock of this hemisphere's
  first ClockSetup slot (left = slot 1 / channel 0, right = slot 3 / channel 2; the same
  index `IOFrame::Load` uses as `virt_chan`, `HemisphereApplet.cpp:120-126`). The jack /
  trigger map is **not** used (no fallback to `trigmap[].Clock()` while stopped or "off").
  Runs whenever the internal clock runs (also when it follows external sync on TR1).
- **`Div` with `Src Int`** reads `HS::clock_m.GetMultiply(io_offset)` and the encoder writes
  `HS::clock_m.SetMultiply(m, io_offset)` immediately. Curated list (`kIntMults`,
  ClockManager terms −3, −2, −1, 1..8, 12):

  | Shown | /4 | /3 | /2 | x1 | x2 | x3 | x4 | x5 | x6 | x7 | x8 | x12 |
  |---|---|---|---|---|---|---|---|---|---|---|---|---|
  | Note | 1/1 | – | 1/2 | 1/4 | 1/8 | 1/8T | 1/16 | – | 1/16T | – | 1/32 | 1/32T |

  Display: ClockSetup notation, then the note name when known (`kMultNotes`), e.g.
  `Div x4 1/16`; `x12` shows no note name (does not fit 64 px). A value set in ClockSetup
  that is not in the list (e.g. `x9`, `/32`, `off`) is shown as is; the next encoder step
  snaps to the nearest list entry in that direction (`custom::StepIntMult`).
- **The applet writes to ClockSetup.** On load (`Start()`, after `OnDataReceive`) and when
  `Src` is switched to `Int`: if the slot multiplier is 0 (off), the applet sets x4
  (`kDefaultIntMult`). The value is persisted by ClockSetup (its `OnDataRequest`, 6 bits per
  slot, `ClockSetup.h:252/278`), not by the applet. If ClockSetup's own preset data loads
  after the applet, ClockSetup's stored value wins (also "off": the applet then does not step
  until the slot is set again — not observed in the sim, check on hardware).
- **`Src TRn`** = the physical jack. `Src TRn`, `Roll TRn`, `Rst TRn` read the rising edge
  of `HS::frame.gate_high[n-1]` (raw level, `HemisphereApplet.cpp:87-90`), edge state kept in
  the applet. **`Div`** = jack pulses per engine step from `kTrDivs` = 1 2 3 4 5 6 7 8 12 16
  (index persisted, default 1; decrement counter `custom::PulseDivider`, fires on the first
  pulse after a reset).
- **Roll / Rst inputs per applet**: an applet may offer its own trigger inputs for actions
  (Likembe: `Roll` re-roll, `Rst` restart at bar start; values `Off`, `TR1`..`TR4`). Several
  actions on the same input all fire. Defaults identical on both sides.
- Applets read `HS::clock_m` (`IsRunning()`, `Tock()`, `GetMultiply()`); the only write is
  `SetMultiply()` on their own slot (`io_offset`), as described above.
- **Clock start always restarts the sequence**: upstream calls `Reset()` on every applet when
  the clock starts (auto_reset, 1.2); kept as is. `Rst` adds a trigger input for restarts.
- Likembe is the reference implementation: Clock page (`docs/modules/likembe.md`).
- Superseded the same day: `Src Int` = 24 PPQN `Tock(MIDI_CLOCK)` with an applet-side
  `div_int` (two division layers; slot multipliers ignored).

Sections 1.1-1.3 describe upstream routing for upstream applets; with `Src Int` our applets use the slot's tock (1.1) but never the trigger map.

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
`likembe_int_stopped_tr1_ignored`, `likembe_int_div_from_clocksetup`,
`likembe_int_div_from_applet`, `likembe_src_tr1_div3`,
`likembe_src_tr1_jack_override`, `likembe_rst_clockstart`, `likembe_rst_tr2`,
`likembe_ext_sync_tr1`.
