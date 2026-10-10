# Phazerville — Research Notes

Distilled upstream facts. Paths relative to `firmware/software/src/` (vendored `T32`,
commit `9c870503`).

## Lineage

mxmxmx **Ornament & Crime** (original hardware + firmware, Teensy 3.2) →
Chysn **Hemisphere Suite** (split-screen applet framework, `APP_HEMISPHERE.h`) →
benirose **Benisphere** (more applets, presets) →
djphazer **Phazerville Suite** (https://github.com/djphazer/O_C-Phazerville,
docs: https://firmware.phazerville.com, wiki on GitHub).

## Branches

| Branch | Target | Notes |
|---|---|---|
| `T32` | Teensy 3.2 | Maintenance line: deprecated but kept for custom builds, occasional applet updates. **We track this.** |
| `main` | Teensy 4.x / ORN8 (+ T3.2 legacy) | Diverges heavily (more slots, 8-channel I/O) |
| `main-dev` | Teensy 4.x | Active development; e.g. clock refactor to microseconds |

Our subtree workflow and local edit list: `docs/UPSTREAM.md`.

## Build

PlatformIO, `firmware/software/platformio.ini`; envs such as `T32`, `T32_vor`, `stock`,
`custom` (board `teensy31`, 120 MHz). Applet selection in `hemisphere_config.h` via
`AppletRegistry reg{ DeclareApplet<Class>{id, categories}... }` (`:113-153`). Flash is the
binding constraint on T3.2, which is why stock builds juggle applet subsets.
Our envs (`custom_min`, `custom_min_vor`) and numbers: `docs/BUILD.md`.

## Hardware constants

- Core ISR `OC_CORE_ISR_FREQ = 16666` Hz, 60 µs period (`OC_config.h:18-19`).
- `ONE_OCTAVE = 12 << 7 = 1536` units per volt (1/128 semitone) (`HSUtils.h:13`);
  `HEMISPHERE_MAX_CV`, `HEMISPHERE_MIN_CV`, `HEMISPHERE_3V_CV` (`HSUtils.h:15-18`).
- `HEMISPHERE_CLOCK_TICKS = 17` ≈ 1 ms (`HSUtils.h:22`).

## Applet API — `HemisphereApplet.h`

Pure virtuals (`:87-103`, `:652`): `applet_name()` (≤ 9 chars), `Start`, `Controller`,
`View`, `OnDataRequest`, `OnDataReceive`, `OnEncoderMove`, `SetHelp`; optional `Reset`,
`OnButtonPress` (default: toggle cursor), `Unload`, `DrawFullScreen`, `AuxButton`,
`applet_icon`. Helpers: `In(ch)`, `Out(ch, v)`, `Clock(ch)`, `Gate(ch)`, `ClockOut(ch)`,
`GateOut(ch, on)`, `gfx*` drawing (auto x-offset for the right hemisphere, `:322`),
`Pack/Unpack(data, PackLocation{offset, bits}, value)`. State = one `uint64_t` per applet.

References: `applets/Boilerplate.txt` (template — `.txt`, not `.h`), `applets/Voltage.h`,
`applets/Slew.h`, `applets/RndWalk.h`.

## Clock

`HSClockManager.h` (`HS::clock_m`), UI in `applets/ClockSetup.h`, delivery in
`HemisphereApplet.cpp:117-140`, trigger map in `CVInputMap.h:96`. Full analysis:
`docs/clock/MASTER_CLOCK_CONCEPT.md`.

## Applet inventory (T32, 95 files in `applets/`)

LFO/modulation: EbbAndLfo, VectorLFO/EG/Mod/Morph, ADSREG, ADEG, ASR, Relabi, LowerRenz,
RunglBook, RndWalk, Stairs, Shredder, BitBeat, Slew, EnvFollow.
Random/algorithmic: TwoRings, TB3PO, Palimpsest, MarkoV, MarkovPerc, ProbabilityDivider,
ProbabilityMelody, Pigeons, EnigmaJr, DrumMap, EuclidX, EuclidO, Carpeggio, GameOfLife, ShiftReg.
Clocking: ClockDivider, PolyDiv, ClockSkip, Burst, GateDelay, Shuffle, Metronome, Brancher,
ResetClock, ClkToGate.
Quantizers: DualQuant, ScaleDuet, Squanch, MultiScale, OffsetQuant, DuoTET.
Utilities: Voltage, AttenuateOffset, Logic, Switch, Scope, Calculate, Compare, Xfader, Tuner, ...
Full list: `ls firmware/software/src/applets/` and `hemisphere_config.h`.

## Gotchas (found during scaffolding)

- With `-DUSB_MIDI` the Teensy enumerates without a USB serial tty (HID "seremu" only).
  Workarounds: an env with `-DUSB_MIDI_SERIAL`, or `teensy_serialmon`.
- Hiding every applet in the show/hide menu makes `get_next_applet_index` loop forever
  (upstream behavior). Relevant with a one-applet registry: never hide the last visible applet.
- `src/drivers/weegfx.cpp:533` defines `print(uint32_t, size_t)` while `weegfx.h` declares
  `print(uint32_t, unsigned)`: harmless on 32-bit Teensy, breaks 64-bit host builds. The host
  harness compiles a patched copy.

## Host simulation

None upstream. `src/drivers/weegfx.*` renders into a plain 1-bit 128x64 buffer with no
hardware dependency; our harness reuses it. See `docs/HOST_SIM.md`.

## Licensing

File headers mostly MIT; a few core files are GPLv3 → distributed binaries treated as
GPLv3. Details and file list: `docs/UPSTREAM.md`.
