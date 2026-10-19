// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// likembe_dump -- prints the Likembe core's event stream for parity checks against
// the browser lab engine (docs/demo/likembe_engine.js). Line format:
//   bar pulse thumb degree rest accent pitch_units
// Usage: likembe_dump --seed S --part P --dens D --root R --oct O --grid 16|12
//                     --var V --pass IDX --bars N   (missing options = part defaults)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "custom/core/likembe_core.h"

int main(int argc, char** argv) {
  int part = 0;
  for (int i = 1; i + 1 < argc; i += 2) if (!strcmp(argv[i], "--part")) part = atoi(argv[i + 1]);
  custom::LikembeParams p = custom::LikembeParams::Defaults((uint8_t)part);
  int bars = 16;
  for (int i = 1; i + 1 < argc; i += 2) {
    const char* k = argv[i]; int v = atoi(argv[i + 1]);
    if (!strcmp(k, "--seed")) p.seed = (uint16_t)v;
    else if (!strcmp(k, "--part")) p.part = (uint8_t)v;
    else if (!strcmp(k, "--dens")) p.density_pct = (uint8_t)v;
    else if (!strcmp(k, "--root")) p.root = (uint8_t)v;
    else if (!strcmp(k, "--oct")) p.octave = (int8_t)v;
    else if (!strcmp(k, "--grid")) p.grid16 = v == 16 ? 1 : 0;
    else if (!strcmp(k, "--var")) p.variation_pct = (uint8_t)v;
    else if (!strcmp(k, "--pass")) p.passage_idx = (uint8_t)v;
    else if (!strcmp(k, "--bars")) bars = v;
    else { fprintf(stderr, "unknown option %s\n", k); return 2; }
  }
  custom::LikembeCore core;
  core.Init(p);
  for (int b = 0; b < bars; ++b) {
    const int B = core.pulses_per_bar;
    for (int i = 0; i < B; ++i) {
      const int idx = core.pulse_idx;
      custom::LikembeEvent e = core.Pulse();
      printf("%d %d %d %d %d %d %d\n", b, idx, e.thumb, e.degree, e.note ? 0 : 1, e.accent ? 1 : 0, e.pitch_units);
    }
  }
  return 0;
}
