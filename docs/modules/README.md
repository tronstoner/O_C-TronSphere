# Modules

Own modules, one at a time (see `docs/DESIGN_PRINCIPLES.md` §3), and the upstream applets
that overlap each idea — so the user can try or compare before building. Upstream paths
are in `firmware/software/src/applets/`. None of them is enabled in our build.

## Own modules

| Module | Status | Doc |
|---|---|---|
| Likembe (one part per applet) | core + host tests done, glue pending | `likembe.md`, `likembe-impl-plan.md` |
| Evolving voltage (name TBD) | concept (after Likembe) | `evolving-voltage-concept.md` |
| Algorithmic voltage sources | idea | — |
| Better LFO | idea | — |

## Upstream overlap

### Random / evolving CV and triggers
- `Palimpsest.h` — accent sequencer that builds up/erodes a pattern over time.
- `TwoRings.h` — dual Turing machine (shift-register random looping).
- `ShiftReg.h` — Turing-machine style shift register.
- `Stairs.h` — stepped random/ordered voltage staircase.
- `Shredder.h` — random 2-channel CV sequence, re-shreddable.
- `RndWalk.h` — clocked random walk with smoothing.
- `MarkoV.h`, `MarkovPerc.h` — Markov-chain melodies / percussion.
- `TB3PO.h` — 303-style random acid sequencer with density.
- `ProbabilityDivider.h`, `ProbabilityMelody.h` — probabilistic clock divider / melody.
- `EnigmaJr.h` — Turing-machine player.
- `RunglBook.h` — Rungler (Benjolin-style chaos shift register).
- `DrumMap.h` — Grids-style drum map; `EuclidX.h` — Euclidean patterns.
- `BitBeat.h`, `Pigeons.h`, `GameOfLife.h`, `Trending.h` — other algorithmic sources.

### LFO / modulation
- `EbbAndLfo.h` — Tides-like LFO/envelope with shape/slope.
- `VectorLFO.h`, `VectorEG.h`, `VectorMod.h`, `VectorMorph.h` — waveform-table modulators.
- `LowerRenz.h` — Lorenz attractor; `Relabi.h` — relabi-style coupled oscillators.
- `ADSREG.h`, `ADEG.h`, `ASR.h` — envelopes; `Slew.h`, `EnvFollow.h`.

### Clocking (relevant to `docs/clock/MASTER_CLOCK_CONCEPT.md`)
- `ClockDivider.h`, `PolyDiv.h`, `ClockSkip.h`, `Burst.h`, `GateDelay.h`, `Shuffle.h`,
  `Metronome.h`, `Brancher.h`, `ResetClock.h`, `ClkToGate.h`.
