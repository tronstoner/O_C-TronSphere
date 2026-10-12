# Roadmap

One phase at a time; each phase ends with docs updated and the user's sign-off.

## Phase 0 — Scaffold (now)
Upstream subtree, `custom/` layout, `custom_min` env, Taskfile, docs tree, agent rules.
**Done when:**
- [x] `task build` produces a minimal Hemisphere firmware (`custom_min`, shell + Hello applet).
- [x] `task size` reports flash/RAM (92,828 B / 20,472 B).
- [x] Docs tree complete and indexed in `docs/PROJECT.md`.
- [ ] `task flash` / `task catch` exercised on hardware (moves with Phase 1).

## Phase 1 — Host harness + Hello applet
Stub HAL headers, plain `g++` core tests (`task host:test`), first trivial applet (ID 200)
with a core in `custom/core/`. Optional: weegfx rendering of `View()` to an image.
**Done when:**
- [x] A core unit test runs on the host (`task host:test`).
- [x] weegfx renders `View()` on the host (`task host:render`); applet harness runs Hello (`task host:sim`).
- [x] BUILD/HOST_SIM docs match.
- [ ] Hello flashes, shows on the OLED, reads an input and drives an output on hardware.

## Phase 2 — Master clock buses
Per `docs/clock/MASTER_CLOCK_CONCEPT.md`, after the user answers its open questions.
**Done when:** N buses with polymetric ratios run from internal and external clock without
drift (host test + scope on hardware); any applet's `Clock(ch)` can follow a bus via the
input map; tempo reachable in one gesture; settings persist as decided (global/per preset).

## Phase 3 — Likembe (one part per applet)
Per `docs/modules/likembe.md` / `likembe-impl-plan.md`: core → host tests → glue → visualization → hardware.
**Done when:**
- [x] Core + constants implemented, host tests pass (`task host:test`).
- [ ] Glue applet with controls, I/O and visualization (loops of different lengths visible).
- [ ] Runs clocked from TR1 on hardware, two instances (e.g. Bass + Lead) interlock; module doc as built.

## Phase 4 — Evolving voltage module
Per `docs/modules/evolving-voltage-concept.md` → impl plan → host → hardware.
**Done when:** the module runs bus-clocked on hardware, L = 0 is steady and consumes no
randomness, evolution per pass audibly musical, module doc with full controls table.

## Phase 5 — LFO
Concept first (compare `EbbAndLfo`, `VectorLFO`), bus-phase-synced via `Phase(bus)`.
**Done when:** concept decided, implemented, phase-locked to buses verified on hardware.

## Phase 6+ — One module at a time
Further own modules or individual upstream applets, each on explicit decision, each with
its own entry in `docs/PROJECT.md` § Decisions and a flash-budget check.
