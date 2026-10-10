# NitroTron3 — Lessons Carried Over

The user's previous project (`/Users/ralf/src/tronstoner/daisyseed/NitroTron3/`, Daisy Seed
bass pedal) established conventions this project reuses. Read-only reference.

## Carried over

- **Taskfile CLI** (`NitroTron3/Taskfile.yml`): one discoverable entry point (`task build`,
  `task flash`, `task catch` retry-flash loop, `task clean`). Here: PlatformIO behind
  go-task; see `docs/BUILD.md`.
- **Build-profile stamp**: the build records which profile/env it was made with and refuses
  to mix outputs (NitroTron3 wiped `build/` on `INSTRUMENT` change). Here: env name
  (`custom_min` / `custom_min_vor`) stamped and shown, so a VOR binary is never flashed by accident.
- **Stub-header host tests** (`NitroTron3/tools/host/stub/`, `run.sh`, `test_*.cpp`): stub
  headers shadow the HAL on the include path; pure code compiles with plain `g++`. Here:
  `host/` with stubs for Arduino/Teensy/O_C headers. See `docs/HOST_SIM.md`.
- **Core / shell split** (`src/core/` vs `pedals/<pedal>/`): here `custom/core/` vs `custom/applets/`.
- **Docs conventions**: `docs/PROJECT.md` with Document Map, Current Status, Next; concept →
  impl plan → as-built per module; UPPER_SNAKE at docs root, kebab-case in subfolders;
  tuning values only in `*_constants.h`; docs updated every session.
- **Agent rules** (`NitroTron3/agents-instructions.md`): ask with 2–3 options before fixing;
  only main agent commits; verification level in commit messages; never push; measure
  before theorising; "Firmware translation" section in plans.
- **Serial diagnostics** (`NitroTron3/tools/diag/`, `serial-diag` skill): timestamped USB-serial
  heartbeat/fault capture. Teensy offers USB serial too (upstream already prints in
  `Main.cpp`); adopt when the first intermittent fault appears.
- **Rhythm engine**: the vestige error/line model is the basis of
  `docs/modules/evolving-voltage-concept.md`.

## Not carried over

- DFU flashing, Daisy bootloader, QSPI/SDRAM placement (Teensy uses the Teensy Loader /
  PlatformIO upload).
- libDaisy / DaisySP, audio-rate DSP, float-heavy code (Daisy has an FPU; Teensy 3.2 does not).
- Footswitch/knob/LED controls template (O_C has encoders, buttons, OLED; module docs get
  their own controls table).
- Continuous savepoint commits without asking: here commits happen on the user's request.
