# Likembe — implementation plan

Module doc: `likembe.md`. Spec: `docs/research/Likembe pattern engine technical concept.md`.
Reference: `docs/research/Congotronics line generator.html` (pattern logic ported; audio not).

## Steps

1. [x] Core `custom/core/likembe_core.h` + constants `likembe_constants.h`.
2. [x] Host tests `host/tests/test_likembe_core.cpp` (14 tests, `task host:test`).
3. [x] Glue `custom/applets/Likembe.h` (ID 201, I/O, cursor controls, Pack/Unpack).
4. [x] Visualization (pulse row + cursor, two thumb loops with current element, contour, status).
4a. [x] Bar indicator: beat ticks under the pulse row, realignment counter `b<i>/<N>` (core `cycle_bars`/`cycle_bar_idx`, test `cycle_bars_and_index`).
4b. [x] Display redesign: kalimba tine view (struck tine while gate high, accent tip, last-note foot), bar progress line + beat ticks, bar-cycle squares (segment fallback N > 12), pixel status glyphs; replaces pulse row, cursor, contour and `b<i>/<N>` text. Follow-up: tines shortened to 12 px, step row with running light (inverted current pulse) and beat ticks restored instead of the progress line.
5. [x] Sim verification (`task host:sim`, `task host:ui --headless` screenshots).
6. [x] Density 0–100 % (no floor); `Oct` parameter −2…+3 after Root, persisted in bits 40–42.
7. [ ] Hardware: flash, clock from TR1, tune provisional profiles by ear, `task size` delta.

## API (as implemented, namespace `custom`)

```cpp
struct LikembeParams { uint8_t density_pct, part, root; int8_t octave; uint8_t len, variation_pct, passage_idx; uint16_t seed;
  static LikembeParams Defaults(uint8_t part); void Clamp(); };
struct LikembeEvent { bool note; uint8_t degree; int16_t pitch_units; bool accent; uint8_t thumb; };
struct LikembeCore {
  void Init(const LikembeParams&);        // regenerate from seed
  void SetParams(const LikembeParams&);   // everything immediate; len/part/seed -> regen now, bar position kept
  void SetDensityMod(int8_t pct_delta);   // CV1, effective clamped 0..100
  uint8_t EffectiveDensity() const;
  LikembeEvent Pulse();                   // play one pulse, bar-line work on wrap
  LikembeEvent EventAt(uint8_t i) const;  // event of pulse i of the current bar (display)
  uint8_t CurrentElement(uint8_t th) const;
  void Reroll();                          // immediate (TR2)
  void RerollNow();                       // same as Reroll()
  void Reset();                           // position + phases to 0, figures kept
  bool ThemeDiffers() const;
  uint64_t Pack() const; void Unpack(uint64_t); static LikembeParams UnpackParams(uint64_t);
  // public state: params, pulses_per_bar, pulse_idx, bar_in_passage,
  // thumb[2], theme[2], base_midi, key_lo, key_hi
};
```

`sizeof(LikembeCore)` = 104 bytes (host, 64-bit). Glue note: after a re-roll the core
changes `params.seed`; the glue must take its settings from `core.params` (not hold a stale
copy and push it back via `SetParams`, which would regenerate the old seed).

## Decisions taken in the core

- **Re-roll** = new seed (drawn from the passage stream) + new passage with *forced* new loop
  lengths under the policy + regenerated figures (new theme), applied immediately (decided
  2026-10-19; pulse position kept, phases restart at 0). `RerollNow()` is the same call.
- **Density is live**: each element stores its rest draw (0..255); rest iff
  `j > 0 && draw*100 >= density*256`. Changing density/CV1 changes rests immediately and
  consumes no randomness; it is the same decision as the spec's `random > ρ` at generation.
- `active_part` holds the part the figures belong to (updated on regeneration, i.e. immediately on a part change).
- Len/part/seed changes regenerate from the seed immediately (decided 2026-10-19); `pulse_idx` is kept (wrapped to the new Len) and `bar_in_passage` is kept, so the clock grid never jumps; phases restart at 0. Root is immediate.
- ~~Pulse clock source `Clk`~~ replaced 2026-10-20 by the Clock page (below); `div` and
  `clock_src` removed from `LikembeParams`.
- Integer RNG: two xorshift32 streams, part stream = seed + 7919·part, passage stream =
  seed + 31, seeded via multiply/xorshift mix with a non-zero fallback. Probabilities
  against 16-bit draws; weighted picks with integer weights (`kStepWeight`, `kMutWeight`).
- Variation rate in percent: bass 0.075 → 8 %.

## Differences doc ↔ JS (JS followed)

- **Octave mutation** headroom: JS allows `d + n <= hi + 2` (doc: within range) →
  `kOctaveHeadroom` = 2. With n = 5 an octave shift flips parity, so after an octave
  mutation a thumb may own a degree of the other parity (both doc and JS do this; the
  ownership test checks fresh themes only).
- **Key mutation** on a degree outside the thumb's keys (after octave): JS `indexOf` = −1,
  so the new key becomes keys[0]; ported as is.
- **Loop growth** appends elements with rest = false (doc silent).
- **Loop-length mutations persist** into the next passage's regeneration (JS writes them back
  to the loop setting); ported (current `thumb[].n` is reused unless the passage rolls new lengths).
- **Density change** in JS regenerates the part; here density is a live threshold (see above).
- Return-to-theme compares keys and rests only, not loop length (JS); ported, but the core
  compares loop length too (a length change always counts as differing).
- One part per instance: hocket/overlap, register shift, energy build not ported.

## Firmware translation

- JS renders a whole bar at bar start (`unroll`); the core evaluates each pulse lazily
  (`EventAt`) from the same state — equivalent because figures only change at bar lines.
- JS schedules by audio time; here one external trigger = one pulse (event driven). Missing
  or extra clocks shift the bar position; `Reset()` re-aligns.
- Bar-line work (mutation, regeneration, ≤ 8 elements × 2 thumbs) runs inside the
  `Pulse()` call from the glue's clock handler — small, bounded, no allocation.
- Probabilities are integer; distributions match the JS within rounding, sequences do not.

## Sim verification (2026-10-12, 100 bpm, 4 pulses/beat = 0.15 s pulses)

- Out A triggers only on clock pulses (max deviation 0.12 ms = tick jitter); every Out B change coincides with an Out A trigger (0 exceptions).
- Bass default (85 %): 14 notes / 16 pulses per bar.
- `--trig 2@3.0` (bar 2): rhythm and pitches change exactly at the start of bar 3 (t = 4.8 s), not before.
- `--in0 -4608` (−3 V: 85 − 35 = 50 %): 9–10 notes per bar.
- 2026-10-19, Bass at 0 % (Len 16, loops 4/3): expected L 2 + R 8/3 = 14 notes per 3 bars; observed 5, 5, 4 per bar = 14, and 56 in 12 bars.

## Out A velocity gate (2026-10-19)

- Out A changed from a ~1 ms `ClockOut` trigger to a plain `Out(0, level)` velocity gate in the
  glue (`Likembe.h` Controller): tick counter since last TR1 pulse (clamped 10 s), countdown
  to `Out(0, 0)` at interval × 50 %. Core unchanged, no new core test.
- `--log-events` detects ClockOut/GateOut only and no longer fires for Out A (expected);
  use `--log-outputs` instead.
- Sim (`--clock 100:4 --seconds 6 --log-outputs`): 35 gates in 40 pulses (rests stay 0 V),
  12 at 7680 (5 V), 23 at 4608 (3 V), no other levels; every gate 75.00 ms (±0.06 ms) except
  the first (62.5 ms = default 125 ms interval before the first measurement).

## Len / Div replace Grid (2026-10-19, user decision)

- `Grid` (16/12) split into `Len` (pulses per bar 1..32 since 2026-10-20, was 2..32; default 16) and `Div` (clock
  division 1..8, default 1). Core: `params.len` drives `pulses_per_bar`, bar line, cycle,
  `FitMask` (2n | Len; fewer than two fitting lengths -> all lengths fit), default loops
  `loops16`/`loops12` at Len 16/12, otherwise `RollLoopLengths()` from the passage stream at
  regeneration. `params.div` is persisted but used only by the glue: `DivFire()` fires on the
  first of every Div input clocks with one decrement per clock (no division per tick); gate
  length measured between engine pulses. `Reset()` re-arms (next clock fires).
- Pack re-laid out (no presets existed): len−2 at 13–17, div−1 at 18–20, rest shifted, 54 bits.
- Menu: Dens, Var, Part, Root, Oct, Len, Div, Roll, Seed, Clk. Step row: 4 px cells up to
  16 pulses, 2 px above; beat ticks every 4 pulses (`kBeatPulses`).
- Tests: `Make()` takes Len; `loop_policies_and_unequal`, `phase_advance_per_bar` now loop
  Len 12/16/24/32; `params_live_immediate`, `pack_unpack` (new bit positions, Len/Div clamp),
  `cycle_bars_and_index` use `len`; new `len_sweep` (Len 1..32 x 4 parts). Parity: `--grid` ->
  `--len`, 16 combinations incl. Len 2, 7, 12, 16, 24, 32.
- Verified: host tests, parity, sim screenshots Len 12/16/24/32, sim Div 2 at 120:4 (engine
  pulse 250 ms, gate 125 ms). Lab Len/Div UI: script syntax-checked only, not run in a browser.

## Clock page replaces Div and Clk (2026-10-20, user decision; done)

- Main list Dens, Var, Part, Len, Root, Oct, Roll, Seed, `Clock >`; push on `Clock` opens an
  applet-drawn page (own cursor) with Src (Int/TR1-4), Div (1-24, TRn only, `-` with Int),
  Roll (re-roll input Off/TR1-4, default own 2nd input), Rst (Off/TR1-4, default Off), Back.
  2026-10-20: TRn = physical jack (rising edge of `frame.gate_high[n-1]`), `Rst Int` removed.
  `AuxButton` while editing a page row also returns. Rules: `docs/CLOCK.md`.
- Glue-side settings `custom::LikembeClock` + `custom::PulseDivider` in
  `custom/core/likembe_clock.h` (host-testable; core never sees them). Core pack re-laid out
  to bits 0–47 (`kPackBits`), clock page at 48–61 (2026-10-20: 48–63); no presets existed.
- Glue order per tick: jack edges (4 bits from `frame.gate_high`) → Roll input → Rst TRn
  → step ((Src Int = 24 PPQN tock, TRn = jack edge) && divider). `Reset()` also drops
  the step-interval measurement (`have_pulse = false`): found by the former `likembe_rst_int` sim
  scenario, the first gate after a restart was 50 % of the stop gap and swallowed notes.
- Finding: upstream auto_reset already calls `Reset()` on every applet on clock start; kept,
  so clock start always restarts and `Rst` only adds a trigger input (`Int` value dropped;
  stored code 1 of old presets unpacks as Off).
- 2026-10-20 (user decision): `Src Int` = 24 PPQN internal clock
  (`clock_m.IsRunning() && clock_m.Tock(MIDI_CLOCK)`), not `Clock(0)`: identical on both
  hemispheres, ClockSetup slot multipliers / trigger map have no effect. `Div` applies to
  every source, two stored values (`div_int` 1..24 default 6 = 1/16, `div_tr` 1..16 default 1),
  note names with Src Int. Defaults equal on both sides (`Roll TR2`). Clock block re-laid out
  to bits 48–63 (base-5 Src/Roll/Rst field; `div_tr` limited to 16 for lack of bits).
- 2026-10-20 (user decision, supersedes the 24-PPQN entry above): one clock division, not two
  layers. `Src Int` = `clock_m.IsRunning() && clock_m.Tock(io_offset)` (this hemisphere's
  ClockSetup slot, never the jack); the Clock page `Div` with Src Int reads/writes
  `clock_m.GetMultiply/SetMultiply(io_offset)` over `kIntMults` (/4 /3 /2 x1..x8 x12, note
  names `kMultNotes`, off-list values shown as is and snapped on the next step); applet sets
  x4 when the slot is off on `Start()`, after `OnDataReceive` and on Src→Int; ClockSetup
  persists the value. `div_int` and the MIDI_CLOCK path removed. Src TRn Div = `kTrDivs`
  (1..8, 12, 16) index, 4 bits. Clock block 13 bits at 48–60 (src 3, roll 3, rst 3, idx 4).
  Lab: per-voice slot multiplier select in the Clock section = the voice's Int Div.
- 2026-10-20 (user decision): one multiplier list everywhere. `custom/core/clock_mult_list.h`
  (`clock_mult::kList` off /4 /3 /2 x1..x8 x12, `IndexOf`, `Step`, `Label`) replaces
  `kIntMults`/`FormatMult`; note names (`kMultNotes`) removed. ClockSetup.h edited under
  `CUSTOM_BUILD`: both encoders step a slot multiplier through the list (`docs/UPSTREAM.md`).
  Applet Src Int Div = `Step(m, dir, kIntMultLo)` (off excluded); `Div` value at x 34 like
  the other rows. Src TRn `kTrDivs` = 1..8, 12 (16 dropped). Lab: same list, no note names.
- 2026-10-20 (user decision): list widened to off /32 /24 /16 /12 /8../2 x1..x8 x12 x16 x24
  (23 entries; Src Int 22 without off). Src TRn `kTrDivs` = 1..8, 12, 16, 24, 32 shown `/n`
  via `clock_mult::DivLabel` (`/1` special case); indices 0..8 unchanged, so old presets keep
  their Div. Lab: same lists, TR Div `/n` (`formatDiv`). Sim: `clocksetup_mult_list` (/32 shot,
  22 steps to x24, clamp, back to off), new `likembe_int_div_x24`, `clocksetup_screen` 14 steps to x3.
- Tests: `clock_pack_unpack` (incl. `clock_mult::Step/IndexOf/Label`, `StepIntMult`), `pulse_divider` (host); sim scenarios `likembe_int_both_sides`,
  `likembe_int_stopped_tr1_ignored`, `likembe_int_div_from_clocksetup`, `likembe_int_div_from_applet`,
  `likembe_src_tr1_div3`, `likembe_src_tr1_jack_override`, `likembe_rst_clockstart`,
  `likembe_rst_tr2`, `likembe_ext_sync_tr1`, `clocksetup_mult_list` (real ClockSetup in
  the sim); lab (2026-10-20: BPM + Start/Stop + slot multipliers shared with the voice Div; node syntax check only): earlier ClockSetup model verified in headless Chrome (step counts for x4, steady
  TR1 / Div 3, manual pulses, Rst restart; at the time with Rst Int, not re-run after the 2026-10-20 TRn change) with a stubbed AudioContext.

### Firmware translation (lab → glue)

- The lab schedules 120 ms ahead in audio time and evaluates one IOFrame instant per event
  time; the firmware polls every tick. Same order of Roll / Rst / step per instant.
- The lab's internal clock is a grid model (`t0 + k·beat/m`); upstream `SyncTrig()` has
  integer ticks, shuffle and external-sync nudging. Step order is identical, timing is not.
- Lab TRn inputs are event pulses (TR buttons / TR test clock); the firmware detects rising
  edges of the polled jack level. Same result for pulses longer than one tick.

## Open questions for the user

1. ~~Re-roll at next bar line vs immediate?~~ Decided 2026-10-19: immediate (all setting changes immediate).
2. ~~Clock input choosable?~~ Done 2026-10-19 (`Clk`), replaced 2026-10-20 by the Clock page.
3. CV2: variation-rate modulation, re-roll gate, or root transpose? (currently unassigned)
7. Part change: reload the part's default Density/Var, or keep the user's values (current)?
8. Seed edit: random new seed per turn (current) vs stepping the seed number?
4. Two instances: share bar sync / hocket later via a shared static (bar phase, bass notes)?
5. Provisional Mi-solo/Solo profiles: anchors, ranges, policies, offsets OK as a start?
6. Persist tuning once more tunings are enabled (1–2 bits free in the layout)?
