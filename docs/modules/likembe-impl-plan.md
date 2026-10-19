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
struct LikembeParams { uint8_t density_pct, part, root; int8_t octave; uint8_t grid16, variation_pct, passage_idx; uint16_t seed;
  static LikembeParams Defaults(uint8_t part); void Clamp(); };
struct LikembeEvent { bool note; uint8_t degree; int16_t pitch_units; bool accent; uint8_t thumb; };
struct LikembeCore {
  void Init(const LikembeParams&);        // regenerate from seed
  void SetParams(const LikembeParams&);   // density/variation/passage/root/octave immediate; grid/part/seed -> regen at bar line
  void SetDensityMod(int8_t pct_delta);   // CV1, effective clamped 0..100
  uint8_t EffectiveDensity() const;
  LikembeEvent Pulse();                   // play one pulse, bar-line work on wrap
  LikembeEvent EventAt(uint8_t i) const;  // event of pulse i of the current bar (display)
  uint8_t CurrentElement(uint8_t th) const;
  void Reroll();                          // pending, at next bar line
  void RerollNow();                       // immediate
  void Reset();                           // position + phases to 0, figures kept
  bool ThemeDiffers() const;
  uint64_t Pack() const; void Unpack(uint64_t); static LikembeParams UnpackParams(uint64_t);
  // public state: params, pulses_per_bar, pulse_idx, bar_in_passage, reroll_pending,
  // regen_pending, thumb[2], theme[2], base_midi, key_lo, key_hi
};
```

`sizeof(LikembeCore)` = 104 bytes (host, 64-bit). Glue note: after a re-roll the core
changes `params.seed`; the glue must take its settings from `core.params` (not hold a stale
copy and push it back via `SetParams`, which would regenerate the old seed).

## Decisions taken in the core

- **Re-roll** = new seed (drawn from the passage stream) + new passage with *forced* new loop
  lengths under the policy + regenerated figures (new theme), applied at the next bar line
  (`reroll_pending`). `RerollNow()` is the immediate variant.
- **Density is live**: each element stores its rest draw (0..255); rest iff
  `j > 0 && draw*100 >= density*256`. Changing density/CV1 changes rests immediately and
  consumes no randomness; it is the same decision as the spec's `random > ρ` at generation.
- `active_part` holds the part the figures belong to; a pending part change does not move offset/range/pitch before the bar line.
- Grid/part/seed changes regenerate from the seed at the next bar line; root is immediate.
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
- 2026-10-19, Bass at 0 % (16 grid, loops 4/3): expected L 2 + R 8/3 = 14 notes per 3 bars; observed 5, 5, 4 per bar = 14, and 56 in 12 bars.

## Out A velocity gate (2026-10-19)

- Out A changed from a ~1 ms `ClockOut` trigger to a plain `Out(0, level)` velocity gate in the
  glue (`Likembe.h` Controller): tick counter since last TR1 pulse (clamped 10 s), countdown
  to `Out(0, 0)` at interval × 50 %. Core unchanged, no new core test.
- `--log-events` detects ClockOut/GateOut only and no longer fires for Out A (expected);
  use `--log-outputs` instead.
- Sim (`--clock 100:4 --seconds 6 --log-outputs`): 35 gates in 40 pulses (rests stay 0 V),
  12 at 7680 (5 V), 23 at 4608 (3 V), no other levels; every gate 75.00 ms (±0.06 ms) except
  the first (62.5 ms = default 125 ms interval before the first measurement).

## Open questions for the user

1. Re-roll at next bar line (implemented default) vs immediate (`RerollNow()` exists)?
3. CV2: variation-rate modulation, re-roll gate, or root transpose? (currently unassigned)
7. Part change: reload the part's default Density/Var, or keep the user's values (current)?
8. Seed edit: random new seed per turn (current) vs stepping the seed number?
4. Two instances: share bar sync / hocket later via a shared static (bar phase, bass notes)?
5. Provisional Mi-solo/Solo profiles: anchors, ranges, policies, offsets OK as a start?
6. Persist tuning once more tunings are enabled (1–2 bits free in the layout)?
