# Ornament & Crime custom firmware

A deliberately minimal custom firmware for the Ornament & Crime eurorack module
(**Teensy 3.2** hardware), based on the [Phazerville Suite](https://github.com/djphazer/O_C-Phazerville)
(branch `T32`, vendored as a git subtree in `firmware/`).

Instead of shipping ~95 applets, this build carries only the Hemisphere shell and a small
set of self-written applets. Upstream applets are added one at a time, only when wanted.
The master clock is meant to become a first-class citizen: one global tempo with
polymetric clock buses any applet can follow.

## Hardware target

Ornament & Crime with Teensy 3.2: 128x64 OLED (two 64x64 halves), 4 CV in, 4 trigger in,
4 CV out, 2 push encoders, 2 buttons. Not for Teensy 4.x / ORN8 hardware.

## Status

Scaffolding. No custom applet is functional yet. See `docs/PROJECT.md` § Current Status.

## Documentation

- `docs/PROJECT.md` — goals, status, next steps, index of every doc.
- `docs/BUILD.md` — building and flashing (`task build`, `task flash`).
- `docs/ROADMAP.md` — phased plan.

Each module gets its own doc in `docs/modules/` with a full controls table (encoders,
buttons, inputs, outputs) once it is implemented.

## License

Upstream files are mostly MIT; some core files are GPLv3, so distributed binaries are
treated as GPLv3. Details in `docs/UPSTREAM.md`.
