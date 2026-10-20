# Upstream (Phazerville T32) vendoring

`firmware/` is a **git subtree** (squashed) of https://github.com/djphazer/O_C-Phazerville, branch **T32**, the Teensy-3.2 maintenance line. Imported at upstream commit `9c87050` (2026-10-08).

## Commands

The remote is already configured:

```sh
git remote add upstream https://github.com/djphazer/O_C-Phazerville.git   # only if missing
```

See what's new (read-only):

```sh
task upstream-fetch
# equivalent:
git fetch upstream T32
git log --oneline <last-split>..upstream/T32   # last split = git-subtree-split: in the last squash commit
```

Merge upstream (main agent or user only, on a clean tree, on the feature branch):

```sh
git subtree pull --prefix firmware upstream T32 --squash
task clean && task build && task build ENV=T32   # rebuild ours and the reference
```

Resolve conflicts using the file list below. Afterwards, update the measured sizes in `docs/BUILD.md`.

## Rules for editing upstream files

- Edit files inside `firmware/` freely, but **keep edits minimal** and wrap every one in
  ```
  // --- custom build ---
  ...
  // --- end custom build ---
  ```
  (`; --- custom build ---` in `.ini`). Prefer adding hooks (an `#include` of a file under `custom/`) over changing upstream logic.
- Put new code in `custom/`, not `firmware/`.
- Never modify upstream envs (`T3`, `T32`, ...) in `platformio.ini`. Add new envs instead.
- **Update the list below** in the same change whenever you touch an upstream file. This list is the merge-conflict map.

## Modified upstream files

| File | Change | Why |
|---|---|---|
| `firmware/software/platformio.ini` | Added `[env:custom_min]` and `[env:custom_min_vor]` in a `; --- custom build ---` block before `[env:antihem]` | Our build envs (`-DCUSTOM_BUILD -I../..`) |
| `firmware/software/src/hemisphere_config.h` | (1) After the applet `#include`s: under `CUSTOM_BUILD`, include `custom/hemisphere_config_custom.h` with `CUSTOM_APPLET_INCLUDES` defined (our headers). (2) At the start of `AppletRegistry reg{`: `#ifdef CUSTOM_BUILD` includes our registry lines, `#else` the upstream list (Ponglet + PEWPEWPEW/APPLETS_*), closed by `#endif // --- custom build ---` after `#endif // !PEWPEWPEW` | Registry contains only our applets. Ponglet dropped |
| `firmware/software/src/applets/ClockSetup.h` | (1) After `#pragma once`: under `CUSTOM_BUILD`, include `custom/core/clock_mult_list.h`. (2) `OnEncoderMove` case `MULT1..4` (right encoder) and (3) `OnLeftEncoderMove` multiplier path (left encoder doubling/halving): under `#ifdef CUSTOM_BUILD` the new value is `custom::clock_mult::Step(mult, direction)`; upstream code kept in `#else`. Display, persistence, other cursors unchanged. Host sim: `host/sim/host_clocksetup.cpp` defines `CUSTOM_BUILD` for this unit | One multiplier list (off /32../2 x1..x8 x12 x16 x24, widened 2026-10-20) in ClockSetup and in our applets (`docs/CLOCK.md`, decided 2026-10-20) |

## Conventions

- Our applet classes: `custom/applets/<Name>.h`, class `<Name>`, name ≤ 9 chars (`applet_name()`).
- Pure logic: `custom/core/<name>_core.h`, `namespace custom`, struct `<Name>Core`, no Arduino/O_C includes.
- IDs: ours start at **200** and increase. Upstream applets keep upstream ids. Never reuse or renumber ids, because presets persist them.

## Licensing

Most upstream file headers are MIT. Four core files are **GPLv3**: `OC_DAC.cpp`, `SH1106_128x64_driver.cpp`, `grids_resources.h`, `grids2_resources.h`. Because they're linked into every build, **treat distributed binaries as GPLv3** (ship source with any released hex).

### Decision (2026-10-10): GPLv3 for the whole repo

- **Our code** (`custom/`, `host/`, `tools/`, docs) is licensed GPL-3.0-or-later; full text in
  `/LICENSE`. New files carry an SPDX header (see `AGENTS.md` § O_C coding rules).
- **The combined work** (any built firmware) is GPLv3 anyway, because the four GPLv3 upstream
  files above are linked into every build.
- **Upstream MIT headers stay intact.** Never strip or rewrite copyright/license headers in
  `firmware/`; MIT code may be distributed under GPLv3 as part of the combined work, but its
  notices must be kept.

### Publishing on GitHub

- Anyone who receives a hex/binary must be able to get the corresponding source. The public
  repo satisfies this as long as every released hex is built from a tagged, pushed commit
  (name the commit/tag in the release notes).
- Keep upstream attribution: the `firmware/` subtree with its history, upstream file headers,
  and the lineage (mxmxmx → Hemisphere Suite → Benisphere → Phazerville) in `README.md`/docs.
