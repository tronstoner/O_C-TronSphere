# Likembe

**Status:** core + glue implemented, host-tested and sim-verified (`custom/core/likembe_core.h`,
`custom/applets/Likembe.h`, `host/tests/test_likembe_core.cpp`); **not yet verified on hardware**.
Applet ID **201**, name `Likembe`, category Sequencer (0x02). Flash cost +5 040 B, RAM +272 B.
Plan and open questions: `likembe-impl-plan.md`.
Spec: `docs/research/Likembe pattern engine technical concept.md`; JS reference:
`docs/research/Congotronics line generator.html`.

## Purpose

Generates one monophonic, interlocking likembe-style line (Congotronics) on an external
pulse clock. Two virtual thumbs strike on alternate pulses; each loops its own short figure
(2–8 strokes). Cross-rhythm comes from the different loop lengths. The figures evolve
slowly (mutations with return to theme) and are replaced by a new passage on a re-roll or
after an optional passage length.

**One applet instance = one part** (decision). Two instances (e.g. Bass left, Lead right)
on the same clock give the two-part texture of the spec. Drums/guide rhythm are not part of
this module.

## I/O (Hemisphere terms)

| Jack | Function |
|---|---|
| TR1 | Pulse clock: one pulse = one grid pulse (16th on the 16 grid, triplet 8th on the 12 grid) |
| TR2 | Re-roll: new seed + new passage, applied at the next bar line |
| CV1 | Density modulation, bipolar: ±3 V (`HEMISPHERE_3V_CV`) = ±35 percent points (`kDensityCvRange`), added to Density, effective value clamped to 0..100 %; read every tick (one multiply/divide by constant) |
| CV2 | Unassigned (open question) |
| Out A | Velocity gate per note: 5 V on accented notes (`kGateAccentUnits`, element 0 of a thumb), 3 V otherwise (`kGateNormalUnits`); length = 50 % (`kGateLengthPct`) of the measured TR1 interval (last interval, clamped to 10 s `kMaxPulseTicks`; 125 ms `kDefaultPulseTicks` until two pulses are seen); a new note re-asserts the level immediately; rests start no gate |
| Out B | Pitch CV, 1 V/oct, 0 V = C2 (`kMidiAtZeroUnits`); held through rests |

Physical numbering shifts by two in the right-hand slot (TR3/TR4, CV3/CV4, Out C/D). The
trigger map in Clock Setup can route any physical trigger input, or the internal clock
(x3 for the 12 grid, x4 for the 16 grid per beat), to TR1.

## Controls

Encoder turns move the cursor through the parameters (one line, `Name value`); push
toggles edit mode; in edit mode the encoder changes the value. Order = user priority.

| Cursor | Parameter | Range | Default | Step | Takes effect |
|---|---|---|---|---|---|
| 1 `Dens` | Density | 0–100 % | per part (`kParts[].density_pct`, Bass 85) | 5 % | immediately; display shows the effective value incl. CV1 |
| 2 `Part` | Part | Bass, Lead, MiSol, Solo | Bass | 1 | next bar line (regenerates from seed with the part's default loops); Density/Var keep the user's values (no per-part default reload) |
| 3 `Root` | Root (lowest key) | C..B, wraps | E (`kDefaultRoot`) | 1 semitone | immediately |
| 4 `Oct` | Octave offset (shifts all pitches by 12·Oct semitones) | −2…+3, shown `Oct +1` / `Oct 0` / `Oct -2` | 0 | 1 octave | immediately |
| 5 `Grid` | Pulses per bar | 16 / 12 (right = 16, left = 12) | 16 | — | next bar line (regenerates) |
| 6 `Var` | Variation rate | 0–100 % (= 0–1 mutations per bar) | per part (Bass 8) | 5 % | immediately |
| 7 `Pass` | Passage length | Off, 8, 16, 24, 32, 48, 64 bars; shows `len:bar` | Off | — | immediately |
| 8 `Seed` | Seed | 0–65535 (decimal) | 1 | any turn in edit mode = new random seed | **immediately** (`RerollNow`); TR2 re-roll waits for the bar line |

Element 0 of each thumb never rests, so at 0 % density each thumb plays only its first element, once per pass through its loop.

Unused/fixed (constants): Tuning (pentatonic only, `kTuningCount`), return to theme
`kReturnToThemePct`, loop change `kLoopChangePct`, energy build `kEnergyBuildPct` (0),
register shift, hocket, start offset (per part). Buttons: upstream defaults only.
`Reset()` (shell reset) zeroes bar position and phases, keeps figures.

## Part profiles (`kParts` in `likembe_constants.h`)

| Part | Anchor | Degrees (n = 5) | Loop policy | Density | Variation | Offset | Loops 16 / 12 | Source |
|---|---|---|---|---|---|---|---|---|
| Bass | MIDI 45 (A2) | 0..8 (0–1.6 n) | one may drift | 85 % | 8 % (spec 0.075) | 0 | 4/3, 3/4 | spec |
| Lead / accomp. | MIDI 57 (A3) | 2..10 (0.4–2.0 n) | fit | 65 % | 25 % | 1 | 8/4, 6/3 | spec |
| Mi-solo | MIDI 60 (C4) | 3..11 (0.6–2.2 n) | fit | 55 % | 30 % | 1 | 4/8, 3/6 | **PROVISIONAL** |
| Solo | MIDI 64 (E4) | 5..13 (1.0–2.6 n) | one may drift | 45 % | 40 % | 3 | 8/5, 6/4 | **PROVISIONAL** |

Mi-solo/Solo follow Congolese guitar-band roles: mi-solo between accompaniment and solo in
register and busyness, solo highest, sparsest and most varied. To be tuned by ear.
The Solo anchor is chosen so the highest reachable degree (`key_hi + kOctaveHeadroom` = 15,
via octave mutation) at the worst root stays at 70 semitones above C2 = 5.83 V.

## Clock model

No tempo inside the core. Each `Pulse()` call plays one grid pulse; the core counts pulses
modulo 12/16 and does the bar-line work (phase advance by pulses/2 mod n, then pending
regeneration, pending re-roll, passage end, or variation) inside the call that ends the bar.
`Reset()` zeroes the bar position and phases and keeps the figures.

## Pitch mapping

`pitch_units = (midi - kMidiAtZeroUnits) * kUnitsPerSemitone`, clamped to
0..`kMaxPitchUnits` (6 V); a large Oct offset simply clamps there. Bass A2 = 1152 (0.75 V), lead A3 = 2688 (1.75 V). Degree →
MIDI per spec: `base + 12*(floor(d/n) + Oct) + offset[d mod n]`, base = root pitch class nearest
the anchor (−5..+6 semitones).

## Persistence (`Pack()`/`Unpack()`, settings only, 43 of 64 bits)

| Bits | Field | Validation |
|---|---|---|
| 0–6 | density_pct | clamp ≤ 100 |
| 7–8 | part | ≥ 4 → Bass (2 bits, always valid) |
| 9–12 | root | clamp ≤ 11 |
| 13 | grid16 | — |
| 14–20 | variation_pct | clamp ≤ 100 |
| 21–23 | passage_idx | clamp ≤ 6 |
| 24–39 | seed | — |
| 40–42 | octave + 2 (0..5 = −2..+3) | 6, 7 clamp to +3 |

Unpack regenerates everything from the seed (evolving state is not persisted).

## Constants (`custom/core/likembe_constants.h`)

`kUnitsPerSemitone`, `kUnitsPerOctave`, `kMidiAtZeroUnits`, `kMaxPitchUnits`, `kTunings`,
`kTuningCount`, `kGrid16`, `kGrid12`, `kMinLoop`, `kMaxLoop`, `kParts`, `kStepDelta`,
`kStepWeight`, `kMutWeight`, `kOctaveHeadroom`, `kReturnToThemePct`, `kLoopChangePct`,
`kEnergyBuildPct`, `kDensityMin` (0), `kDensityMax`, `kOctaveMin`, `kOctaveMax`, `kDefaultOctave`, `kDensityCvRange`, `kVariationMax`, `kPassageBars`,
`kDefaultPassageIdx`, `kDefaultRoot`, `kDefaultGrid16`, `kDefaultSeed`,
`kPartStreamOffset`, `kPassageStreamOffset`, `kSeedMix`, `kSeedNonZero`, `kGateAccentUnits`,
`kGateNormalUnits`, `kGateLengthPct`, `kDefaultPulseTicks`, `kMaxPulseTicks`.

## Display (64×64, below the applet header)

| y | Element | Meaning |
|---|---|---|
| 14 | `Name value` | selected parameter; underline cursor (blinks), inverted in edit mode |
| 25–30 | Pulse row | this bar's pulses (`EventAt`), 4 px cells (16 grid) / 5 px (12 grid): tall bar = accented note (element 0 of a thumb), short bar = note, dot = rest or offset pulse |
| 32–33 | Pulse cursor | block under the next pulse to play |
| 35 | Beat ticks | 1 px tick at the first pulse of each beat (every 4 pulses on 16, every 3 on 12); downbeat tick 2 px wide |
| 38–43 | `L` + n cells | left thumb loop (cells from x 7, 2 px right margin): one 7 px cell per element (n = 2..8), filled = element played next, solid frame = note, dotted frame = rest at current density |
| 46–51 | `R` + n cells | right thumb, same; different widths = different loop lengths, the filled cell moves at half the pulse rate and starts at a different cell each bar when the loop does not fit |
| 54–63 | Contour | degree per sounding pulse of this bar (2×2 dots joined by dotted lines), scaled to the part's range |
| 54, right-aligned | `b<i>/<N>` + status | on a cleared box over the contour's top-right: bar counter (see below); one glyph slot left of it: blinking `*` = re-roll pending (next bar line), `~` = figures differ from the passage theme |

**Bar counter.** N = lcm(2·nL, 2·nR, B) / B bars (B = pulses per bar, nL/nR = loop lengths in strokes) is the realignment cycle after which both thumbs start the bar on element 0 again; the part's start offset shifts both thumbs equally and does not change N. Bass 16 grid L4/R3: 3; Bass 12 grid L3/R4: 2; fitting loops (Lead): 1. i = bar playing now, 1..N. Rule (core `SyncCycle`): N is recomputed and i restarts at 1 on `Reset()`, whenever figures are generated (Init, grid/part/seed change, re-roll, new passage -- phases restart at 0) and when a mutation or return-to-theme changes a loop length; otherwise i advances at each bar line and wraps N -> 1. Display caps both at 99 (max reachable N is 35). The counter is not on the parameter line because long values (`Pass 64:12`, `Seed 65535`) reach x 60.

Verified screenshots: `host/build/out/lkb_{bass,bass12,lead,pass}/ui_001_oled_x6.png` (bar indicator), `host/build/out/lk3_*/ui_001_oled_x6.png`, `host/build/out/lk5/` (Oct +1), `host/build/out/lk5d/` (Dens 0 %) (sim, not hardware).
