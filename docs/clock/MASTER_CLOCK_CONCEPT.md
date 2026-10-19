# Master Clock — Concept

Status: shelved 2026-10-19 (user decision). Reference only. Paths relative to `firmware/software/src/`; line numbers
refer to the vendored `T32` snapshot.

## 0. Assessment: is the rework needed?

Status: **shelved 2026-10-19 (user decision). Reference only.**

**Already works without core changes**
1. ClockSetup per-input-channel mult/div (x24 .. /32) on the internal clock, via the
   both-buttons gesture (`APP_HEMISPHERE.h:671`); costs no slot.
2. Any applet can divide its incoming clock internally (one integer parameter + counter).
3. Applets can read `HS::clock_m` directly: `GetTempo()` (`HSClockManager.h:135`),
   `IsRunning()` (`:318`), `Tock(ch)` (`:335`), `EndOfBeat(ch)` (`:344`) — e.g. an LFO with a
   period in beats.

**What a rework would add**
- Bar/cycle phase — not derivable from pulses alone; needed for phase-locked LFOs and for
  realigning polymeters without a reset trigger.
- Free n/d ratios (3:2) on one channel.
- Global instead of per-preset clock settings.
- CV tempo without spending a slot on Metronome.

**Minor fixes, independent of a redesign**
- The commented-out internal-clock division in `CVInputMap.h:141-144` ("beat_tick gets reset").
- Euclid settings of the trigger map are not persisted (`CVInputMap.h:213-220`).

**Recommendation (pending user decision):** defer the bus rework. The first module reads
`HS::clock_m` directly and derives its own division from one parameter. Revisit after that
module runs on hardware; if needed, the smallest useful change is a bar-phase accessor plus
the division fix (tens of lines), with the bus design below as fallback.

## 1. Current state (upstream T32)

### ClockManager — `HSClockManager.h`
- Singleton `HS::clock_m`, `class ClockManager` (`:46`). Holds tempo (`ticks_per_beat`,
  BPM from tap via `SetTempoFromTaps` `:114`), shuffle, external sync PPQN
  (`SetClockPPQN` `:100`), MIDI clock in (`MIDI_CLOCK_PPQN = 2`) and out (`MIDI_OUT_PPQN = 24`) (`:41-42`).
- Channels: `enum ClockOutput` (`:48-59`) = `LEFT_CLOCK1, LEFT_CLOCK2, RIGHT_CLOCK1,
  RIGHT_CLOCK2, LEFT2_*, RIGHT2_*, MIDI_CLOCK` → `NR_OF_CLOCKS = 9`. The first four are the
  trigger inputs of the two applet slots on Teensy 3.2.
- Per channel: `int8_t tocks_per_beat[ch]` (`:75`) — `>0` multiply, `<0` divide (`-1` = /2,
  `-2` = /3, ...), `0` = off. `SyncTrig()` (`:190-245`) computes `tock[ch]` every tick from
  `BeatTick()`; multipliers reset when all have met their count; shuffle shifts odd tocks.
- `Tock(ch)` (`:335`) returns this tick's tock. Applets do not subscribe.

### Delivery to applets — `HemisphereApplet.cpp:117-140` (`HS::IOFrame::Load`)
For each applet input channel: if the clock runs and `GetMultiply(ch) != 0`, `clocked[ch]
= clock_m.Tock(ch)`; otherwise `clocked[ch] = trigmap[ch].Clock()`. Then skip probability
(`CheckSkip`) and "boops" (manual triggers). So the internal clock **replaces** the
physical trigger of that channel. Code TODO at `HemisphereApplet.cpp:123`: *"implement
div/mult within DigitalInputMap and get rid of this call to clock_m"*.

### Trigger map — `CVInputMap.h:96` `struct DigitalInputMap` (`HS::trigmap[]`)
Already has: a `CLOCK` source type (sources `-2` = 1 ppqn, `-1` = 4 ppqn, `:224-228`),
a per-channel `ClkDivMult div_mult` (`util/clkdivmult.h`, range `-24..64`), and an
Euclidean filter (`e_length, e_beats, e_rotate`, `:115`, `:179-183`). The CLOCK source
derives its gate from `BeatTick()` phase; dividing it is commented out because
"this doesn't work for larger divisions, because beat_tick gets reset" (`:141-144`).
Persisted per channel as 16 bits (source + div_mult, `:213-220`); Euclid params are **not** persisted.

### UI — `applets/ClockSetup.h`
Built-in overlay (not a slot), opened by pressing both buttons (`APP_HEMISPHERE.h:671`),
closed by any single button release (`:663`). Cursor pages (`:29-54`): play/stop, tempo,
shuffle, PPQN, `MULT1-4`, `TRIG1-4` (trigger map source), `BOOP1-4`, `INSKIP1-4`, `OUTSKIP1-4`.

### Persistence
`ClockSetup::OnDataRequest` (`:247-268`) packs tempo (9 b), shuffle (7), 4 × mult (6 b each,
offset +32), PPQN (5), screensaver (4), autosave, cursor wrap, trig length (7) → bits 0..57,
VOR adds 58..59: **≤ 6 bits free**. Stored per preset in `HEMISPHERE_CLOCK_DATA1..4`
(`APP_HEMISPHERE.h:57-60, 108-117`). `GetGlobals()` (`:298-306`, in/out skip, 8 bits free) is,
despite its name, also stored per preset (`APP_HEMISPHERE.h:296-299`).

### Metronome applet — `applets/Metronome.h`
Uses `clock_m.Tock(hemisphere*2)` and CV to set tempo — the "sacrifice a slot" path for CV
tempo control.

## 2. Pain points

1. One ratio per **input channel**, bound to a slot position, not to a musical meaning.
2. Integer ratios only (n× or /n of the beat). No n:d polymeters (3:2, 5:4, 7:8).
3. No phase / bar position: an LFO cannot lock its cycle to "2 bars" and stay aligned.
4. No named shared clocks: two applets wanting the same /3 each need their own setting.
5. ClockSetup is dense (≈ 25 cursor stops); tempo is not one gesture away.
6. Clock settings follow presets; changing preset can change tempo/ratios.
7. CV tempo control costs an applet slot (Metronome).
8. Persistence word is full.

## 3. Proposed design: clock buses

### Data model (in `ClockManager`)
```
constexpr int CLOCK_BUS_COUNT = 4;            // proposal; see open questions
struct ClockBus {
  uint8_t num, den;     // ratio relative to the beat: tocks = beat * num / den
  uint8_t phase_off;    // 0..255 → fraction of the bus period
  int8_t  swing;        // optional, per bus (or global only)
  // runtime
  uint32_t period_q16;  // ticks per bus tock, Q16 fixed point (no FPU)
  uint32_t acc_q16;     // phase accumulator within the bus period
  uint32_t count;       // tocks since bar/reset
  bool     tock;        // this tick
};
```
- Phase accumulator per bus advanced every tick by `1 << 16` against `period_q16`; tock on
  wrap. All buses reset together on transport start / external reset / bar boundary
  (bar = LCM of active ratios, or a fixed "bar length" setting) so polymeters realign.
- External sync: buses derive from the measured beat period exactly as today's channels do.

### Ratio set
Curated list, not free n/d entry: `1/8, 1/4, 1/3, 1/2, 2/3, 3/4, 1, 4/3, 3/2, 2, 3, 4, 6, 8`
plus polymetric `5:4, 4:5, 7:8, 8:7, 5:3, 3:5` (reads as "n tocks per d beats"). Shown as
`3:2` text. The list lives in one constants table so the UI and persistence share an index.

### Accessors
- `bool Tock(bus)` — rising edge this tick.
- `uint16_t Phase(bus)` — 0..65535 within the current bus period (for synced LFOs / ramps).
- `uint32_t Count(bus)` — tock count since bar reset (for pattern step position).
- `uint32_t PeriodTicks(bus)` — current period, for applets that need lookahead (gate length, slew).

### Subscription via the input map
Extend `DigitalInputMap` source types with `CLOCK_BUS` (sources `-3..-(2+N)` or a new
range) so any applet's `Clock(ch)` can follow "CLK bus k" without applet changes. This
resolves the upstream TODO at `HemisphereApplet.cpp:123`: `IOFrame::Load` no longer
special-cases `clock_m`; the legacy `MULT1-4` path becomes "trigmap source = bus k".
Applets that want phase call `HS::clock_m.Phase(bus)` directly; our own applets get a
"bus" parameter in their 64-bit state.

### UI access
- Tempo one gesture away: proposal — in ClockSetup the first page is tempo with
  encoder = BPM, push = play/stop; buses on the next pages (one line per bus: ratio, phase).
- Faster gesture candidates (decide): (a) keep dual-press but land on tempo; (b) long-press
  one button; (c) a persistent tempo/bus strip in the header row of both hemispheres.
- Retire or hide pages we do not use (boops, out-skip) in our build behind a compile flag.

### Persistence
ClockSetup's word is full, so buses need new storage: add `HEMISPHERE_CLOCK_BUS_DATA*`
entries to `enum HEMISPHERE_SETTINGS` (`APP_HEMISPHERE.h:47-60`). Per bus ≈ 5 b ratio index
+ 4 b phase (16 steps) + 3 b swing = 12 b → 4 buses = 48 b, one extra `uint64_t`.
Changing the enum changes the EEPROM preset layout: bump the storage version and accept
that old presets reset (fine — this build has no legacy users).

### Global vs per preset
Proposal: tempo, buses and PPQN become **global** (not restored on preset change), presets
keep applet choices and applet state. Option: per-preset "recall clock" flag.

## 4. Upstream impact (merge-conflict map)

| File | Change | Conflict risk |
|---|---|---|
| `HSClockManager.h` | add `ClockBus[]`, `Tock/Phase/Count(bus)`, bus tick in `SyncTrig` | high (upstream touches clock often) |
| `CVInputMap.h` | `CLOCK_BUS` source type, icon, gfx label | medium |
| `HemisphereApplet.cpp` | `IOFrame::Load` clock path | medium |
| `applets/ClockSetup.h` | bus pages, pack/unpack | high |
| `APP_HEMISPHERE.h` | new settings entries, global clock storage, gesture | high |
| `HSUtils.cpp` | input-map label/drawing | low |

Mitigation: put the bus engine itself in `custom/core/clock_bus.h` (pure, host-tested) and
keep upstream edits to thin delimited call sites (see `docs/UPSTREAM.md` markers).

## 5. Phased implementation

1. **Core** `custom/core/clock_bus.h`: ratio table, Q16 accumulators, reset/realign, phase;
   host tests (drift over 10 min, polymeter realignment, tempo change mid-bar, ext. sync jitter).
2. **Engine hook**: ClockManager owns N buses, ticks them; `Tock/Phase/Count(bus)`.
   Verify on hardware with a scope / a trigger-out test applet.
3. **Subscription**: `CLOCK_BUS` trigmap source; legacy MULT path mapped to buses.
4. **UI**: tempo-first ClockSetup, bus pages, chosen gesture.
5. **Persistence + global**: new settings words, global clock decision applied.
6. Our modules (evolving voltage, LFO) consume `Count`/`Phase`.

## 6. Open questions (user)

1. Number of buses: 2, 4 (proposal) or 8?
2. Ratio list: is the curated list right? Free n/d needed?
3. Swing per bus, or one global shuffle?
4. Phase offset per bus: needed, and what resolution?
5. Bar length: fixed (e.g. 4 beats, settable) or LCM of active ratios?
6. Global clock settings vs per preset (proposal: global)?
7. Access gesture: (a) dual-press → tempo page, (b) long-press, (c) header strip?
8. CV tempo / CV ratio control without a slot — wanted? Which input would it use?
9. Keep MIDI clock in/out?
