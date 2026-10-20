# Project

Custom Ornament & Crime firmware for Teensy 3.2, forked from Phazerville Suite branch `T32`.

## Goals

- A **minimal** build: Hemisphere shell + the user's own applets only. Upstream applets are
  opt-in, one at a time, on explicit decision.
- Clock: upstream as is.
- A small number of carefully designed own modules: an evolving random voltage + trigger
  source (Vestige-derived), other algorithmic voltage sources, a better LFO.
- Host-first development: pure logic in `custom/core`, unit-tested and simulated on the host.

## Non-goals

- Teensy 4.x / ORN8 support; tracking upstream `main`/`main-dev`.
- Feature parity with stock Phazerville; the full applet catalogue; the classic O_C apps
  beyond what the Hemisphere shell needs.
- Batch-porting or bulk-enabling applets.

## Document Map

| Doc | Status | Content |
|---|---|---|
| `AGENTS.md` | current | Agent entry point, hard rules, coding rules |
| `README.md` | current | Human overview |
| `docs/PROJECT.md` | current | This file |
| `docs/ROADMAP.md` | current | Phases with "done when" criteria |
| `docs/DESIGN_PRINCIPLES.md` | current | Minimal philosophy, module process, 64x64 UI rules |
| `docs/ARCHITECTURE.md` | current | How the O_C/Hemisphere firmware works; where our code hooks in |
| `docs/BUILD.md` | current | Build, flash, envs, size budget (maintained by build work) |
| `docs/UPSTREAM.md` | current | Subtree workflow, local edits to `firmware/`, licensing |
| `docs/HOST_SIM.md` | current | Host test harness and simulator |
| `docs/CLOCK.md` | current | Upstream clock layers (trigger map, ClockManager) with file:line refs; applet clock rules; host coverage |
| `docs/clock/MASTER_CLOCK_CONCEPT.md` | shelved | Clock buses proposal (reference only) |
| `docs/modules/README.md` | current | Module index + overlapping upstream applets |
| `docs/modules/likembe.md` | current | Likembe module doc (as built, hardware pending) |
| `docs/modules/likembe-impl-plan.md` | current | Likembe steps, API, doc/JS deviations, open questions |
| `docs/research/Likembe pattern engine technical concept.md` | reference | Likembe pattern engine spec |
| `docs/research/Congotronics line generator.html` | reference | JS reference implementation of the pattern engine |
| `docs/demo/likembe-lab.html` | current | Likembe browser lab: two voices, module display, sound, lab constants + export, event dump |
| `docs/demo/likembe_engine.js` | current | 1:1 JS port of `likembe_core.h` used by the lab (parity-checked) |
| `host/tools/likembe_dump.cpp`, `likembe_lab_parity.mjs`, `parity.sh` | current | Likembe C++/JS parity check (`task host:parity`) |
| `docs/modules/evolving-voltage-concept.md` | concept | Vestige-derived evolving voltage/trigger module |
| `docs/research/phazerville-notes.md` | current | Upstream facts, lineage, applet API, inventory |
| `docs/research/nitrotron3-lessons.md` | current | What carries over from the NitroTron3 project |

## Current Status

- Upstream `T32` vendored as subtree in `firmware/`.
- Docs tree and agent rules written.
- Build scaffold done: `custom/` with Hello applet (ID 200, core `custom/core/hello_core.h`),
  registry `custom/hemisphere_config_custom.h`, env `custom_min`. PlatformIO installed via brew.
- `Taskfile.yml`: `build`, `size`, `clean`, `upstream-fetch` verified; `flash`, `catch`,
  `monitor` not yet run (no hardware connected).
- Size: `custom_min` 92,828 B flash (35.4%) / 20,472 B RAM (31.2%), vs upstream `T32`
  225,340 B (86.0%) / 38,156 B (58.2%).
- Under `CUSTOM_BUILD` Ponglet is dropped; no `ENABLE_APP_*` flags needed (Setup/About,
  Hemispheres, Scale/Waveform editors, Backup/Restore are always compiled by `OC_apps.cpp`).
- Hello applet not yet verified on hardware.
- Host harness done (`docs/HOST_SIM.md`): Tier 1 core tests (`task host:test`, 5 pass),
  Tier 2 weegfx render (`task host:render`, PBM/PGM), Tier 3 applet harness (`task host:sim`)
  compiling the real `HemisphereApplet.cpp`/`HSUtils.cpp`/`HSIOFrame.cpp` against `host/stub/`.
  SDL interactive sim planned, not built.
- Host gaps: one applet per side, no HemisphereManager/select mode; no MIDI/EEPROM. The real
  ClockSetup screen is shown and operable in the sim (Up+Down or `G`; scenario `clocksetup_screen`).
  Since 2026-10-20 the real `HS::clock_m` + `ClockSetup::Controller()` run in the sim
  (internal clock, multipliers, PPQN/TR1 sync; `docs/HOST_SIM.md`).
- `custom_min` currently 92,836 B flash / 20,472 B RAM.
- Likembe (Phase 3): core + glue (`custom/applets/Likembe.h`, ID 201) implemented, host tests
  pass, sim-verified (I/O timing, CV1 density, screenshots). Not yet on
  hardware. Density 0–100 %, Oct parameter (−2…+3). 2026-10-19 (host-verified, not sim/hardware):
  all setting changes and TR2 re-roll immediate (no pending state), `Pass` renamed `Roll`,
  new `Clk` source (Auto/TR1..TR4, bits 44–46), menu order Dens, Var, Part, Root, Oct, Grid, Roll, Seed, Clk.
  `custom_min` now 98,660 B flash / 20,784 B RAM (+72 B flash). Mi-solo/Solo profiles provisional.
  2026-10-19 (host + sim-verified, not hardware): `Grid` replaced by `Len` (pulses per bar 2..32)
  and `Div` (clock division 1..8); menu Dens, Var, Part, Root, Oct, Len, Div, Roll, Seed, Clk;
  packed layout re-laid out (54 bits). `custom_min` 98,724 B flash / 20,792 B RAM (+64 B / +8 B).
  2026-10-20 (host + sim-verified, not hardware): **Clock page** replaces `Div` and `Clk`. Main list
  Dens, Var, Part, Len, Root, Oct, Roll, Seed, `Clock >`; page rows Src (Int/TR1-4), Div (1-24, TRn only),
  Roll (re-roll input Off/TR1-4, default own 2nd input), Rst (Off/TR1-4 since the TRn change below), Back. Settings packed in
  bits 48-61 (`custom/core/likembe_clock.h`), core re-laid out to bits 0-47. Rules: `docs/CLOCK.md`.
  `custom_min` 99,476 B flash / 20,824 B RAM (+752 B / +32 B).
  2026-10-20 (host + sim-verified, not hardware): `TRn` on the Clock page = physical jack (rising
  edge of `frame.gate_high`), ClockSetup no longer affects Src/Roll/Rst TRn; `Rst` = Off/TR1-4
  (default Off, `Int` removed; clock start always restarts via upstream auto_reset).
  `custom_min` 99,492 B flash / 20,824 B RAM (+16 B / 0 B).
  2026-10-20 (host + sim-verified, not hardware): `Src Int` = internal clock 24 PPQN
  (`clock_m.Tock(MIDI_CLOCK)` while running), identical on both hemispheres, slot multipliers
  irrelevant; `Div` for every source, stored per source (`div_int` 1-24 default 6 = 1/16,
  `div_tr` 1-16 default 1), note names with Src Int (`Div 6 1/16`); defaults equal on both
  sides (Roll TR2). Clock block bits 48-63 (base-5 Src/Roll/Rst field). Lab: BPM + Start/Stop,
  no per-voice multipliers. `custom_min` 99,732 B flash / 20,824 B RAM (+240 B / 0 B, `task size`).
  2026-10-20 (host + sim-verified, not hardware; supersedes the entry above): `Src Int` =
  this hemisphere's ClockSetup slot tock (`clock_m.Tock(io_offset)`); the Clock page `Div`
  with Src Int **is** the slot multiplier (read/written via `clock_m`, list /4 /3 /2 x1..x8 x12
  with note names, e.g. `Div x4 1/16`); an "off" slot is set to x4 on load / Src→Int (the
  applet writes to ClockSetup, which persists the value). Src TRn Div = 1..8, 12, 16. Clock
  block 13 bits at 48–60. Lab: slot multipliers back, shared with the voice Div.
  `custom_min` 100,052 B flash / 20,824 B RAM (+320 B / 0 B, `task size`).
  2026-10-20 (host + sim-verified, not hardware): one clock multiplier list in ClockSetup and
  the applet (`custom/core/clock_mult_list.h`: off /4 /3 /2 x1..x8 x12). `ClockSetup.h` edited
  under `CUSTOM_BUILD`: both encoders step a slot multiplier through the list. Applet Src Int
  Div steps the same list without off; ClockSetup notation only, note names removed (applet
  and lab). Src TRn Div = 1..8, 12. `custom_min` 99,788 B flash / 20,824 B RAM (-264 B / 0 B).
  2026-10-20 (host + sim-verified, not hardware): list widened to off /32 /24 /16 /12 /8../2
  x1..x8 x12 x16 x24 (ClockSetup and Src Int Div); Src TRn Div /1../8 /12 /16 /24 /32 in
  division notation. `custom_min` 99,820 B flash / 20,824 B RAM (+32 B / 0 B).
- Likembe browser lab (`docs/demo/likembe-lab.html`): JS engine matches the C++ core event for event (`task host:parity`, 18 combinations incl. Len 1/2/7/12/16/24/32). `Len` range is 1..32 (Len 1 allowed 2026-10-20; offset parts silent there).

### Open decisions
- Open questions in `docs/modules/evolving-voltage-concept.md` §6.

## Next Steps

See `docs/ROADMAP.md`. Immediately next: user answers the open questions in
`docs/modules/likembe-impl-plan.md`; flash `custom_min` and verify Likembe on hardware
(Src Int = slot multiplier on both sides, both applets start with the clock, preset load order applet vs ClockSetup, TRn = physical jack with the internal clock running, two instances Bass + Lead, display readability, tune provisional profiles).
Hello hardware check (Phase 1 remainder) can be done with the same flash.

## Decisions

| Date | Decision | Why |
|---|---|---|
| 2026-10 | Fork as git subtree of Phazerville branch `T32` | Only maintained line for Teensy 3.2; subtree keeps upstream history pullable without submodule friction |
| 2026-10 | Minimal build; upstream applets opt-in one at a time | Stock builds are overwhelming; frees flash for own work |
| 2026-10 | Split `custom/core` (pure logic) vs `custom/applets` (glue) | Host testability; keeps hardware out of algorithms |
| 2026-10 | Host-first testing | Faster iteration than flash-and-listen; hardware reserved for timing and feel |
| 2026-10 | go-task (`Taskfile.yml`) as the CLI | Same pattern as NitroTron3; one discoverable entry point |
| 2026-10 | Own applet IDs start at 200 | No collision with upstream IDs stored in presets |
| 2026-10 | Host stubs via `-include` prelude + include-guard pre-definition, not only include-path shadowing | Quoted includes resolve next to the including file first, so shadowing alone cannot replace sibling HAL headers (see `docs/HOST_SIM.md`) |
| 2026-10 | Phase 3 = Likembe, one part per applet instance; evolving voltage moves after it | User decision |
| 2026-10-10 | License: GPLv3 (GPL-3.0-or-later) for the whole repo, `LICENSE` at root | Combined work is GPLv3 anyway (4 GPLv3 upstream files); matches NitroTron3; repo may be published on GitHub. Details `docs/UPSTREAM.md` |
| 2026-10-19 | A setting change is audible immediately; no waiting for the next bar/step unless the module doc states otherwise | Turning a knob must give instant feedback; deferred changes feel broken. Likembe: Len (was Grid)/Part/Seed and TR2 re-roll now immediate |
| 2026-10-19 | No master clock rework; use upstream clock + trigger map | Existing per-channel mult/div and in-applet division cover the needs; keeps `firmware/` edits at 2 files |
| 2026-10-20 | `Src Int` = the hemisphere's ClockSetup slot tock (`clock_m.Tock(io_offset)`); its `Div` row is the slot multiplier itself (read/written via `clock_m`, curated list /4 /3 /2 x1..x8 x12 with note names); off slot → applet sets x4; Src TRn Div = 1..8, 12, 16 | One clock division, not two layers; both applets start with the internal clock; fewer, musical values. Supersedes the same-day 24-PPQN `Tock(MIDI_CLOCK)` + `div_int` decision. Details `docs/CLOCK.md` 2 |
| 2026-10-20 | TRn in applet clock settings = physical jack, not ClockSetup-routed | `Src`/`Roll`/`Rst TRn` read rising edges of `frame.gate_high[n-1]`; `Int` is the only routed source; `Rst Int` dropped (clock start restarts anyway) |
| 2026-10-20 | Clock page model: applets step once per pulse; `Src Int` = upstream routing (`Clock(0)`), `Src TRn` = `frame.clocked[n-1]` + pulse counter only (TRn superseded by the physical-jack row above); Roll/Rst trigger inputs per applet; applet-drawn sub-page with `Back` row | No tempo logic in applets; one place (ClockSetup) owns tempo/multipliers. Details `docs/CLOCK.md` |
| 2026-10-20 | Real `ClockSetup`/`HS::clock_m` compiled into the host sim | `Src Int` / `Rst Int` / external sync become host-testable without stubbing clock behaviour |
| 2026-10-20 | Multiplier list widened to off /32 /24 /16 /12 /8../2 x1..x8 x12 x16 x24; Src TRn Div shown /1../32 | Wider range in both places, same notation for TR division. Details `docs/CLOCK.md` 2 |
| 2026-10-20 | One multiplier list in ClockSetup and applet; ClockSetup.h edited under CUSTOM_BUILD | Same values and steps in both places, encoders step the list (off /4 /3 /2 x1..x8 x12), no note names. Details `docs/CLOCK.md` 2, `docs/UPSTREAM.md` |
