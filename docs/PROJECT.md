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
- Host gaps: one applet per side, no HemisphereManager/select mode/ClockSetup UI; no MIDI/EEPROM.
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
  Roll (re-roll input Off/TR1-4, default own 2nd input), Rst (Off/Int/TR1-4), Back. Settings packed in
  bits 48-61 (`custom/core/likembe_clock.h`), core re-laid out to bits 0-47. Rules: `docs/CLOCK.md`.
  `custom_min` 99,476 B flash / 20,824 B RAM (+752 B / +32 B).
- Likembe browser lab (`docs/demo/likembe-lab.html`): JS engine matches the C++ core event for event (`task host:parity`, 16 combinations incl. Len 2/7/12/16/24/32).

### Open decisions
- Open questions in `docs/modules/evolving-voltage-concept.md` §6.

## Next Steps

See `docs/ROADMAP.md`. Immediately next: user answers the open questions in
`docs/modules/likembe-impl-plan.md`; flash `custom_min` and verify Likembe on hardware
(clock on TR1, two instances Bass + Lead, display readability, tune provisional profiles).
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
| 2026-10-20 | Clock page model: applets step once per pulse; `Src Int` = upstream routing (`Clock(0)`), `Src TRn` = `frame.clocked[n-1]` + pulse counter only; Roll/Rst trigger inputs per applet; applet-drawn sub-page with `Back` row | No tempo logic in applets; one place (ClockSetup) owns tempo/multipliers. Details `docs/CLOCK.md` |
| 2026-10-20 | Real `ClockSetup`/`HS::clock_m` compiled into the host sim | `Src Int` / `Rst Int` / external sync become host-testable without stubbing clock behaviour |
