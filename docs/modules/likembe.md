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
| TR1 | Pulse clock (with `Clk` = `Auto`): one pulse = one grid pulse (16th on the 16 grid, triplet 8th on the 12 grid). `Clk` = `TR1..TR4` takes the pulse from that trigger channel instead |
| TR2 | Re-roll: new seed + new passage, applied **immediately** (pulse position kept, phases restart at 0, passage counter restarts) |
| CV1 | Density modulation, bipolar: ±3 V (`HEMISPHERE_3V_CV`) = ±35 percent points (`kDensityCvRange`), added to Density, effective value clamped to 0..100 %; read every tick (one multiply/divide by constant) |
| CV2 | Unassigned (open question) |
| Out A | Velocity gate per note: 5 V on accented notes (`kGateAccentUnits`, element 0 of a thumb), 3 V otherwise (`kGateNormalUnits`); length = 50 % (`kGateLengthPct`) of the measured TR1 interval (last interval, clamped to 10 s `kMaxPulseTicks`; 125 ms `kDefaultPulseTicks` until two pulses are seen); a new note re-asserts the level immediately; rests start no gate |
| Out B | Pitch CV, 1 V/oct, 0 V = C2 (`kMidiAtZeroUnits`); held through rests |

Physical numbering shifts by two in the right-hand slot (TR3/TR4, CV3/CV4, Out C/D). The
trigger map in Clock Setup can route any physical trigger input, or the internal clock
(x3 for the 12 grid, x4 for the 16 grid per beat), to TR1.

**Clock source (`Clk`).** `Auto` = `Clock(0)`: this hemisphere's own first trigger, i.e.
whatever the upstream trigger map / internal clock delivers to TR1 (left) or TR3 (right).
`TR1..TR4` = `HS::frame.clocked[n-1]` read directly, so the right-hand instance can follow
TR1 without patching. Finding (`firmware/software/src/HemisphereApplet.cpp`,
`IOFrame::Load()`): `frame.clocked[0..3]` is computed once per tick for all four trigger
channels; when the internal clock runs and the channel's multiplier is non-zero it holds
`clock_m.Tock(ch)` (internal-clock pulses at that channel's multiplier), otherwise
`trigmap[ch].Clock()` (the trigger map's mapped physical input), plus manual boops, after
the per-channel input skip. So `TRn` follows exactly what channel n of the trigger map /
Clock Setup produces -- including the internal clock -- not necessarily the raw jack n.
The re-roll input stays `Clock(1)`.

## Controls

Encoder turns move the cursor through the parameters (one line, `Name value`); push
toggles edit mode; in edit mode the encoder changes the value. Order = user priority:
Density and Var are the performance parameters and come first. Every setting change is
audible immediately (project rule, `docs/DESIGN_PRINCIPLES.md`); only the passage end (Roll)
happens at the bar line, by definition.

| Cursor | Parameter | Range | Default | Step | Takes effect |
|---|---|---|---|---|---|
| 1 `Dens` | Density | 0–100 % | per part (`kParts[].density_pct`, Bass 85) | 5 % | immediately; display shows the effective value incl. CV1 |
| 2 `Var` | Variation rate | 0–100 % (= 0–1 mutations per bar) | per part (Bass 8) | 5 % | immediately |
| 3 `Part` | Part | Bass, Lead, MiSol, Solo | Bass | 1 | immediately (regenerates from seed with the part's default loops; bar position and passage counter kept, phases restart at 0); Density/Var keep the user's values (no per-part default reload) |
| 4 `Root` | Root (lowest key) | C..B, wraps | E (`kDefaultRoot`) | 1 semitone | immediately |
| 5 `Oct` | Octave offset (shifts all pitches by 12·Oct semitones) | −2…+3, shown `Oct +1` / `Oct 0` / `Oct -2` | 0 | 1 octave | immediately |
| 6 `Grid` | Pulses per bar | 16 / 12 (right = 16, left = 12) | 16 | — | immediately (regenerates; pulse position wrapped to the new grid, passage counter kept, phases restart at 0) |
| 7 `Roll` | Re-roll every N bars (passage length) | Off, 1–8 bars; shows `Off` or `len:bar` | Off | — | setting immediately; the roll itself happens at the bar line that ends the passage |
| 8 `Seed` | Seed | 0–65535 (decimal) | 1 | any turn in edit mode = new random seed | immediately (`RerollNow`, same as TR2) |
| 9 `Clk` | Pulse clock source | `Auto`, `TR1`, `TR2`, `TR3`, `TR4` | `Auto` (`kClockAuto`) | 1 | immediately (next tick) |

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
modulo 12/16 and does the bar-line work (phase advance by pulses/2 mod n, then passage end
or variation) inside the call that ends the bar. Grid/Part/Seed changes and re-rolls (TR2,
Seed encoder) regenerate immediately in `SetParams()` / `Reroll()`; the bar position is kept
(`pulse_idx` wrapped to the new grid), so the clock grid never jumps.
`Reset()` zeroes the bar position and phases and keeps the figures.

## Pitch mapping

`pitch_units = (midi - kMidiAtZeroUnits) * kUnitsPerSemitone`, clamped to
0..`kMaxPitchUnits` (6 V); a large Oct offset simply clamps there. Bass A2 = 1152 (0.75 V), lead A3 = 2688 (1.75 V). Degree →
MIDI per spec: `base + 12*(floor(d/n) + Oct) + offset[d mod n]`, base = root pitch class nearest
the anchor (−5..+6 semitones).

## Persistence (`Pack()`/`Unpack()`, settings only, 47 of 64 bits)

| Bits | Field | Validation |
|---|---|---|
| 0–6 | density_pct | clamp ≤ 100 |
| 7–8 | part | ≥ 4 → Bass (2 bits, always valid) |
| 9–12 | root | clamp ≤ 11 |
| 13 | grid16 | — |
| 14–20 | variation_pct | clamp ≤ 100 |
| 21–24 | passage_idx | clamp ≤ 8 |
| 25–40 | seed | — |
| 41–43 | octave + 2 (0..5 = −2..+3) | 6, 7 clamp to +3 |
| 44–46 | clock_src (0 Auto, 1–4 TR1–TR4) | 5–7 → Auto |

Unpack regenerates everything from the seed (evolving state is not persisted).

## Browser lab

`docs/demo/likembe-lab.html` (open directly from disk, `file://`; no build, no network).
The pattern engine lives in `docs/demo/likembe_engine.js`, a 1:1 port of
`likembe_core.h` (same xorshift32 streams and seeding, integer math, draw order, thresholds,
mutation rules, bar-line handling, Oct/Root mapping, part profiles incl. the provisional
Mi-solo/Solo, passages). Given seed + parameters it produces the same event stream as the
module; `task host:parity` proves it (C++ `host/tools/likembe_dump.cpp` vs
`host/tools/likembe_lab_parity.mjs`, line format `bar pulse thumb degree rest accent pitch_units`).

- **Two voices** = two applet instances on one tempo (each clocked x4 per beat on the 16 grid,
  x3 on 12). Per voice the module parameters (Dens, Part, Root, Oct, Grid, Var, Roll, Seed; `Clk`
  is not in the lab -- all lab voices share one tempo) with the module's ranges and steps;
  "re-roll (TR2)" = TR2 / Seed encoder, immediate; typing a seed regenerates immediately. Mute per voice.
- **Display:** a 4x canvas of the module display below the parameter line (tines, step row,
  running light, beat ticks, bar-cycle squares, status glyphs, L/R loop rows), drawn by a port of
  `Likembe::View`, plus a text line per thumb (`X` accent, `x` note, `.` rest, `[ ]` next element,
  degrees, phase, cycle, seed, roll counter, `~theme`, `+lab` = lab constants pending).
- **Sound:** tine synth, amp and room from the Congotronics demo. Velocity = Out A gate level
  / 5 V (accent 5 V = 1.0, normal 3 V = 0.6); note sustain = gate length.
- **Reference only (not in the firmware):** bass drum patterns and clave/bell/beat from the
  demo; they follow voice 1's grid.
- **Lab / constants:** firmware constants with their C++ names, per part: `kStepWeight`
  (per-part table in the core), `kMutWeight`, `kReturnToThemePct`, `kLoopChangePct`,
  `kOctaveHeadroom`; global: `kThumbLStartMiddle`; global and live:
  `kGateAccentUnits`, `kGateNormalUnits` (as volts), `kGateLengthPct`. Engine constants apply at
  the next bar line (immediately when stopped). **Export constants** prints a snippet in
  `likembe_constants.h` format (`kStepWeight[kPartCount][5]`, `kThumbLStartMiddle`); a
  global-only value that differs between parts is printed as a per-part table with a note
  that the core needs a change for it.
- **Dump events:** N bars from Init with the voice's current parameters, in the parity format,
  headed by the equivalent `likembe_dump` arguments (flagged when lab values differ from firmware).

Workflow: tune by ear in the lab, Export, paste into `likembe_constants.h`, port the lab
default in `labDefaults()` (`likembe_engine.js`) to the same values, `task host:parity`, `task host:test`.

## Constants (`custom/core/likembe_constants.h`)

`kUnitsPerSemitone`, `kUnitsPerOctave`, `kMidiAtZeroUnits`, `kMaxPitchUnits`, `kTunings`,
`kTuningCount`, `kGrid16`, `kGrid12`, `kMinLoop`, `kMaxLoop`, `kParts`, `kStepDelta`,
`kStepWeight`, `kThumbLStartMiddle`, `kMutWeight`, `kOctaveHeadroom`, `kReturnToThemePct`, `kLoopChangePct`,
`kEnergyBuildPct`, `kDensityMin` (0), `kDensityMax`, `kOctaveMin`, `kOctaveMax`, `kDefaultOctave`, `kDensityCvRange`, `kVariationMax`, `kPassageBars`,
`kDefaultPassageIdx`, `kDefaultRoot`, `kDefaultGrid16`, `kDefaultSeed`,
`kPartStreamOffset`, `kPassageStreamOffset`, `kSeedMix`, `kSeedNonZero`, `kGateAccentUnits`,
`kGateNormalUnits`, `kGateLengthPct`, `kDefaultPulseTicks`, `kMaxPulseTicks`.

- `kStepWeight[kPartCount][5]`: per part, relative odds of the key step (+1, -1, 0, +2, -2,
  order of `kStepDelta`) between consecutive strokes when a thumb figure is generated; the
  active part's row is used.
- `kThumbLStartMiddle`: thumb L's figure starts at the middle key (`c / 2`) like thumb R when
  true (current, all parts); lowest key (`0`) when false.

## Display (64×64, below the applet header)

| y | Element | Meaning |
|---|---|---|
| 14 | `Name value` | selected parameter; underline cursor (blinks), inverted in edit mode |
| 24 | Bridge | 1 px line across the tines |
| 25–36 | Tines | one tine per key of the part (key_lo .. key_hi + octave headroom, max 15), 4 px column pitch, hanging from the bridge. Mapping follows the core's thumb split by absolute degree parity: even degree (thumb L) on the left from x 29 outward, odd degree (thumb R) on the right from x 33 outward, 4 px pitch, no shared centre tine; column within a side = (d − first same-parity degree ≥ key_lo) / 2, so each side's lowest degree is nearest the centre. Length 12 px in the centre, −1 px per column outward (min 6). Struck tine (Out A gate high): 3 px wide bar; accent additionally a 5×3 block at the tip. After the gate ends, the last struck tine keeps a 3 px foot under its tip until the next note |
| 38–43 | Step row | this bar's pulses (`EventAt`), 4 px cells (16 grid) / 5 px (12 grid): tall bar = accented note (element 0 of a thumb), short bar = note, dot = rest or offset pulse |
| 44–45 | Running light | 2 px bar (cell width) under the pulse just played |
| 46 | Beat ticks | 1 px tick at the first pulse of each beat (every 4 pulses on 16, every 3 on 12); downbeat 2 px wide |
| 48–50 | Bar-cycle squares | one 3×3 square per bar of the realignment cycle N (4 px pitch, from x 0), current bar filled, others hollow; if N × 4 > 50: 1 px-wide markers (width 50/N) with the current one filled. Never reaches x 50 |
| 48–50, x 58 | Status | 3 px pixel glyphs: `~` (x 58) = figures differ from the passage theme |
| 52–56 | `L` + n cells | left thumb loop: 3×5 pixel label at x 0, 5×5 cells from x 5 at 7 px pitch (n = 2..8, ends x 58): filled = element played next, solid frame = note, dotted frame (checkerboard, corners lit) = rest at current density |
| 58–62 | `R` + n cells | right thumb, same; different widths = different loop lengths, the filled cell moves at half the pulse rate and starts at a different cell each bar when the loop does not fit |

**Bar counter.** N = lcm(2·nL, 2·nR, B) / B bars (B = pulses per bar, nL/nR = loop lengths in strokes) is the realignment cycle after which both thumbs start the bar on element 0 again; the part's start offset shifts both thumbs equally and does not change N. Bass 16 grid L4/R3: 3; Bass 12 grid L3/R4: 2; fitting loops (Lead): 1. i = bar playing now, 1..N (the filled square). Rule (core `SyncCycle`): N is recomputed and i restarts at 1 on `Reset()`, whenever figures are generated (Init, grid/part/seed change, re-roll, new passage -- phases restart at 0) and when a mutation or return-to-theme changes a loop length; otherwise i advances at each bar line and wraps N -> 1. Max reachable N is 35 (segment fallback above 12 bars).

Verified screenshots (sim, not hardware): `host/build/out/tv_{bass3,lead3,cycle3}/` (current layout); earlier tine-only layout: `host/build/out/tv_bass/` (Bass, accented note, gate high), `tv_bass_after/` (gate low, foot marker), `tv_lead/` (left = Lead), `tv_12/` (grid 12), `tv_cycle/` (seed 14572, N = 7, `~`).
