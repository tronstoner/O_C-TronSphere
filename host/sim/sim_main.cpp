// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Tier 3: scripted host harness for Hemisphere applets (shares sim_core).
//
// Arguments form a script executed left to right, e.g.
//   sim --press --encoder +3 --in0 1536 --seconds 2
// The frame is rendered (PBM/PGM/ASCII) and outputs printed at the end.
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "sim_core.h"
#include "oled_dump.h"
#include "png.h"

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
      "  --quiet         no ASCII frame dump\n"
      "internal clock (real HS::clock_m + ClockSetup::Controller):\n"
      "  --int-start     start now (clock_m.Start(); applets get Reset() via auto_reset)\n"
      "  --int-arm       arm (paused): starts on the next TR1 pulse\n"
      "  --int-stop      stop\n"
      "  --int-bpm N     tempo in BPM\n"
      "  --mult S:M      multiplier of input slot S (1..4 = left in1, in2, right in1, in2):\n"
      "                  x1..x24, /2../32, off\n"
      "  --ppqn N        external sync: TR1 pulses per beat (0..24)\n"
      "panel keys (same letters as the SDL window, docs/HOST_SIM.md):\n"
      "  --keys LIST     comma list, tap or down:/up: prefix: q/e Up/Down button\n"
      "                  (down:q,down:e,up:q,up:e = Up+Down -> ClockSetup screen,\n"
      "                  a later q or e tap closes it), g toggle ClockSetup screen,\n"
      "                  a/d left enc -/+, j/l right enc -/+, s/k left/right push\n"
      "  --png FILE      write the current 128x64 frame as PNG (relative to --out)\n"
      "checks (exit 1 on failure):\n"
      "  --segment              start a new segment of recorded output rises\n"
      "  --expect-rises K N     output K (0..3) rose from 0 N times (N or LO..HI) in this segment\n"
      "  --expect-bpm N         internal clock tempo (N or LO..HI)\n"
      "  --expect-state TEXT   the state line (as printed at the end) contains TEXT\n"
      "  --expect-same K A B N  first N rises of output K in segments A and B (0-based)\n"
      "                         show the same values on all four outputs");
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
// Clock multiplier like ClockSetup shows it: x4, /2, off.
std::string Mult(int m) {
  if (m == 0) return "off";
  return m > 0 ? "x" + std::to_string(m) : "/" + std::to_string(1 - m);
}
}  // namespace fmt

// One output rise (0 -> nonzero) and all four outputs at that tick.
struct Rise { int ch; int32_t out[simcore::kChannels]; };

struct Logger : simcore::OutListener {
  FILE *csv = nullptr;
  bool events = false;
  std::vector<std::vector<Rise>> segments{1};
  int32_t last[simcore::kChannels] = {};
  void Row(double t, int ch, int32_t v) {
    std::fprintf(csv, "%s,%d,%d,%s\n", fmt::Seconds(t).c_str(), ch, (int)v, fmt::Volts(v).c_str());
  }
  void OnChange(double t, int ch, int32_t v) override {
    if (csv) Row(t, ch, v);
    if (last[ch] == 0 && v != 0) segments.back().push_back({ch, {}});
    last[ch] = v;
  }
  // Snapshot outputs after the tick (OnChange runs per channel inside the tick).
  void Settle() {
    for (auto &r : segments.back())
      if (r.out[0] == 0 && r.out[1] == 0 && r.out[2] == 0 && r.out[3] == 0)
        for (int k = 0; k < simcore::kChannels; ++k) r.out[k] = simcore::Out(k);
  }
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

// "x4" -> 4, "/2" -> -1 (upstream: -1 = /2), "off" -> 0, else the raw number.
static int ParseMult(const char *s) {
  if (s[0] == 'x') return std::atoi(s + 1);
  if (s[0] == '/') return 1 - std::atoi(s + 1);
  if (std::strcmp(s, "off") == 0) return 0;
  return std::atoi(s);
}

// "N" or "LO..HI" (inclusive).
struct Range { long lo, hi; bool Has(long v) const { return v >= lo && v <= hi; } };
static Range ParseRange(const char *s) {
  const char *d = std::strstr(s, "..");
  return d ? Range{std::atol(s), std::atol(d + 2)} : Range{std::atol(s), std::atol(s)};
}
static std::string Str(const Range &r) {
  return r.lo == r.hi ? std::to_string(r.lo) : std::to_string(r.lo) + ".." + std::to_string(r.hi);
}

// --keys: the SDL window's panel keys, applied to simcore the same way ui_main.cpp does.
static void Keys(const std::string &list) {
  size_t pos = 0;
  while (pos < list.size()) {
    size_t e = list.find(',', pos);
    if (e == std::string::npos) e = list.size();
    std::string t = list.substr(pos, e - pos);
    pos = e + 1;
    if (t.empty()) continue;
    int phase = 0;  // 0 tap, 1 down, 2 up
    if (t.rfind("down:", 0) == 0) { phase = 1; t = t.substr(5); }
    else if (t.rfind("up:", 0) == 0) { phase = 2; t = t.substr(3); }
    if (t.size() != 1) { std::fprintf(stderr, "--keys: unknown key '%s'\n", t.c_str()); std::exit(2); }
    const char k = (char)std::tolower((unsigned char)t[0]);
    const bool dn = phase != 2, up = phase != 1;
    switch (k) {
      case 'q': case 'e': {
        const simcore::Side s = k == 'q' ? simcore::kLeft : simcore::kRight;
        if (dn) simcore::ButtonDown(s);
        if (up) simcore::ButtonUp(s);
        break;
      }
      case 's': if (up) simcore::EncoderPress(simcore::kLeft); break;   // acts on release
      case 'k': if (up) simcore::EncoderPress(simcore::kRight); break;
      case 'a': if (dn) simcore::EncoderMove(simcore::kLeft, -1); break;
      case 'd': if (dn) simcore::EncoderMove(simcore::kLeft, +1); break;
      case 'j': if (dn) simcore::EncoderMove(simcore::kRight, -1); break;
      case 'l': if (dn) simcore::EncoderMove(simcore::kRight, +1); break;
      case 'g': if (dn) simcore::ClockSetupShow(!simcore::ClockSetupOpen()); break;
      default: std::fprintf(stderr, "--keys: unsupported key '%c'\n", k); std::exit(2);
    }
  }
}

static void Png(const std::string &path) {
  const uint8_t *fb = simcore::Render();
  std::vector<uint8_t> g(128 * 64);
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 128; ++x) g[y * 128 + x] = host::Pixel(fb, x, y) ? 255 : 0;
  const bool ok = host::WritePng(path.c_str(), 128, 64, 1, g.data());
  std::printf("%s %s\n", ok ? "wrote" : "FAILED to write", path.c_str());
}

// The final state line; --expect-state matches against the same text.
static std::string StateLine(simcore::Side side) {
  char b[256];
  std::snprintf(b, sizeof b, "t=%.3fs ticks=%u edit=%d int=%s bpm=%d ppqn=%d mult=%s,%s,%s,%s", simcore::Seconds(),
                (unsigned)simcore::Ticks(), (int)simcore::EditMode(side),
                simcore::IntRunning() ? "running" : (simcore::IntArmed() ? "armed" : "stopped"), simcore::IntBpm(),
                simcore::IntPpqn(), fmt::Mult(simcore::IntMultiply(0)).c_str(), fmt::Mult(simcore::IntMultiply(1)).c_str(),
                fmt::Mult(simcore::IntMultiply(2)).c_str(), fmt::Mult(simcore::IntMultiply(3)).c_str());
  return b;
}

static int g_fail = 0;
static void Check(bool ok, const std::string &what) {
  std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
  if (!ok) ++g_fail;
}

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
    auto run = [&](uint32_t n) { for (uint32_t k = 0; k < n; ++k) { simcore::Tick(); log.Settle(); } };
    if (a == "--seconds") run(ScriptTicks(std::atof(next())));
    else if (a == "--ticks") run((uint32_t)std::atol(next()));
    else if (a == "--int-start") simcore::IntStart(false);
    else if (a == "--int-arm") simcore::IntStart(true);
    else if (a == "--int-stop") simcore::IntStop();
    else if (a == "--int-bpm") simcore::IntSetBpm(std::atoi(next()));
    else if (a == "--ppqn") simcore::IntSetPpqn(std::atoi(next()));
    else if (a == "--mult") {
      const char *v = next();
      const char *c = std::strchr(v, ':');
      if (!c) { std::fprintf(stderr, "--mult S:M\n"); return 2; }
      const int slot = ParseTr(v, "--mult");
      simcore::IntSetMultiply(slot, ParseMult(c + 1));
      std::printf("t=%s mult slot %d = %s\n", fmt::Seconds(simcore::Seconds()).c_str(), slot + 1,
                  fmt::Mult(simcore::IntMultiply(slot)).c_str());
    }
    else if (a == "--segment") log.segments.emplace_back();
    else if (a == "--expect-rises") {
      const int k = std::atoi(next()) & 3;
      const Range n = ParseRange(next());
      int got = 0;
      for (const auto &r : log.segments.back()) got += r.ch == k;
      Check(n.Has(got), "segment " + std::to_string(log.segments.size() - 1) + " out" + std::to_string(k) +
                          " rises: want " + Str(n) + " got " + std::to_string(got));
    }
    else if (a == "--expect-bpm") {
      const Range n = ParseRange(next());
      Check(n.Has(simcore::IntBpm()), "internal clock tempo: want " + Str(n) + " got " + std::to_string(simcore::IntBpm()) + " BPM");
    }
    else if (a == "--expect-state") {
      const std::string want = next(), got = StateLine(side);
      Check(got.find(want) != std::string::npos, "state line contains '" + want + "': " + got);
    }
    else if (a == "--expect-same") {
      const int k = std::atoi(next()) & 3, sa = std::atoi(next()), sb = std::atoi(next()), n = std::atoi(next());
      auto pick = [&](int seg) {
        std::vector<Rise> v;
        if (seg >= 0 && seg < (int)log.segments.size())
          for (const auto &r : log.segments[seg]) if (r.ch == k && (int)v.size() < n) v.push_back(r);
        return v;
      };
      const auto va = pick(sa), vb = pick(sb);
      bool ok = (int)va.size() == n && (int)vb.size() == n;
      std::string detail;
      for (int j = 0; ok && j < n; ++j)
        for (int c = 0; c < simcore::kChannels; ++c)
          if (va[j].out[c] != vb[j].out[c]) { ok = false; detail = " (rise " + std::to_string(j) + " out" + std::to_string(c) + ": " + std::to_string(va[j].out[c]) + " vs " + std::to_string(vb[j].out[c]) + ")"; }
      Check(ok, "out" + std::to_string(k) + " first " + std::to_string(n) + " rises equal in segments " +
                    std::to_string(sa) + " and " + std::to_string(sb) + detail +
                    (ok ? "" : " [have " + std::to_string(va.size()) + ", " + std::to_string(vb.size()) + "]"));
    }
    else if (a.rfind("--in", 0) == 0 && a.size() == 5) simcore::SetCv((a[4] - '0') & 3, std::atoi(next()));
    else if (a.rfind("--gate", 0) == 0 && a.size() == 7) simcore::SetGate((a[6] - '0') & 3, std::atoi(next()) != 0);
    else if (a == "--encoder") simcore::EncoderMove(side, std::atoi(next()));
    else if (a == "--press") simcore::EncoderPress(side);
    else if (a == "--keys") Keys(next());
    else if (a == "--png") Png(outdir + "/" + next());
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
  std::printf("%s\n", StateLine(side).c_str());
  for (int ch = 0; ch < simcore::kChannels; ++ch) std::printf("out%d=%d ", ch, (int)simcore::Out(ch));
  std::printf("\n");
  std::string p = outdir + "/sim.pbm";
  host::WritePbm(fb, p.c_str());
  host::WritePgm(fb, (outdir + "/sim_x4.pgm").c_str());
  std::printf("wrote %s\n", p.c_str());
  if (log.csv) { std::fclose(log.csv); std::printf("wrote %s\n", csv_path.c_str()); }
  if (!quiet) host::PrintAscii(fb);
  return g_fail ? 1 : 0;
}
