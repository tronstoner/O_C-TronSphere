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
  std::printf(
      "usage: sim [boot options] [script...]\n"
      "boot options (position-independent):\n"
      "  --left NAME|none   left applet (default Hello); available: %s\n"
      "  --right NAME|none  right applet (default none)\n"
      "  --side L|R|LR      legacy: Hello left / right / both (--left/--right win)\n"
      "                     encoder/press address the left applet if present\n"
      "  --clock BPM[:DIV]  internal clock source, DIV pulses per beat (default 4),\n"
      "                     1 ms pulses from t=0\n"
      "  --clock-tr N       TR input (1..4) the clock drives (default 1)\n"
      "  --trig N@T         one 1 ms trigger on TR N (1..4) at T seconds (repeatable)\n"
      "  --log-outputs FILE CSV of every output change: t_seconds,out_index,value_units,value_volts\n"
      "  --log-events       print a line on every output trigger (ClockOut/GateOut)\n"
      "script (left to right):\n",
      simcore::AppletNames());
  std::puts(
      "  --seconds S     run Controller() at OC_CORE_ISR_FREQ for S seconds\n"
      "  --ticks N       run N ticks\n"
      "  --inK V         set CV input K (0..3) in pitch units (1536 = 1 V)\n"
      "  --gateK 0|1     set digital input K (0..3)\n"
      "  --encoder N     N encoder detents (+/-)\n"
      "  --press         encoder button press (toggles edit mode)\n"
      "  --out DIR       output dir for sim.pbm / sim_x4.pgm (default .)\n"
      "  --quiet         no ASCII frame dump");
}

// Single formatter for everything this front end prints or logs.
namespace fmt {
constexpr int kTimeDecimals = 6, kVoltDecimals = 4;
std::string Num(double v, int dec) {
  char b[64];
  std::snprintf(b, sizeof b, "%.*f", dec, v);
  return b;
}
std::string Seconds(double t) { return Num(t, kTimeDecimals); }
std::string Volts(int32_t units) {
  double v = (double)units / simcore::kUnitsPerVolt;
  if (Num(v, kVoltDecimals).find_first_not_of("-0.") == std::string::npos) v = 0.0;  // no "-0.0000"
  return Num(v, kVoltDecimals);
}
char OutName(int ch) { return (char)('A' + ch); }
}  // namespace fmt

struct Logger : simcore::OutListener {
  FILE *csv = nullptr;
  bool events = false;
  void Row(double t, int ch, int32_t v) {
    std::fprintf(csv, "%s,%d,%d,%s\n", fmt::Seconds(t).c_str(), ch, (int)v, fmt::Volts(v).c_str());
  }
  void OnChange(double t, int ch, int32_t v) override { if (csv) Row(t, ch, v); }
  void OnTrigger(double t, int ch, simcore::TrigKind k) override {
    if (events)
      std::printf("event t=%s out=%c trig=%s\n", fmt::Seconds(t).c_str(), fmt::OutName(ch),
                  k == simcore::kTrigClockOut ? "ClockOut" : "GateOut");
  }
};

static int ParseTr(const char *s, const char *opt) {
  int n = std::atoi(s);
  if (n < 1 || n > 4) { std::fprintf(stderr, "%s: TR must be 1..4\n", opt); std::exit(2); }
  return n - 1;
}

static uint32_t ScriptTicks(double seconds) { return (uint32_t)(seconds * simcore::kTickHz + 0.5); }

int main(int argc, char **argv) {
  std::string outdir = ".";
  bool quiet = false;

  // Boot options: resolve before the script runs.
  std::string sides = "L", left_name, right_name, csv_path;
  bool have_left = false, have_right = false, clock = false;
  double bpm = 120;
  int ppb = 4, clock_tr = 0;
  Logger log;
  for (int i = 1; i + 1 < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--side") sides = argv[i + 1];
    else if (a == "--left") { left_name = argv[i + 1]; have_left = true; }
    else if (a == "--right") { right_name = argv[i + 1]; have_right = true; }
    else if (a == "--clock") {
      clock = true;
      bpm = std::atof(argv[i + 1]);
      if (const char *c = std::strchr(argv[i + 1], ':')) ppb = std::atoi(c + 1);
      if (bpm <= 0 || ppb < 1) { std::fprintf(stderr, "--clock BPM[:DIV], both > 0\n"); return 2; }
    } else if (a == "--clock-tr") clock_tr = ParseTr(argv[i + 1], "--clock-tr");
    else if (a == "--log-outputs") csv_path = argv[i + 1];
  }
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--log-events") == 0) log.events = true;
  if (!have_left) left_name = (sides.find('L') != std::string::npos || sides.find('R') == std::string::npos) ? "Hello" : "none";
  if (!have_right) right_name = sides.find('R') != std::string::npos ? "Hello" : "none";
  auto opt = [](const std::string &n) { return n == "none" ? nullptr : n.c_str(); };
  if (!simcore::Init(opt(left_name), opt(right_name))) return 2;
  const simcore::Side side = simcore::Active(simcore::kLeft) || !simcore::Active(simcore::kRight)
                                 ? simcore::kLeft : simcore::kRight;
  if (!csv_path.empty()) {
    log.csv = std::fopen(csv_path.c_str(), "w");
    if (!log.csv) { std::perror(csv_path.c_str()); return 2; }
    std::fprintf(log.csv, "t_seconds,out_index,value_units,value_volts\n");
    for (int ch = 0; ch < simcore::kChannels; ++ch) log.Row(0.0, ch, simcore::Out(ch));  // initial values
  }
  simcore::SetOutListener(&log);
  if (clock) {
    simcore::SetClock(bpm, ppb, clock_tr);
    simcore::ClockEnable(true);
  }

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
    else if (a == "--side" || a == "--left" || a == "--right" || a == "--clock" ||
             a == "--clock-tr" || a == "--log-outputs") next();
    else if (a == "--log-events") {}
    else if (a == "--trig") {
      const char *v = next();
      const char *at = std::strchr(v, '@');
      if (!at) { std::fprintf(stderr, "--trig N@T\n"); return 2; }
      simcore::ScheduleTrigger(ParseTr(v, "--trig"), std::atof(at + 1));
    }
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
  if (log.csv) { std::fclose(log.csv); std::printf("wrote %s\n", csv_path.c_str()); }
  if (!quiet) host::PrintAscii(fb);
  return 0;
}
