# Agent Entry Point

This file provides guidance to AI agents when working with code in this repository.

Custom firmware for the **Ornament & Crime** (O_C) eurorack module on **Teensy 3.2**
(MK20DX256, Cortex-M4 @ 120 MHz, **no FPU**, 256 KB flash, 64 KB RAM), built on the
Phazerville Suite (Hemisphere framework). Hardware: 128x64 SH1106 OLED (two 64x64
hemispheres), 4 CV in, 4 trigger in, 4 CV out, 2 push encoders, 2 buttons.

## Read first, always

1. `docs/PROJECT.md` — goals, non-goals, status, next steps, **Document Map** (indexes every doc).
2. `docs/DESIGN_PRINCIPLES.md` — minimal-build philosophy and the one-module-at-a-time process. **Non-negotiable.**
3. The docs relevant to your task (below). Do not propose changes before you understand the relevant concept doc.

## Read for specific tasks

| Task | Read |
|---|---|
| Building, flashing, env names, flash/RAM budget | `docs/BUILD.md` |
| Touching `firmware/`, pulling upstream, licensing | `docs/UPSTREAM.md` |
| How the firmware works (ISR, IOFrame, applets, display, presets) | `docs/ARCHITECTURE.md` |
| Host tests / simulator | `docs/HOST_SIM.md` |
| Clock buses (shelved, reference) | `docs/clock/MASTER_CLOCK_CONCEPT.md` |
| A specific module | `docs/modules/README.md` → that module's docs |
| Upstream facts (branches, applet API, inventory) | `docs/research/phazerville-notes.md` |
| Phasing / what comes next | `docs/ROADMAP.md` |

Any other topic: check `docs/PROJECT.md` § Document Map before assuming a doc does not exist.

## Philosophy — hard rules

- **Absolute minimum.** The build carries the Hemisphere shell plus the user's own applets
  and nothing else. Upstream applets are **opted out by default**.
- **Upstream applets are added one at a time, each on the user's explicit decision.** Never
  batch-enable applets "because they fit", never re-enable one to make something compile or
  to demo something. Propose; the user decides.
- **One module at a time.** Process: proposal → concept doc → user decision → impl plan →
  host test → hardware. No parallel module development, no speculative modules.
- **Inside a module: few, well-understood controls.** One main knob/CV per module is the
  default. Every extra parameter needs a reason the user agreed to.
- **A setting change is audible immediately.** No waiting for the next bar/step unless the
  module doc states otherwise (decided 2026-10-19).
- Clock: use the upstream ClockSetup/trigger map as is. No clock rework (decided 2026-10-19).
  Applets derive their own divisions; `HS::clock_m` may be read directly.

## Repository layout

```
ornamentandcrime/
├── AGENTS.md, README.md, Taskfile.yml   # entry points; `task --list` for the CLI
├── firmware/            # git subtree of djphazer/O_C-Phazerville, branch T32 (upstream)
│   └── software/        #   platformio.ini, src/ (Main.cpp, APP_HEMISPHERE.h, applets/, ...)
├── custom/
│   ├── core/            # pure, hardware-free logic (*.h) — host-testable
│   ├── applets/         # thin HemisphereApplet glue (*.h) around a core
│   └── hemisphere_config_custom.h   # our applet registry (replaces upstream selection)
├── host/                # host test harness + simulator (see docs/HOST_SIM.md)
├── tools/               # helper scripts (serial capture, size reports, ...)
└── docs/                # source of truth: specs, concepts, plans, research
```

- **`firmware/` is upstream.** Edits there are minimal, delimited, and justified. Every
  edit is wrapped in `// --- custom build ---` / `// --- end custom build ---` (in
  `platformio.ini`: `; --- custom build ---` / `; --- end custom build ---`) and listed
  in `docs/UPSTREAM.md` so subtree pulls stay mergeable. Prefer adding code in `custom/`
  and hooking it in with the smallest possible change.
- **`custom/core` vs `custom/applets`.** Cores contain the algorithm (state, step, mutate,
  pack/unpack) with no Arduino/Teensy/O_C includes, so they compile with plain `g++` on the
  host. Applets translate hardware I/O (In/Out/Clock/encoders/View) to core calls. This split
  is what makes host-first testing possible; never let hardware types leak into a core.

## Workflow rules

### Documentation is source of truth
- Docs must match the **implemented** code after every session; planned behavior is marked
  as planned. Update `docs/PROJECT.md` (Current Status, Next Steps) and the affected module
  doc (incl. its full controls table) before considering work done. Not a follow-up — part of the task.
- `docs/PROJECT.md` § Document Map indexes **every** doc and marks it current or historical.
  Add an entry whenever you add a doc.
- Topic docs at `docs/` root are `UPPER_SNAKE.md`; docs in subfolders are `kebab-case.md`.
- Tuning values live in constants headers (`custom/core/<module>_constants.h`), not in docs
  and not as literals scattered in code. Docs reference the constant name.

### Decision-making
- **Ask before coding a fix or change.** Describe the problem, list 2–3 concrete options,
  wait for the user's choice. Exception: trivial numeric or obvious choices — just pick.
- **Measure before theorising.** Intermittent fault → get data first (serial capture, host
  repro); never bolt recovery logic onto an unexplained fault.
- **Host harness for logic, hardware for timing and feel.** Cores are verified on the host
  first; anything timing-, voltage- or UX-related is only "verified" on hardware.
- When translating a prototype/sim to firmware, add a "Firmware translation" section to the
  impl plan listing every execution-model divergence (event vs polling, exact vs noisy
  inputs, atomic vs multi-tick state changes).

### Git
- **Only the main agent commits**, only on the current feature branch. **Commit savepoints
  during work** (after each coherent step that builds, at the latest when host tests pass);
  uncommitted work in progress is a risk, not a courtesy (user decision 2026-10-19). Commit
  messages state the verification level honestly:
  `savepoint, untested` / `host-verified` / `verified on hardware`.
- Subagents never run `git commit`, `git push`, `git rebase`, `git config`; they leave
  changes uncommitted and report.
- **Never `git push`.** Never add `Co-Authored-By` lines. Never bypass signing or hooks.
- No git worktrees. Work in the existing checkout.

## O_C coding rules

- **No FPU.** The Teensy 3.2 Cortex-M4 (MK20DX256) has no hardware floating point; `float`
  is software-emulated and slow. Use integer / fixed-point math in anything that runs per
  tick. Floats are tolerable only in rare UI or init paths. Upstream conventions: pitch CV
  in 1/128 semitone units, `ONE_OCTAVE = 1536` (`firmware/software/src/HSUtils.h:13`).
- **`Controller()` runs in the core ISR context at 16666 Hz** (`OC_CORE_ISR_FREQ`,
  `firmware/software/src/OC_config.h:18`) — a ~60 µs budget shared by both hemispheres and
  the DAC/ADC/display work. No loops over large arrays, no divisions in hot paths where a
  shift works, no allocation. Do heavy work (pattern mutation, regeneration) once per
  clock/pass event, spread across ticks if needed.
- **`View()` is cheap**: draw from state, never compute state in `View()`.
- **Applet glue stays thin**: parameter mapping, I/O, drawing. Logic belongs in `custom/core`.
- **Applet IDs 200+** for our applets, so they never collide with upstream IDs in presets.
- **64-bit state discipline.** Each applet persists exactly one `uint64_t`
  (`OnDataRequest`/`OnDataReceive`, `Pack`/`Unpack` with `PackLocation`). Document the bit
  layout in the module doc; persist settings, not evolving state; validate on unpack
  (constrain every field) so stale/garbage presets cannot crash or misbehave.
- **Flash/RAM budget awareness.** 256 KB flash is tight. Check `task size` before and after
  a change and report the delta; `docs/BUILD.md` records the current numbers.
- RNG: use a seeded per-module xorshift32 (deterministic, host-reproducible); keep separate
  streams where "zero amount must consume no randomness" matters.
- **License header for new files** in `custom/` and `host/` (repo is GPLv3, `LICENSE`):
  ```
  // SPDX-License-Identifier: GPL-3.0-or-later
  // Copyright (C) 2026 Ralf Traunsteiner
  ```
  Never strip upstream headers in `firmware/`. Still missing the header:
  `custom/applets/Hello.h`, `custom/core/hello_core.h`.
- Prefer `constexpr` constants over `#define`; no dynamic allocation; no STL containers in
  firmware code.
