# Design Principles

## 1. Absolute minimum

Stock O_C builds are crammed with modules; that is the problem this project solves. The
build contains the Hemisphere shell and the user's own applets. Everything else is out.

- Upstream applets are **opted out by default** and added **one at a time**, each on the
  user's explicit decision, recorded in `docs/PROJECT.md` § Decisions.
- Agents may *suggest* an upstream applet (with its overlap to existing modules, see
  `docs/modules/README.md`); they never enable one on their own, and never several at once.
- Same inside a module: few, well-understood controls. A parameter that the user cannot
  explain after a week is a parameter too many.

## 2. Master clock first

Tempo is global and reachable in one gesture. Rhythmic modules follow a clock bus rather
than each owning its own divider; polymeters come from the clock, not from per-module menus.
No applet slot is spent on clocking. See `docs/clock/MASTER_CLOCK_CONCEPT.md`.

## 3. One module at a time

1. **Propose** one module (one paragraph: what it does, why, what upstream applet overlaps).
2. **Concept doc** in `docs/modules/<name>-concept.md`: behavior, data model, I/O, open questions.
3. **Decide** — the user answers the open questions, one step at a time.
4. **Impl plan** `docs/modules/<name>-impl-plan.md`: core API, bit layout of the 64-bit
   state, constants, test list, "Firmware translation" section.
5. **Host test** — core in `custom/core/`, unit tests and (later) simulator in `host/`.
6. **Hardware** — glue in `custom/applets/`, flash, listen/patch, tune constants.
7. **Document as-built** — module doc with full controls table; status updated.

No step is skipped and the next module does not start before the current one is done or
explicitly parked.

## 4. UI on a 64x64 hemisphere

- **One main knob / CV per module.** The main parameter is the encoder's default target and
  has a CV input. Secondary parameters sit behind the cursor and are few (aim: ≤ 4).
- **Readable at a glance**: the main value is large or graphical; state (pattern, phase,
  level) is visible without entering a menu. Text is a fallback, not the interface.
- **Consistent semantics across modules**: CV in 1 is the main parameter, trigger in 1 the
  clock/advance, Out A the primary signal. Deviations are documented in the controls table.
- **Formatting through shared helpers** (one helper for BPM, ratios, voltages, percent);
  no ad-hoc number printing per applet.
- **A setting change is audible immediately.** No waiting for the next bar/step unless the
  module doc states otherwise (decided 2026-10-19).
- **No hidden modes.** Long-press or combo gestures only where the shell already uses them.
- `View()` only draws state; it never mutates it.

## 5. Naming

- Applet display names ≤ 9 characters (`applet_name()`, `HemisphereApplet.h:87`).
- Files: core `custom/core/<module>.h` (+ `<module>_constants.h`), applet
  `custom/applets/<Module>.h`, class name = applet file name.
- Docs: `docs/modules/<module>-concept.md`, `-impl-plan.md`, as-built `<module>.md`.
- Applet IDs: 200+, allocated in order, recorded in the module doc. Never reuse an ID.
