# Likembe pattern engine: technical concept

Oct 5, 2026 · @Ralf Traunsteiner

## Overview

The engine generates two interlocking, monophonic melodic lines over a fixed pulse, in the manner of amplified likembe (lamellophone) ensembles. Each line is played by two virtual thumbs that strike on alternate pulses, and each thumb loops its own short figure. Cross-rhythm comes from the thumbs' loop lengths, not from imposed accents.

The engine has four layers on one shared time base:

| Layer | Role | Changes over time |
| --- | --- | --- |
| Bass drum | Marks the beat | Fixed pattern |
| Guide rhythm | Timeline reference (clave) | Fixed pattern |
| Bass part | Low, dense line | Slowly |
| Lead part | Higher, sparser line | More often |

Per pulse, the engine outputs at most one note event per part (pitch, thumb, accent) plus the drum and guide hits. Everything is deterministic for a given seed and parameter set.

Processing per bar:

1. Unroll both parts' thumb figures over the bar's pulses.
2. Emit drum, guide and part events pulse by pulse.
3. At the bar line, advance the thumbs' phases, then apply variation or start a new passage.

## Time model

Tempo defines the beat; a bar is always four beats. The bar is subdivided into a pulse grid of either 12 or 16 pulses, and every layer runs on that grid.

| Grid | Pulses per beat | Pulses per bar | Feel |
| --- | --- | --- | --- |
| 12 | 3 | 12 | Ternary |
| 16 | 4 | 16 | Binary |

```latex
t_{pulse} = \frac{60}{\text{bpm} \cdot p}
```

where p is pulses per beat (3 or 4). Switching grids keeps the beat and bar duration; the pulse becomes a triplet or a sixteenth of the beat.

The generation cycle is one bar. All per-bar processing (variation, passage counting, phase advance) happens at the bar line.

## Rhythm layers

The bass drum and guide rhythm are fixed onset sets on the pulse grid, repeated every bar. Pulse 0 is the downbeat and is played slightly louder.

**Bass drum patterns** (pulse indices):

| Pattern | 12-pulse grid | 16-pulse grid |
| --- | --- | --- |
| On every beat | 0, 3, 6, 9 | 0, 4, 8, 12 |
| Every beat with a pickup | 0, 3, 6, 9, 11 | 0, 4, 8, 12, 14 |
| Half time | 0, 6 | 0, 8 |
| Three across the bar | 0, 4, 8 | 0, 3, 6, 8, 11, 14 |

**Guide rhythm (clave).** The 16-pulse clave is a rotation of the Euclidean rhythm E(9,16): seven gaps of 2 pulses and two gaps of 1. The two single gaps are turning points where the strokes switch between on-beat and off-beat pulses. The 12-pulse equivalent is E(7,12), which has the same structure: five gaps of 2 and two gaps of 1.

| Grid | Euclidean form | Onsets | Pattern |
| --- | --- | --- | --- |
| 16 | E(9,16) | 0, 2, 4, 6, 8, 9, 11, 13, 15 | `x.x.x.x.xx.x.x.x` |
| 12 | E(7,12) | 0, 2, 4, 6, 7, 9, 11 | `x.x.x.xx.x.x` |

Both are rotated so the bar starts with the on-beat run, then turns. Alternative guides: the standard 12/8 bell (0, 2, 4, 5, 7, 9, 11), the 3-2 son clave on 16 (0, 3, 6, 10, 12), or a plain beat.

## Part model

Each part is one monophonic voice played by two thumbs, L and R, that strike on alternate pulses. The thumbs own alternate scale steps, so alternating thumbs produce zigzag lines rich in thirds.

**Tuning.** A tuning is a list of semitone offsets within one octave, with n = its length:

| Tuning | Offsets |
| --- | --- |
| Pentatonic | 0, 2, 4, 7, 9 |
| Pentatonic, minor colour | 0, 3, 5, 7, 10 |
| Hexatonic | 0, 2, 4, 5, 7, 9 |
| Seven notes | 0, 2, 4, 5, 7, 9, 10 |

**Keys.** A key is a scale degree d (integer, 0 = the root in the part's register). Its pitch is:

```latex
\text{midi}(d) = \text{base} + 12 \left\lfloor d / n \right\rfloor + \text{offset}[d \bmod n]
```

**Register.** Each part has an anchor note. The base is the note with the root's pitch class nearest the anchor (within ±6 semitones), plus 12 × register shift (−1, 0 or +1).

| Part | Anchor | Key range in degrees |
| --- | --- | --- |
| Bass | MIDI 45 (A2) | 0 … round(1.6 n) |
| Lead | MIDI 57 (A3) | round(0.4 n) … round(2.0 n) |

**Key partition.** Within the range, thumb L owns the even degrees and thumb R the odd degrees.

## Thumb figures

A thumb figure is a looped list of n elements, each a key plus a rest flag. A thumb plays every other pulse, so a loop of n strokes lasts 2n pulses.

**Generation** (for a thumb with keys K, sorted low to high, loop length n, density ρ):

1. Start index k: 0 for thumb L, ⌊|K| / 2⌋ for thumb R.
2. For each element j = 0 … n−1: if j > 0, move k by a weighted step and clamp to K: +1 (3), −1 (3), 0 (1.2), +2 (0.8), −2 (0.8).
3. Element j = key K\[k\], rest = (j > 0 and random > ρ). Element 0 never rests.
4. If all keys are equal and n > 1, raise the last element by one key.

**Phase.** Each thumb keeps a phase, the index of its next element. It starts at 0 and, at every bar line, advances by the strokes per bar (pulses per bar / 2), modulo n. A figure whose length does not divide the bar therefore starts at a different element each bar.

**Fit rule.** A loop fits the bar when 2n divides the pulses per bar:

| Grid | Fitting loop lengths (strokes) |
| --- | --- |
| 12 | 2, 3, 6 |
| 16 | 2, 4, 8 |

The two thumbs of a part always have different lengths. Each part has a loop policy:

| Policy | Rule |
| --- | --- |
| Fit the bar | Both thumbs take fitting lengths |
| One thumb may drift | At least one thumb takes a fitting length; the other takes any length 2–8 |
| Both may drift | Any lengths 2–8 |

**Realignment.** With thumb loops of a = 2n\_L and b = 2n\_R pulses and a bar of B pulses:

- a = b: the thumbs loop together.
- B divisible by both: B/a against B/b in every bar.
- Otherwise the figure repeats every lcm(a, b, B) / B bars.

## Rendering a bar

At each bar start, each part is unrolled into an array of B note slots. The lead part can start a fixed number of pulses late (offset, default 1), which puts its thumbs on the off-pulses of the bass.

```
for i in 0 .. B-1:
    j = i - offset                     # offset = 0 for the bass part
    if j < 0: slot[i] = none; continue
    thumb = L if j is even else R
    t     = figure[thumb]
    idx   = (t.phase + floor(j / 2)) mod t.n
    e     = t.elements[idx]
    slot[i] = none if e.rest else Note(midi(e.key), thumb, accent = (idx == 0))
```

- **Accent:** the first element of a thumb's figure is accented (velocity 1.0, others 0.8). Thumb R plays at 0.9 of thumb L.
- **Offset:** the lead stays silent for the first offset pulses of every bar, because the unrolling restarts at each bar line.
- **Overlap:** with *play both*, the two parts are independent. With *hocket*, a lead note is dropped wherever the bass part has a note on the same pulse.
- **Monophony:** each part has one voice. A new note cuts the previous one with a short fade.

## Variation engine

At each bar line that does not start a new passage, each part is varied independently by a small number of mutations on its thumb figures. Each part keeps a theme: a copy of its figures as generated at the start of the passage.

**Return to theme.** With probability ret × min(1, rate), if the figures differ from the theme, both figures are restored to the theme. Phases are kept, modulo the restored loop lengths.

**Mutation count.** For a variation rate r per bar:

```latex
m = \lfloor r \rfloor + \text{Bernoulli}(r - \lfloor r \rfloor)
```

**Mutations.** Each picks a thumb at random (L or R with equal chance), then a type by weight:

| Type | Weight | Effect |
| --- | --- | --- |
| Key | 3 | One element moves one key up or down within the thumb's keys |
| Rest | 2 | One element other than the first toggles its rest flag |
| Octave | 0.6 | One element moves n degrees up, or down if up leaves the range |
| Loop length | 0.5 | The loop moves to the nearest allowed length, up or down |

Loop-length changes follow the part's loop policy and keep the two thumbs unequal. Growing a loop appends elements one key away from the last element. Shrinking truncates. The phase is reduced modulo the new length.

A mutation that would change nothing is skipped, not retried.

## Passages and energy build

A passage is a run of bars with stable figures; at its end both parts get new figures. A new passage starts when the bar count in the passage reaches the passage length (default 24 bars), or immediately on a manual cue.

At a new passage:

1. The passage counter p increases by 1.
2. For each part, with probability *loop length change*, both thumbs get new loop lengths under the part's loop policy (always unequal). With *one thumb may drift*, a random thumb takes a fitting length and the other any length.
3. Both parts' figures are regenerated with phases reset to 0, and become the new theme.

**Energy build.** Density and amplifier drive rise over five passages, then restart:

```latex
\rho_{eff} = \min\left(1,\ \rho + 0.08 \cdot E \cdot ((p-1) \bmod 5)\right)
```

```latex
\text{drive}_{eff} = \min\left(1,\ \text{drive} \cdot (1 + 0.18 \cdot E \cdot ((p-1) \bmod 5))\right)
```

where E is the energy build setting (0–1).

## Randomness and reproducibility

All pattern decisions come from three independent seeded generators, so a seed plus the parameter set reproduces the same evolution bar for bar.

| Stream | Seed | Used for |
| --- | --- | --- |
| Bass | seed | Bass figure generation and variation |
| Lead | seed + 7919 | Lead figure generation and variation |
| Passages | seed + 31 | Loop-length rerolls at new passages |

Any uniform PRNG works; separate streams keep one part's changes from shifting the other's decisions. Changing a parameter mid-run also changes which random numbers are drawn afterwards, so reproducibility holds only for identical settings and interactions.

Sound synthesis noise may use an unseeded source, since it never feeds back into the pattern.

## Parameters and defaults

| Group | Parameter | Range | Default |
| --- | --- | --- | --- |
| Time | Tempo | 80–180 bpm | 82 bpm |
| Time | Pulse grid | 12 or 16 per bar | 16 |
| Tuning | Root (lowest key) | any pitch class | E |
| Tuning | Tuning | see Part model | Pentatonic |
| Rhythm | Bass drum pattern | see Rhythm layers | On every beat |
| Rhythm | Guide rhythm | clave, bell, beat, off | Clave |
| Bass part | Thumb loops L / R (16 grid) | 2–8 strokes | 4 / 3 |
| Bass part | Thumb loops L / R (12 grid) | 2–8 strokes | 3 / 4 |
| Bass part | Loop policy | fit, one may drift, both may drift | One may drift |
| Bass part | Density | 0.3–1 | 0.85 |
| Bass part | Variation rate | 0–1 per bar | 0.075 |
| Lead part | Thumb loops L / R (16 grid) | 2–8 strokes | 8 / 4 |
| Lead part | Thumb loops L / R (12 grid) | 2–8 strokes | 6 / 3 |
| Lead part | Loop policy | fit, one may drift, both may drift | Fit the bar |
| Lead part | Density | 0.3–1 | 0.65 |
| Lead part | Variation rate | 0–1 per bar | 0.25 |
| Lead part | Start offset | 0–5 pulses | 1 |
| Lead part | Overlap | play both, hocket | Play both |
| Passages | Passage length | 8–64 bars | 24 |
| Passages | Loop length change | 0–1 | 0.4 |
| Passages | Return to theme | 0–1 | 0.3 |
| Passages | Energy build | 0–1 | 0.5 |
| Both parts | Register shift | −1, 0, +1 octave | 0 |

Switching the grid loads that grid's default loop lengths.

## Design decisions and rationale

| Decision | Reason |
| --- | --- |
| Cross-rhythm comes from thumb loop lengths, not imposed accents | Accenting every group start sounded mechanical; interlocking loops give the idiomatic, self-generated cross-rhythm |
| Thumbs own alternate scale steps | Mirrors the likembe key layout; alternation produces thirds and zigzag contours |
| Lead loops fit the bar; bass may let one thumb drift | Drifting melody loops sound random against the clave; a drifting bass thumb gives evolving basslines while the other thumb anchors |
| The beat stays fixed when switching 12 ↔ 16 | The bass drum keeps its tempo; the grid only changes the subdivision (triplets against sixteenths) |
| Clave = E(9,16), 12-grid guide = E(7,12) | E(9,16) was transcribed from a reference recording; E(7,12) keeps its structure of two turning points at a similar density |
| One tonal centre, one-bar cycle | The target is a single evolving loop, not a chord progression |
| Slow variation with return to theme | Long repetition with gradual change; the theme stops variation from drifting into noise |
| One monophonic voice per part | Each part maps to one output on monophonic hardware |
| Separate seeded random streams | Reproducible results; one part's changes do not alter the other's |

## Appendix: reference sound model

The pattern engine is independent of sound; this is the reference voice set, in case a port should match it.

**Likembe tine** (per note, f = note frequency, t in seconds):

- Fundamental: sin(2πft) · e^(−t/τ₁), with τ₁ = clamp(0.9 − 0.016 · (midi − 45), 0.22, 0.9).
- Inharmonic first overtone of a clamped bar: 0.38 · sin(2π · 6.27f · t) · e^(−t/0.035).
- Second harmonic: 0.14 · sin(2π · 2f · t) · e^(−2t/τ₁).
- Rattle buzz: 0.22 · tanh(7 · fundamental) · (0.55 + 2.2 · lowpassed noise) · e^(−t/(0.55τ₁)).
- Key click: 4 ms of decaying noise. Normalise each note to a fixed peak.

**Amplifier per part:** tanh waveshaper with k = 1 + 38 · drive (normalised), output gain 1 / (1 + 1.6 · drive), then highpass 180 Hz, peak +5 dB at 1.7 kHz (Q 1.1), lowpass at the tone setting (default 3.4 kHz), plus a short room reverb.

**Bass drum:** sine sweeping from 3.2 × pitch to pitch in 45 ms (default pitch 50 Hz), exponential decay over the length setting, plus a 12 ms noise click highpassed at 2.5 kHz, through a soft clipper for grit.

**Clave (glass bottle):** sum of sine modes on f₀ = 2620 Hz plus a 4 ms noise tick highpassed at 5 kHz.

| Mode ratio | Amplitude | Decay time constant |
| --- | --- | --- |
| 1 | 1.0 | 0.16 s |
| 1.006 | 0.55 | 0.14 s |
| 2.71 | 0.45 | 0.05 s |
| 5.1 | 0.25 | 0.02 s |

**Electric guitar (alternative part voice):** Karplus–Strong string, excited near the bridge (pick position 12% of the string), with decay shortening for higher notes.
