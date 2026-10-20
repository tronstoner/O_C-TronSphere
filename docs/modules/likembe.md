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
| TR1 | Clock, with Clock page `Src` = `Int` (default): `Clock(0)` as upstream routes it, one engine step per pulse; `Len` steps make one bar. `Src` = `TR1..TR4` takes the physical jack instead (rising edge of `frame.gate_high[n-1]`, ClockSetup has no effect), one step every `Div` pulses |
| TR2 | Re-roll (Clock page `Roll`, default = this hemisphere's 2nd input): new seed + new passage, applied **immediately** (pulse position kept, phases restart at 0, passage counter restarts). `Rst` may use any TR as restart input |
| CV1 | Density modulation, bipolar: ±3 V (`HEMISPHERE_3V_CV`) = ±35 percent points (`kDensityCvRange`), added to Density, effective value clamped to 0..100 %; read every tick (one multiply/divide by constant) |
| CV2 | Unassigned (open question) |
| Out A | Velocity gate per note: 5 V on accented notes (`kGateAccentUnits`, bar head or element 0 of a thumb), 3 V otherwise (`kGateNormalUnits`); length = 50 % (`kGateLengthPct`) of the measured engine-step interval (between engine steps, i.e. `Div` input pulses with Src TRn; last interval, clamped to 10 s `kMaxPulseTicks`; 125 ms `kDefaultPulseTicks` until two steps are seen; the interval across a `Reset()` is not measured, the next gate uses the last one); a new note re-asserts the level immediately; rests start no gate |
| Out B | Pitch CV, 1 V/oct, 0 V = C2 (`kMidiAtZeroUnits`); held through rests |

Physical numbering shifts by two in the right-hand slot (TR3/TR4, CV3/CV4, Out C/D).
Tempo and note value come from upstream (ClockSetup multiplier per input, or the trigger
map with its div/mult); the applet only counts pulses. Rules and upstream details:
`docs/CLOCK.md`.

**`TRn` = physical jack** (decided 2026-10-20, `docs/CLOCK.md` 2): `Src TRn`, `Roll TRn`,
`Rst TRn` use the rising edge of `HS::frame.gate_high[n-1]` (raw jack level,
`HemisphereApplet.cpp:87-90`; previous levels kept in the applet as 4 bits). The routed
`frame.clocked[]` (trigger map, internal clock multipliers) is used only by `Src Int`.

## Controls

Encoder turns move the cursor through the parameters (one line, `Name value`); push
toggles edit mode; in edit mode the encoder changes the value. Order = user priority:
Density and Var are the performance parameters and come first. Every setting change is
audible immediately (project rule, `docs/DESIGN_PRINCIPLES.md`); only the passage end (Roll)
happens at the bar line, by definition.

**Main list**

| Cursor | Parameter | Range | Default | Step | Takes effect |
|---|---|---|---|---|---|
| 1 `Dens` | Density | 0–100 % | per part (`kParts[].density_pct`, Bass 85) | 5 % | immediately; display shows the effective value incl. CV1 |
| 2 `Var` | Variation rate | 0–100 % (= 0–1 mutations per bar) | per part (Bass 8) | 5 % | immediately |
| 3 `Part` | Part | Bass, Lead, MiSol, Solo | Bass | 1 | immediately (regenerates from seed with the part's default loops; bar position and passage counter kept, phases restart at 0); Density/Var keep the user's values (no per-part default reload) |
| 4 `Len` | Steps per bar (bar line, realignment cycle, loop fit, step row) | 2–32 (`kLenMin`..`kLenMax`) | 16 (`kDefaultLen`) | 1 | immediately (regenerates; `pulse_idx` wrapped into the new bar, passage counter kept, phases restart at 0) |
| 5 `Root` | Root (lowest key) | C..B, wraps | E (`kDefaultRoot`) | 1 semitone | immediately |
| 6 `Oct` | Octave offset (shifts all pitches by 12·Oct semitones) | −2…+3, shown `Oct +1` / `Oct 0` / `Oct -2` | 0 | 1 octave | immediately |
| 7 `Roll` | Re-roll every N bars (passage length) | Off, 1–8 bars; shows `Off` or `len:bar` | Off | — | setting immediately; the roll itself happens at the bar line that ends the passage |
| 8 `Seed` | Seed | 0–65535 (decimal) | 1 | any turn in edit mode = new random seed | immediately (`RerollNow`, same as the Roll input) |
| 9 `Clock` | Shown `Clock >`; **push opens the Clock page** (no edit mode) | — | — | — | — |

**Clock page** (applet-drawn sub-page, own cursor; settings `custom::LikembeClock`,
`custom/core/likembe_clock.h`). Turn = move between rows, push = edit (inverted) / leave
edit. `Back` + push returns to the main list with the cursor on `Clock`; while editing a
row, the other hemisphere's button (`AuxButton`) cancels the edit and also returns.

| Row | Function | Values | Default | Takes effect |
|---|---|---|---|---|
| `Src` | Step clock source | `Int` (= `Clock(0)`, upstream routing), `TR1`..`TR4` (= physical jack, rising edge of `frame.gate_high[n-1]`) | `Int` (`kSrcInt`) | next tick; changing it re-arms the divider |
| `Div` | Pulses per step, with `Src` = TRn only; shown `/3`; with `Int` shown `-` and not editable (push ignored) | 1–24 (`kClockDivMin`..`kClockDivMax`) | 1 (`kDefaultClockDiv`) | next pulse: decrement counter, fires on the first pulse after a reset; a Div lowered below the pulses still pending fires on the next pulse |
| `Roll` | Re-roll trigger input, physical jack (immediate re-roll; the main-list `Roll` bars setting is separate) | `Off`, `TR1`..`TR4` | own 2nd input: `TR2` left, `TR4` right (`kDefaultRollLeft`/`Right`) | next tick |
| `Rst` | Restart trigger input, physical jack: `Reset()` = bar start, phases 0, figures kept, divider re-armed | `Off`, `TR1`..`TR4` | `Off` (`kDefaultRst`) | next tick |
| `Back` | Return to the main list | — | — | — |

Roll and Rst on the same input both fire (re-roll first, then restart). Clock start always
restarts the sequence (upstream auto_reset calls `Reset()` on every applet, `docs/CLOCK.md`
1.2); `Rst` adds a trigger input for restarts.

## Engine: rhythm layer vs pitch layer (decision 2026-10-20)

`Len` defines the rhythmic feel; the thumb pitch loops stay polymetric.

- **Rhythm layer** (`LikembeCore::rhythm[kLenMax]`, one rest draw 0..255 per bar position,
  all 32 filled at generation so a Len change needs no new draw). Head = position `offset`
  of the part (`RhythmHead()`; positions before it are silent offset pulses): always plays and
  is accented (bar accent). Every other position i rests iff
  `min(255, rhythm[i] + bias) * 100 >= density * 256` (`RestAt`); bias = `kRestBiasLast`
  (default 40/255, lighter bar end) on the last position, `kRestBiasOther` (default 0)
  elsewhere. The saturation at 255 keeps density 100 rest-free. Density/CV1 is the live
  threshold. Since the draws are per bar position, the play/rest pattern repeats every bar.
- **Pitch layer** (per thumb, `LikembeElement` = degree only): thumb L plays even positions
  after the offset, R odd; the loop index advances per stroke slot (a rest mutes the slot, it
  does not hold the loop), phases advance Len/2 strokes at the bar line.
- `ev.accent` = bar head **or** loop element 0 (secondary accent); `ev.note = !RestAt(i)`.
- Draw order (`rng_part`, also in the lab port): thumb L degrees, thumb R degrees, then
  `rhythm[1..31]` ascending (`Next() >> 24` each); `rhythm[0] = 0`, no draw.
- Mutations: rest (`kMutRest`) toggles one rhythm position after the head (rest → draw 0, play
  → draw 255; the thumb pick draw is still consumed). Key, octave, loop length touch pitches
  only. The passage theme stores the rhythm too; return-to-theme restores it; `~` also shows a
  changed rhythm.

At 0 % density only the bar head plays.

Unused/fixed (constants): Tuning (pentatonic only, `kTuningCount`), return to theme
`kReturnToThemePct`, loop change `kLoopChangePct`, energy build `kEnergyBuildPct` (0),
register shift, hocket, start offset (per part). Buttons: upstream defaults only.
`Reset()` (shell reset) zeroes bar position and phases, keeps figures.

## Part profiles (`kParts` in `likembe_constants.h`)

| Part | Anchor | Degrees (n = 5) | Loop policy | Density | Variation | Offset | Loops Len 16 / Len 12 | Source |
|---|---|---|---|---|---|---|---|---|
| Bass | MIDI 45 (A2) | 0..8 (0–1.6 n) | one may drift | 85 % | 8 % (spec 0.075) | 0 | 4/3, 3/4 | spec |
| Lead / accomp. | MIDI 57 (A3) | 2..10 (0.4–2.0 n) | fit | 65 % | 25 % | 1 | 8/4, 6/3 | spec |
| Mi-solo | MIDI 60 (C4) | 3..11 (0.6–2.2 n) | fit | 55 % | 30 % | 1 | 4/8, 3/6 | **PROVISIONAL** |
| Solo | MIDI 64 (E4) | 5..13 (1.0–2.6 n) | one may drift | 45 % | 40 % | 3 | 8/5, 6/4 | **PROVISIONAL** |

Mi-solo/Solo follow Congolese guitar-band roles: mi-solo between accompaniment and solo in
register and busyness, solo highest, sparsest and most varied. To be tuned by ear.

**Loop lengths and Len.** A length n *fits* when 2n divides Len (each thumb plays Len/2
strokes per bar); if fewer than two lengths in `kMinLoop..kMaxLoop` fit (e.g. odd Len, Len 2)
every length counts as fit (`FitMask`). The default loops above apply at Len 16 and 12; at any
other Len the start loop lengths are rolled under the part policy from the passage stream
(`RollLoopLengths`), so they are deterministic per seed. With an odd Len the thumbs still
advance Len/2 (floored) strokes per bar.
The Solo anchor is chosen so the highest reachable degree (`key_hi + kOctaveHeadroom` = 15,
via octave mutation) at the worst root stays at 70 semitones above C2 = 5.83 V.

## Clock model

No tempo inside the core. Each `Pulse()` call plays one engine pulse; the core counts pulses
modulo Len and does the bar-line work (phase advance by pulses/2 mod n, then passage end
or variation) inside the call that ends the bar. Len/Part/Seed changes and re-rolls (Roll input,
Seed encoder) regenerate immediately in `SetParams()` / `Reroll()`; the bar position is kept
(`pulse_idx` wrapped to the new Len), so the clock never jumps. The Clock page settings
(`Src`, `Div`, `Roll`, `Rst`) are glue-only (`custom::LikembeClock`, `custom::PulseDivider`);
the core never sees them.
`Reset()` zeroes the bar position and phases and keeps the figures.

## Pitch mapping

`pitch_units = (midi - kMidiAtZeroUnits) * kUnitsPerSemitone`, clamped to
0..`kMaxPitchUnits` (6 V); a large Oct offset simply clamps there. Bass A2 = 1152 (0.75 V), lead A3 = 2688 (1.75 V). Degree →
MIDI per spec: `base + 12*(floor(d/n) + Oct) + offset[d mod n]`, base = root pitch class nearest
the anchor (−5..+6 semitones).

## Persistence (`OnDataRequest()`/`OnDataReceive()`, settings only, 62 of 64 bits)

Core `Pack()`/`UnpackParams()` (`likembe_core.h`) own bits 0–47; the glue ORs in the Clock
page (`LikembeClock::Pack()`/`Unpack()`, `likembe_clock.h`) at bits 48–61.

| Bits | Field | Validation |
|---|---|---|
| 0–6 | density_pct | clamp ≤ 100 |
| 7–8 | part | ≥ 4 → Bass (2 bits, always valid) |
| 9–12 | root | clamp ≤ 11 |
| 13–17 | len − 2 (0..30 = Len 2..32) | 31 → clamp to 32 |
| 18–24 | variation_pct | clamp ≤ 100 |
| 25–28 | passage_idx | clamp ≤ 8 |
| 29–31 | octave + 2 (0..5 = −2..+3) | 6, 7 clamp to +3 |
| 32–47 | seed | — |
| 48–50 | src (0 Int, 1–4 TR1–TR4) | 5–7 → Int |
| 51–55 | div − 1 (0..23 = Div 1..24) | 24–31 clamp to 24 |
| 56–58 | roll_in (0 Off, 1–4 TR1–TR4) | 5–7 → hemisphere default (TR2 / TR4) |
| 59–61 | rst (0 Off, 2–5 TR1–TR4; stored = `rst_in` + 1) | 1 (former `Int`), 6, 7 → Off |
| 62–63 | unused (0) | ignored |

Unpack regenerates everything from the seed (evolving state is not persisted).

## Browser lab

`docs/demo/likembe-lab.html` (open directly from disk, `file://`; no build, no network).
The pattern engine lives in `docs/demo/likembe_engine.js`, a 1:1 port of
`likembe_core.h` (same xorshift32 streams and seeding, integer math, draw order, thresholds,
mutation rules, bar-line handling, Oct/Root mapping, part profiles incl. the provisional
Mi-solo/Solo, passages). Given seed + parameters it produces the same event stream as the
module; `task host:parity` proves it (C++ `host/tools/likembe_dump.cpp` vs
`host/tools/likembe_lab_parity.mjs`, line format `bar pulse thumb degree rest accent pitch_units`).

- **Clock (top):** a model of the upstream ClockSetup (`docs/CLOCK.md`): `Start`/`Stop`
  (internal clock; Start resets every voice like upstream auto_reset), `BPM`, and per voice
  the multiplier of its hemisphere's first input slot (x1..x24, ÷2..÷32, off; labelled with
  note values, x4 = 1/16 default). Stopped or `off` = trigger map: slot n gets TRn. TR1..TR4:
  `pulse` button (one pulse) and `steady` (4 pulses per beat at the BPM, for testing
  Src TRn / Div). Voice 1 = left hemisphere (slots 1, 2), Voice 2 = right (slots 3, 4).
- **Two voices** = two applet instances. Per voice the main list (Dens, Var, Part, Len, Root,
  Oct, Roll, Seed) and the Clock page rows (Src, Div, Roll, Rst; Div shows `-` with Src Int)
  with the module's ranges and defaults; the per-frame clock handling is a port of
  `Likembe::Controller()` in the same order (Roll input, Rst input, step); `TRn` = the TR buttons / TR test clock only, never the lab's ClockSetup model, the divider is the
  shared `pulseDividerTick()` in `likembe_engine.js` (1:1 port of `custom::PulseDivider`).
  "re-roll" = Roll input / Seed encoder, immediate; typing a seed regenerates immediately. Mute per voice.
  Lab limitation: events are scheduled 120 ms ahead, so Stop or a setting change acts on the
  next unscheduled pulse.
- **Display:** a 4x canvas of the module display below the parameter line (tines, step row,
  running light, beat ticks, bar-cycle squares, status glyphs, L/R loop rows), drawn by a port of
  `Likembe::View`, plus a `bar` line (rhythm: `X` head, `x` plays, `.` rest) and a line per thumb (`X` element 0, `x` element, `[ ]` next element,
  degrees, phase, cycle, seed, roll counter, `~theme`, `+lab` = lab constants pending).
- **Sound:** tine synth, amp and room from the Congotronics demo. Velocity = Out A gate level
  / 5 V (accent 5 V = 1.0, normal 3 V = 0.6); note sustain = gate length.
- **Reference only (not in the firmware):** bass drum patterns and clave/bell/beat from the
  demo; they run with the internal clock on their own 4-beat bar (Ref grid 16 or 12 steps), independent of the voices (Beat = every 4 steps).
- **Lab / constants:** firmware constants with their C++ names, per part: `kStepWeight`
  (per-part table in the core), `kMutWeight`, `kRestBiasLast`, `kRestBiasOther`, `kReturnToThemePct`, `kLoopChangePct`,
  `kOctaveHeadroom`; global: `kThumbLStartMiddle`; global and live:
  `kGateAccentUnits`, `kGateNormalUnits` (as volts), `kGateLengthPct`. Engine constants apply at
  the next bar line (immediately when nothing is clocking). **Export constants** prints a snippet in
  `likembe_constants.h` format (`kStepWeight[kPartCount][5]`, `kThumbLStartMiddle`); a
  global-only value that differs between parts is printed as a per-part table with a note
  that the core needs a change for it.
- **Dump events:** N bars from Init with the voice's current parameters, in the parity format,
  headed by the equivalent `likembe_dump` arguments (flagged when lab values differ from firmware).

Workflow: tune by ear in the lab, Export, paste into `likembe_constants.h`, port the lab
default in `labDefaults()` (`likembe_engine.js`) to the same values, `task host:parity`, `task host:test`.

## Constants (`custom/core/likembe_constants.h`)

`kUnitsPerSemitone`, `kUnitsPerOctave`, `kMidiAtZeroUnits`, `kMaxPitchUnits`, `kTunings`,
`kTuningCount`, `kLenMin`, `kLenMax`, `kDefaultLen`, `kMinLoop`, `kMaxLoop`, `kParts`, `kStepDelta`,
`kStepWeight`, `kThumbLStartMiddle`, `kMutWeight`, `kRestBiasLast`, `kRestBiasOther`, `kOctaveHeadroom`, `kReturnToThemePct`, `kLoopChangePct`,
`kEnergyBuildPct`, `kDensityMin` (0), `kDensityMax`, `kOctaveMin`, `kOctaveMax`, `kDefaultOctave`, `kDensityCvRange`, `kVariationMax`, `kPassageBars`,
`kDefaultPassageIdx`, `kDefaultRoot`, `kDefaultSeed`,
`kPartStreamOffset`, `kPassageStreamOffset`, `kSeedMix`, `kSeedNonZero`, `kGateAccentUnits`,
`kGateNormalUnits`, `kGateLengthPct`, `kDefaultPulseTicks`, `kMaxPulseTicks`; Clock page:
`kSrcInt`..`kSrcCount`, `kClockDivMin`, `kClockDivMax`, `kDefaultClockDiv`, `kRollOff`..`kRollCount`,
`kDefaultRollLeft`, `kDefaultRollRight`, `kRstOff`..`kRstCount`, `kDefaultRst`.

- `kStepWeight[kPartCount][5]`: per part, relative odds of the key step (+1, -1, 0, +2, -2,
  order of `kStepDelta`) between consecutive strokes when a thumb figure is generated; the
  active part's row is used.
- `kThumbLStartMiddle`: thumb L's figure starts at the middle key (`c / 2`) like thumb R when
  true (current, all parts); lowest key (`0`) when false.

## Display (64×64, below the applet header)

| y | Element | Meaning |
|---|---|---|
| 14 | `Name value` | selected parameter; underline cursor (blinks), inverted in edit mode. Clock row: `Clock` + `>` at x 39, cursor under `>` |
| 24 | Bridge | 1 px line across the tines |
| 25–36 | Tines | one tine per key of the part (key_lo .. key_hi + octave headroom, max 15), 4 px column pitch, hanging from the bridge. Mapping follows the core's thumb split by absolute degree parity: even degree (thumb L) on the left from x 29 outward, odd degree (thumb R) on the right from x 33 outward, 4 px pitch, no shared centre tine; column within a side = (d − first same-parity degree ≥ key_lo) / 2, so each side's lowest degree is nearest the centre. Length 12 px in the centre, −1 px per column outward (min 6). Struck tine (Out A gate high): 3 px wide bar; accent additionally a 5×3 block at the tip. After the gate ends, the last struck tine keeps a 3 px foot under its tip until the next note |
| 38–43 | Step row | rhythm layer, this bar's positions (`RestAt`, `RhythmHead`), 4 px cells for Len ≤ 16, 2 px cells above (Len 32 = 64 px): tall bar = bar head (bar accent), short bar = plays, dot = rest or offset pulse |
| 44–45 | Running light | 2 px bar (cell width) under the pulse just played |
| 46 | Beat ticks | 1 px tick at the first pulse of each beat (every 4 pulses, `kBeatPulses`, for any Len); downbeat 2 px wide |
| 48–50 | Bar-cycle squares | one 3×3 square per bar of the realignment cycle N (4 px pitch, from x 0), current bar filled, others hollow; if N × 4 > 50: 1 px-wide markers (width 50/N) with the current one filled. Never reaches x 50 |
| 48–50, x 58 | Status | 3 px pixel glyphs: `~` (x 58) = figures differ from the passage theme |
| 52–56 | `L` + n cells | left thumb loop: 3×5 pixel label at x 0, 5×5 cells from x 5 at 7 px pitch (n = 2..8, ends x 58): filled = element played next, frame = other elements (pitches only; rests are shown in the step row) |
| 58–62 | `R` + n cells | right thumb, same; different widths = different loop lengths, the filled cell moves at half the pulse rate and starts at a different cell each bar when the loop does not fit |

**Clock page** replaces the whole view while open: five rows at y 14, 24, 34, 44, 54
(10 px pitch, last row ends at y 61): label at x 1 (`Src`, `Div`, `Roll`, `Rst`, `Back`),
value at x 34 (`Int`/`TR1`..`TR4`, `/n` or `-`, `Off`/`TRn`, `Off`/`Int`/`TRn`). Cursor =
upstream `gfxCursor`: blinking underline under the value (x 33–61), under the label on
`Back`; inverted value while editing. Verified with host screenshots (both hemispheres:
Roll default TR2 left, TR4 right).

**Bar counter.** N = lcm(2·nL, 2·nR, B) / B bars (B = pulses per bar, nL/nR = loop lengths in strokes) is the realignment cycle after which both thumbs start the bar on element 0 again; the part's start offset shifts both thumbs equally and does not change N. Bass Len 16 L4/R3: 3; Bass Len 12 L3/R4: 2; fitting loops (Lead): 1. i = bar playing now, 1..N (the filled square). Rule (core `SyncCycle`): N is recomputed and i restarts at 1 on `Reset()`, whenever figures are generated (Init, Len/part/seed change, re-roll, new passage -- phases restart at 0) and when a mutation or return-to-theme changes a loop length; otherwise i advances at each bar line and wraps N -> 1. Max reachable N is 112 (L7/R8 at a Len coprime to 14 and 16, e.g. 31; segment fallback above 12 bars).

Verified screenshots (sim, not hardware): `host/build/out/tv_{bass3,lead3,cycle3}/` (current layout); earlier tine-only layout: `host/build/out/tv_bass/` (Bass, accented note, gate high), `tv_bass_after/` (gate low, foot marker), `tv_lead/` (left = Lead), `tv_12/` (Len 12); Len 12/16/24/32 step rows: `ui_002..005_oled_x6.png` (2026-10-19), `tv_cycle/` (seed 14572, N = 7, `~`).
