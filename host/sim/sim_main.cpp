// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Tier 3: scripted host harness for Hemisphere applets (shares sim_core).
//
// Arguments form a script executed left to right, e.g.
//   sim --press --encoder +3 --in0 1536 --seconds 2
// The frame is rendered (PBM/PGM/ASCII) and outputs printed at the end.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "sim_core.h"
#include "oled_dump.h"

static void Usage() {
  std::puts(
      "usage: sim [--side L|R|LR] [script...]\n"
      "  --side L|R|LR   hemisphere(s) running Hello (default L; LR = both,\n"
      "                  encoder/press then address the left one)\n"
      "  --seconds S     run Controller() at OC_CORE_ISR_FREQ for S seconds\n"
      "  --ticks N       run N ticks\n"
      "  --inK V         set CV input K (0..3) in pitch units (1536 = 1 V)\n"
      "  --gateK 0|1     set digital input K (0..3)\n"
      "  --encoder N     N encoder detents (+/-)\n"
      "  --press         encoder button press (toggles edit mode)\n"
      "  --out DIR       output dir for sim.pbm / sim_x4.pgm (default .)\n"
      "  --quiet         no ASCII frame dump");
}

static uint32_t ScriptTicks(double seconds) { return (uint32_t)(seconds * simcore::kTickHz + 0.5); }

int main(int argc, char **argv) {
  std::string outdir = ".";
  bool quiet = false;

  // --side is a boot option: resolve it before the script runs.
  std::string sides = "L";
  for (int i = 1; i + 1 < argc; ++i)
    if (std::strcmp(argv[i], "--side") == 0) sides = argv[i + 1];
  const bool left = sides.find('L') != std::string::npos;
  const bool right = sides.find('R') != std::string::npos;
  simcore::Init(left || !right, right);
  const simcore::Side side = (left || !right) ? simcore::kLeft : simcore::kRight;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() -> const char * {
      if (i + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", a.c_str()); std::exit(2); }
      return argv[++i];
    };
    if (a == "--seconds") simcore::Run(ScriptTicks(std::atof(next())));
    else if (a == "--ticks") simcore::Run((uint32_t)std::atol(next()));
    else if (a.rfind("--in", 0) == 0 && a.size() == 5) simcore::SetCv((a[4] - '0') & 3, std::atoi(next()));
    else if (a.rfind("--gate", 0) == 0 && a.size() == 7) simcore::SetGate((a[6] - '0') & 3, std::atoi(next()) != 0);
    else if (a == "--encoder") simcore::EncoderMove(side, std::atoi(next()));
    else if (a == "--press") simcore::EncoderPress(side);
    else if (a == "--out") outdir = next();
    else if (a == "--quiet") quiet = true;
    else if (a == "--side") next();
    else { Usage(); return a == "--help" || a == "-h" ? 0 : 2; }
  }

  const uint8_t *fb = simcore::Render();
  std::printf("t=%.3fs ticks=%u edit=%d\n", simcore::Seconds(), (unsigned)simcore::Ticks(),
              (int)simcore::EditMode(side));
  for (int ch = 0; ch < simcore::kChannels; ++ch) std::printf("out%d=%d ", ch, (int)simcore::Out(ch));
  std::printf("\n");
  std::string p = outdir + "/sim.pbm";
  host::WritePbm(fb, p.c_str());
  host::WritePgm(fb, (outdir + "/sim_x4.pgm").c_str());
  std::printf("wrote %s\n", p.c_str());
  if (!quiet) host::PrintAscii(fb);
  return 0;
}
