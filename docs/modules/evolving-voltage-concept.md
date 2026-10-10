# Evolving Voltage — Concept

Status: **concept.** Nothing implemented; open questions at the end.
Working name TBD — candidates: **Vestige** (keeps the lineage), **Drift**, **Erode**. ≤ 9 chars.

## 1. Idea

A Hemisphere applet that emits an evolving, randomised **trigger pattern** and a matching
**CV stream**, locked to a master-clock bus, controlled by **one main knob/CV: error amount
`L`** (0 = steady/clean, 1 = maximally mutating). The pattern is not re-rolled every step;
it *evolves* once per pass, so it repeats enough to be musical and changes enough to stay alive.

## 2. Where it comes from

Derived from the rhythm "error engine" of the user's ChronoTron3 **vestige** module
(`NitroTron3/pedals/chronotron3/modules/vestige.h`, `vestige_constants.h`; design in
`NitroTron3/docs/ChronoTron3/vestige-onward-rework-plan.md` §4.3, §5, §7). Vestige is an
audio looper/freeze; its audio side does **not** carry over. What carries over:

| Vestige | Here |
|---|---|
| Time base T (knob log taper or tap) | clock bus (`docs/clock/MASTER_CLOCK_CONCEPT.md`) |
| Division set 1, 3/4, 2/3, 1/2, 1/3, 1/4, 1/6, 1/8 (`src/core/blocks/grid_quantize.h`) | bus ratio choice; loops of different length phase polymetrically |
| Pass cut into steps (125 ms base, slower above a 2 s knee, ≤ 32 steps) | steps = bus tocks; pass length = N steps (16/32) |
| Error knob, Mode 1 Euclidean (Bjorklund(k,n) stutters/rests) | possible "Euclid" flavour |
| Error knob, Mode 2 **line model** | **core of this module** (below) |
| Hit types (rest, stutter, repeat, double, ratchet, retrig, reverse, decimate, clean, speed) | mapped to trigger behaviors + CV behaviors |
| Separate RNG streams, xorshift32; level 0 consumes no randomness | same |

## 3. Line model (as in vestige, `VESTIGE_TIMING_LINE_*`)

- A **line** of 32–64 cells per layer; every other cell is a pulse cell ("x-x-").
- Hits `{start, len, type, sub, age}`, at least one pause cell between hits.
- **Seed**: `k ≈ 0.5 + 7.5·L` hits per 16 steps, Bjorklund-spread over the pulse cells, random
  rotation; each grows by one cell with probability `0.1 + 0.5·L`.
- **Every pass**: `1 + round(2·L)` mutation ops: add (if below target), remove (if above),
  else shift by one cell / grow / shrink. From `L > 0.67`, ops increasingly become random hits
  anywhere. Top zone `L > 0.75`: up to 8 ops, life 2, max length 1, double speed.
- **Life**: a hit keeps its type for `LIFE = 6` plays, then re-rolls the type — the rhythm
  persists, the content varies. Ratchet counts graded.
- **Layers** TIMING / CONDITION / PLAYBACK stack (which step plays / what is muted / how it plays).
- **Slice mode** (alternative): per-step random substitution with probability `L^curve`.

## 4. Mapping to O_C

Per hemisphere: 2 trig/CV inputs, 2 outputs (A, B).

- **Triggers**: a hit cell → trigger (or gate of `len` cells); ratchet/double/retrig → n
  subdivided triggers inside the step (needs `PeriodTicks(bus)`); rest → no trigger.
- **CV**: each hit carries a value. Options: (a) per-hit random value re-rolled at type
  re-roll; (b) a "slice" table of N values (like a loop of voltages) that hit types act on:
  repeat = hold previous, reverse = read backwards, speed = skip, decimate = quantize coarser.
  Optional quantizer (upstream `HS::Quantizer` helpers in `HSUtils.h`).
- **Clock**: follows clock bus k (planned) or trigger input 1 until buses exist; pass = N
  bus tocks; reset on input 2 or bar reset.
- **Main control**: `L` on encoder default target + CV in 1 (bipolar CV adds to knob).

Default proposal: **Out A = trigger, Out B = CV** (sample-and-hold of the current hit's value).

## 5. Data model sketch (core, `custom/core/evolving.h`)

```
struct Hit { uint8_t start, len, type, sub, age; };      // 5 B
struct Line { uint64_t cells; Hit hits[16]; uint8_t n; };  // ≈ 90 B per layer
struct Evolving {
  Line layer[3]; int16_t values[32]; uint32_t rng_seed_main, rng_seed_cv;
  uint8_t step, pass_len; uint16_t L;                    // L in Q0.16, no float
  void OnStep();   // called on a clock tock (cheap)
  void OnPass();   // mutation; bounded work, can be spread over ticks
};
```
≈ 350 B RAM per instance, two instances (left/right) < 1 KB. Flash: a few KB. All integer
math (no FPU); the vestige float constants become Q16 constants in `evolving_constants.h`.

Persisted 64-bit word: L (8 b), bus/ratio (4), pass length (2), CV range/quant (6), flavour
(2), seed (32) → the pattern can be recalled with a preset (open question).

## 6. Open questions (user)

1. Name (Vestige / Drift / Erode / other)?
2. Outputs: A trig + B CV (proposal), or two independent trigger lanes, or two CV lanes?
3. CV model: random-per-hit (a) or slice table (b)?
4. Which layers survive on a 64x64 module: TIMING only first, CONDITION/PLAYBACK later?
5. Flavours: line model only, or also Euclid (mode 1) and slice mode?
6. Pass length: fixed 16 steps, or selectable 8/16/32?
7. Persist the seed (recallable pattern) or always start fresh?
8. "Freeze" input (stop evolution, keep current pattern) on trig in 2?
9. Display: line as a cell strip + L bar, or something else?
