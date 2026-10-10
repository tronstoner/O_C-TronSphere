# Project

Custom Ornament & Crime firmware for Teensy 3.2, forked from Phazerville Suite branch `T32`.

## Goals

- A **minimal** build: Hemisphere shell + the user's own applets only. Upstream applets are
  opt-in, one at a time, on explicit decision.
- A **master clock** that is easy to reach (global tempo) and offers polymetric clock buses
  (ratios such as 3:2, 5:4, 7:8, plus phase for synced modulation) that any applet can follow
  without spending an applet slot.
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
| `docs/clock/MASTER_CLOCK_CONCEPT.md` | concept | Clock buses proposal |
| `docs/modules/README.md` | current | Module index + overlapping upstream applets |
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
- Host gaps: one applet on one side; no ClockSetup/internal clock; no MIDI/EEPROM.
- `custom_min` currently 92,836 B flash / 20,472 B RAM.

### Open decisions
- License for own code: GPLv3 for the whole repo (matches the combined work and NitroTron3)
  vs MIT for `custom/` — user decision pending.
- Open questions in `docs/clock/MASTER_CLOCK_CONCEPT.md` §6 and
  `docs/modules/evolving-voltage-concept.md` §6.

## Next Steps

See `docs/ROADMAP.md`. Immediately next: flash `custom_min` on hardware and verify the Hello
applet (Phase 1 remainder); then the user decides the license and the open questions in the
clock and module concept docs.

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
