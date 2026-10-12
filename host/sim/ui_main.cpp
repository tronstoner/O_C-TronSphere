// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Tier 4: interactive SDL2 front end for the host simulator.
//
// Same tick loop and render as the scripted harness (sim_core). Simulated time
// advances in batches per video frame, paced by the wall clock. The panel under
// the OLED is drawn with the firmware's own 6x8 font (src/extern/gfx_font_6x8.h).
// See docs/HOST_SIM.md "Interactive simulator".
#include <SDL.h>

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

#include "oled_dump.h"
#include "png.h"
#include "sim_core.h"

#ifndef PROGMEM
#define PROGMEM
#endif
#include "gfx_font_6x8.h"  // ssd1306xled_font6x8: ' '..'z', 6 column bytes, LSB = top

namespace {

using simcore::Side;
using simcore::kLeft;
using simcore::kRight;

// ---------------------------------------------------------------------------
// Display formatting: the single source of truth for every number on screen.
namespace fmt {
constexpr int kVoltDecimals = 2;  // volts = units / kUnitsPerVolt

std::string Printf(const char *f, ...) {
  char buf[128];
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(buf, sizeof buf, f, ap);
  va_end(ap);
  return buf;
}
// "+1.00 V": sign always shown, never "-0.00".
std::string Volts(int32_t units) {
  double v = (double)units / simcore::kUnitsPerVolt;
  const double q = std::pow(10.0, kVoltDecimals);
  if (std::round(v * q) == 0) v = 0.0;
  return Printf("%+.*f V", kVoltDecimals, v);
}
std::string Raw(int32_t units) { return Printf("%+6d", (int)units); }
std::string Seconds(double s) { return Printf("%.3f s", s); }
std::string Ratio(double r) { return Printf("%.2fx", r); }
std::string Bpm(double b) { return Printf("%g BPM", b); }
}  // namespace fmt

// ---------------------------------------------------------------------------
constexpr int kOledW = 128, kOledH = 64;
constexpr int kCvMin = -10 * simcore::kUnitsPerVolt, kCvMax = 10 * simcore::kUnitsPerVolt;
constexpr int kSemitone = 128, kOctave = simcore::kUnitsPerVolt;
constexpr int kStepTicks = 17;                       // '.' = 1 ms
constexpr int kBpmStep = 5, kBpmMin = 5, kBpmMax = 600;
constexpr uint32_t kTrigFlashTicks = simcore::kTickHz / 20;  // output trigger marker: 50 ms
constexpr uint32_t kMaxTicksPerFrame = simcore::kTickHz / 10;  // drop backlog beyond 100 ms

constexpr uint32_t kOn = 0xE8F2FF, kOnGap = 0x6C7682, kOff = 0x08080A;
constexpr uint32_t kPanelBg = 0x1A1C20, kText = 0xD0D4DA, kDim = 0x7C828C,
                   kAccent = 0xFFC640, kSelBg = 0x3A4A66, kWarn = 0xFF6A5A, kHigh = 0x6AE07A;

struct Options {
  int scale = 6;
  std::string left = "Hello", right = "Hello";  // "none" = empty
  bool clock = false;                            // --clock given: start running
  double bpm = 120;
  int ppb = 4, clock_tr = 0;
  std::vector<std::pair<int, double>> trigs;     // --trig N@T
  bool headless = false;
  double shot_after = -1;
  std::string out = ".";
  std::string keys;
};

struct Ui {
  Options opt;
  int W = 0, H = 0, oled_h = 0, p = 1;  // window size, panel font scale
  std::vector<uint32_t> px;              // 0xRRGGBB framebuffer for the whole window

  int sel = 0;                           // selected CV channel
  bool gate_held[4] = {}, gate_latch[4] = {};
  bool paused = false;
  bool up_held = false, down_held = false, dual = false;
  double ratio = 1.0;
  bool quit = false;
  std::string msg = "Ready. Keys are listed below.";
};

void Msg(Ui &u, const std::string &m) {
  u.msg = m;
  std::printf("[%s] %s\n", fmt::Seconds(simcore::Seconds()).c_str(), m.c_str());
}

void ApplyGate(Ui &u, int ch) { simcore::SetGate(ch, u.gate_held[ch] || u.gate_latch[ch]); }

const char *SideName(Side s) { return s == kLeft ? "left" : "right"; }

void Encoder(Ui &u, Side s, int dir) {
  if (!simcore::Active(s)) return Msg(u, std::string(SideName(s)) + " hemisphere is empty");
  simcore::EncoderMove(s, dir);
}

void Push(Ui &u, Side s) {
  if (!simcore::Active(s)) return Msg(u, std::string(SideName(s)) + " hemisphere is empty");
  simcore::EncoderPress(s);
}

// Up = button A (left), Down = button B (right). Firmware acts on release.
void SelectRelease(Ui &u, Side s) {
  if (simcore::SelectButton(s))
    Msg(u, std::string(s == kLeft ? "Up" : "Down") + ": AuxButton (edit cancelled)");
  else
    Msg(u, std::string(s == kLeft ? "Up" : "Down") +
               ": select mode needs HemisphereManager (not simulated)");
}

void Screenshot(Ui &u);

void OnKey(Ui &u, SDL_Keycode k, bool shift, bool down, bool repeat) {
  // Gates: momentary while held, Shift+key toggles latch.
  const SDL_Keycode gate_keys[4] = {SDLK_z, SDLK_x, SDLK_c, SDLK_v};
  for (int ch = 0; ch < 4; ++ch) {
    if (k != gate_keys[ch]) continue;
    if (repeat) return;
    if (down && shift) {
      u.gate_latch[ch] = !u.gate_latch[ch];
      Msg(u, fmt::Printf("TR%d latch %s", ch + 1, u.gate_latch[ch] ? "on" : "off"));
    } else if (!shift || !down) {
      u.gate_held[ch] = down;
    }
    return ApplyGate(u, ch);
  }

  if (k == SDLK_q || k == SDLK_e) {
    if (repeat) return;
    bool &me = (k == SDLK_q) ? u.up_held : u.down_held;
    bool &other = (k == SDLK_q) ? u.down_held : u.up_held;
    if (down) {
      me = true;
      if (other) {
        u.dual = true;
        Msg(u, "Up+Down = Clock Setup: not simulated (ClockSetup is not ticked)");
      }
    } else {
      me = false;
      if (u.dual) { if (!other) u.dual = false; }  // swallow both releases
      else SelectRelease(u, k == SDLK_q ? kLeft : kRight);
    }
    return;
  }

  if (!down) {  // encoder pushes act on release, like the firmware
    if (k == SDLK_s) Push(u, kLeft);
    if (k == SDLK_k) Push(u, kRight);
    return;
  }

  switch (k) {
    case SDLK_a: Encoder(u, kLeft, -1); break;
    case SDLK_d: Encoder(u, kLeft, +1); break;
    case SDLK_j: Encoder(u, kRight, -1); break;
    case SDLK_l: Encoder(u, kRight, +1); break;
    case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4:
      u.sel = (int)(k - SDLK_1);
      break;
    case SDLK_0:
      simcore::SetCv(u.sel, 0);
      break;
    case SDLK_UP: case SDLK_DOWN: {
      int step = (shift ? kOctave : kSemitone) * (k == SDLK_UP ? 1 : -1);
      int v = simcore::Cv(u.sel) + step;
      simcore::SetCv(u.sel, v < kCvMin ? kCvMin : (v > kCvMax ? kCvMax : v));
      break;
    }
    case SDLK_SPACE:
      if (repeat) break;
      u.paused = !u.paused;
      Msg(u, u.paused ? "paused" : "running");
      break;
    case SDLK_PERIOD:
      simcore::Run(kStepTicks);
      Msg(u, "step 1 ms (17 ticks)");
      break;
    case SDLK_p:
      if (!repeat) Screenshot(u);
      break;
    case SDLK_r:
      if (repeat) break;
      simcore::ResetApplet(kLeft);
      simcore::ResetApplet(kRight);
      Msg(u, "Reset()+Start() on all applets");
      break;
    case SDLK_t:
      if (repeat) break;
      simcore::ClockEnable(!simcore::ClockEnabled());
      Msg(u, fmt::Printf("clock %s", simcore::ClockEnabled() ? "running" : "stopped"));
      break;
    case SDLK_LEFTBRACKET: case SDLK_RIGHTBRACKET: {
      double b = simcore::ClockBpm() + (k == SDLK_RIGHTBRACKET ? kBpmStep : -kBpmStep);
      simcore::SetClockBpm(b < kBpmMin ? kBpmMin : (b > kBpmMax ? kBpmMax : b));
      Msg(u, "clock " + fmt::Bpm(simcore::ClockBpm()));
      break;
    }
    case SDLK_ESCAPE: u.quit = true; break;
    default: break;
  }
}

// ---------------------------------------------------------------------------
// Drawing into u.px
void Fill(Ui &u, int x, int y, int w, int h, uint32_t c) {
  for (int yy = y < 0 ? 0 : y; yy < y + h && yy < u.H; ++yy)
    for (int xx = x < 0 ? 0 : x; xx < x + w && xx < u.W; ++xx) u.px[(size_t)yy * u.W + xx] = c;
}

int CharW(const Ui &u) { return 6 * u.p; }
int LineH(const Ui &u) { return 10 * u.p; }

// Draw text at character column/row of the panel. Returns next column.
int Text(Ui &u, int col, int row, uint32_t color, std::string s, uint32_t bg = 0xFFFFFFFF) {
  const int max_cols = u.W / CharW(u) - 1 - col;
  if ((int)s.size() > max_cols) s = s.substr(0, max_cols < 0 ? 0 : max_cols);
  const int x0 = CharW(u) * col + 2 * u.p, y0 = u.oled_h + 4 * u.p + LineH(u) * row;
  if (bg != 0xFFFFFFFF) Fill(u, x0 - u.p, y0 - u.p, CharW(u) * (int)s.size() + u.p, 10 * u.p, bg);
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = (unsigned char)s[i];
    if (c < 32 || c > 'z') c = '?';
    const uint8_t *g = ssd1306xled_font6x8 + 6 * (c - 32);
    for (int cx = 0; cx < 6; ++cx)
      for (int cy = 0; cy < 8; ++cy)
        if ((g[cx] >> cy) & 1) Fill(u, x0 + (int)(i * 6 + cx) * u.p, y0 + cy * u.p, u.p, u.p, color);
  }
  return col + (int)s.size();
}

void DrawOled(Ui &u, const uint8_t *fb) {
  const int s = u.opt.scale;
  for (int y = 0; y < kOledH; ++y)
    for (int x = 0; x < kOledW; ++x) {
      const bool on = host::Pixel(fb, x, y);
      for (int yy = 0; yy < s; ++yy)
        for (int xx = 0; xx < s; ++xx) {
          const bool gap = s > 2 && (xx == s - 1 || yy == s - 1);
          u.px[(size_t)(y * s + yy) * u.W + x * s + xx] = on ? (gap ? kOnGap : kOn) : kOff;
        }
    }
}

void DrawPanel(Ui &u) {
  Fill(u, 0, u.oled_h, u.W, u.H - u.oled_h, kPanelBg);
  Fill(u, 0, u.oled_h, u.W, u.p, kDim);
  int r = 0;
  Text(u, 1, r, kDim, "INPUTS  (1-4 select CV)");
  Text(u, 38, r, kDim, "OUTPUTS");
  ++r;
  for (int ch = 0; ch < 4; ++ch, ++r) {
    const bool sel = ch == u.sel;
    const int32_t cv = simcore::Cv(ch);
    std::string cvs = fmt::Printf("%cCV%d %9s %s", sel ? '>' : ' ', ch + 1, fmt::Volts(cv).c_str(),
                                  fmt::Raw(cv).c_str());
    Text(u, 0, r, sel ? kAccent : kText, cvs, sel ? kSelBg : 0xFFFFFFFF);
    const bool g = simcore::Gate(ch);
    int c = Text(u, 23, r, kText, fmt::Printf("TR%d ", ch + 1));
    c = Text(u, c, r, g ? kHigh : kDim, g ? "HIGH" : "low ");
    if (u.gate_latch[ch]) Text(u, c + 1, r, kAccent, "L");
    const int32_t o = simcore::Out(ch);
    c = Text(u, 38, r, kText, fmt::Printf("%c %9s %s", 'A' + ch, fmt::Volts(o).c_str(), fmt::Raw(o).c_str()));
    const int64_t lt = simcore::LastOutTrigTick(ch);
    const bool flash = lt >= 0 && (int64_t)simcore::Ticks() - lt < (int64_t)kTrigFlashTicks;
    Text(u, c + 1, r, flash ? kHigh : kDim, flash ? "*" : ".");
  }
  ++r;
  for (int h = 0; h < 2; ++h) {
    const Side s = (Side)h;
    const int col = h == 0 ? 1 : 32;
    int c = Text(u, col, r, kDim, h == 0 ? "L: " : "R: ");
    c = Text(u, c, r, kText, simcore::AppletName(s));
    if (simcore::Active(s)) {
      const bool e = simcore::EditMode(s);
      Text(u, c + 1, r, e ? kAccent : kDim, e ? "edit ON" : "edit off");
    }
  }
  ++r;
  int c = Text(u, 1, r, kText, "t " + fmt::Seconds(simcore::Seconds()));
  c = Text(u, c + 2, r, kDim, fmt::Printf("%u ticks", (unsigned)simcore::Ticks()));
  if (u.opt.headless) Text(u, c + 2, r, kDim, "HEADLESS (unpaced)");
  else if (u.paused) Text(u, c + 2, r, kAccent, "PAUSED");
  else Text(u, c + 2, r, u.ratio < 0.98 ? kWarn : kHigh,
            "RUN " + fmt::Ratio(u.ratio) + (u.ratio < 0.98 ? " BEHIND" : ""));
  ++r;
  c = Text(u, 1, r, kDim, "CLOCK ");
  c = Text(u, c, r, kText, fmt::Bpm(simcore::ClockBpm()) + fmt::Printf(" x%d/beat -> TR%d", simcore::ClockPpb(), simcore::ClockTr() + 1));
  Text(u, c + 2, r, simcore::ClockEnabled() ? kHigh : kDim, simcore::ClockEnabled() ? "RUNNING" : "stopped");
  r += 2;
  const char *legend[] = {
      "A/D  left enc -/+      S  left push      Q  Up button",
      "J/L  right enc -/+     K  right push     E  Down button",
      "1-4  select CV   Up/Dn +-1 semi   Shift+Up/Dn +-1 oct",
      "0    zero CV     Z X C V  gate TR1-4 (hold), Shift latch",
      "Space pause   . step 1ms   P screenshot   R reset   Esc",
      "T    clock on/off   [ ] clock BPM -/+5   * = output trigger",
  };
  for (const char *l : legend) Text(u, 1, r++, kDim, l);
  ++r;
  Text(u, 1, r, kAccent, "> " + u.msg);
}

// ---------------------------------------------------------------------------
bool Exists(const std::string &p) {
  struct stat st;
  return stat(p.c_str(), &st) == 0;
}

void Screenshot(Ui &u) {
  int n = 1;
  while (Exists(fmt::Printf("%s/ui_%03d_oled.png", u.opt.out.c_str(), n))) ++n;
  const std::string base = fmt::Printf("%s/ui_%03d", u.opt.out.c_str(), n);
  const uint8_t *fb = simcore::Render();
  std::vector<uint8_t> g(kOledW * kOledH);
  for (int y = 0; y < kOledH; ++y)
    for (int x = 0; x < kOledW; ++x) g[y * kOledW + x] = host::Pixel(fb, x, y) ? 255 : 0;
  auto rgb = [&](int w, int h) {
    std::vector<uint8_t> v((size_t)w * h * 3);
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        uint32_t c = u.px[(size_t)y * u.W + x];
        uint8_t *d = &v[((size_t)y * w + x) * 3];
        d[0] = (uint8_t)(c >> 16); d[1] = (uint8_t)(c >> 8); d[2] = (uint8_t)c;
      }
    return v;
  };
  bool ok = host::WritePng((base + "_oled.png").c_str(), kOledW, kOledH, 1, g.data());
  ok &= host::WritePbm(fb, (base + "_oled.pbm").c_str());
  ok &= host::WritePng(fmt::Printf("%s_oled_x%d.png", base.c_str(), u.opt.scale).c_str(), u.W,
                       u.oled_h, 3, rgb(u.W, u.oled_h).data());
  ok &= host::WritePng((base + "_window.png").c_str(), u.W, u.H, 3, rgb(u.W, u.H).data());
  Msg(u, ok ? "saved " + base + "_{oled.png,oled.pbm,oled_x" + std::to_string(u.opt.scale) +
                  ".png,window.png}"
            : "screenshot FAILED: " + base);
}

// --keys "s,d,shift+up,down:z": scripted key events (tap, or explicit down:/up:).
void InjectKeys(Ui &u) {
  std::string s = u.opt.keys;
  size_t pos = 0;
  while (pos <= s.size() && !s.empty()) {
    size_t e = s.find(',', pos);
    std::string t = s.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
    pos = e == std::string::npos ? s.size() + 1 : e + 1;
    if (t.empty()) continue;
    int phase = 0;  // 0 tap, 1 down, 2 up
    if (t.rfind("down:", 0) == 0) { phase = 1; t = t.substr(5); }
    else if (t.rfind("up:", 0) == 0) { phase = 2; t = t.substr(3); }
    bool shift = false;
    if (t.rfind("shift+", 0) == 0) { shift = true; t = t.substr(6); }
    SDL_Keycode k;
    if (t == "up") k = SDLK_UP;
    else if (t == "down") k = SDLK_DOWN;
    else if (t == "space") k = SDLK_SPACE;
    else if (t == "esc") k = SDLK_ESCAPE;
    else if (t == "lbracket") k = SDLK_LEFTBRACKET;
    else if (t == "rbracket") k = SDLK_RIGHTBRACKET;
    else if (t.size() == 1) k = (SDL_Keycode)std::tolower((unsigned char)t[0]);
    else { std::fprintf(stderr, "--keys: unknown key '%s'\n", t.c_str()); std::exit(2); }
    if (phase != 2) OnKey(u, k, shift, true, false);
    if (phase != 1) OnKey(u, k, shift, false, false);
  }
}

void Usage() {
  std::puts(
      "usage: ui [--scale N] [--left NAME|none] [--right NAME|none] [--keys LIST]\n"
      "          [--clock BPM[:DIV]] [--clock-tr N] [--trig N@T]\n"
      "          [--screenshot-after S] [--headless] [--out DIR]\n"
      "  --scale N             OLED pixel scale (default 6)\n"
      "  --left NAME|none      left hemisphere applet (default Hello)\n"
      "  --right NAME|none     right hemisphere applet (default Hello)\n"
      "  --clock BPM[:DIV]     start the clock source (DIV pulses/beat, default 4);\n"
      "                        without it the source is stopped at 120:4 (T starts it)\n"
      "  --clock-tr N          TR input 1..4 driven by the clock (default 1)\n"
      "  --trig N@T            one 1 ms trigger on TR N at T s (repeatable)\n"
      "  --keys LIST           key events at start, e.g. s,d,d,3,shift+up,down:z\n"
      "  --screenshot-after S  save a screenshot at simulated time S and quit\n"
      "  --headless            no window; simulated time runs unpaced (needs\n"
      "                        --screenshot-after)");
}

}  // namespace

int main(int argc, char **argv) {
  Ui u;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() -> const char * {
      if (i + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", a.c_str()); std::exit(2); }
      return argv[++i];
    };
    if (a == "--scale") u.opt.scale = std::atoi(next());
    else if (a == "--left") u.opt.left = next();
    else if (a == "--right") u.opt.right = next();
    else if (a == "--clock") {
      const char *v = next();
      u.opt.clock = true;
      u.opt.bpm = std::atof(v);
      if (const char *c = std::strchr(v, ':')) u.opt.ppb = std::atoi(c + 1);
      if (u.opt.bpm <= 0 || u.opt.ppb < 1) { std::fprintf(stderr, "--clock BPM[:DIV], both > 0\n"); return 2; }
    } else if (a == "--clock-tr" || a == "--trig") {
      const char *v = next();
      const int n = std::atoi(v);
      if (n < 1 || n > 4) { std::fprintf(stderr, "%s: TR must be 1..4\n", a.c_str()); return 2; }
      if (a == "--clock-tr") u.opt.clock_tr = n - 1;
      else {
        const char *at = std::strchr(v, '@');
        if (!at) { std::fprintf(stderr, "--trig N@T\n"); return 2; }
        u.opt.trigs.push_back({n - 1, std::atof(at + 1)});
      }
    }
    else if (a == "--keys") u.opt.keys = next();
    else if (a == "--screenshot-after") u.opt.shot_after = std::atof(next());
    else if (a == "--headless") u.opt.headless = true;
    else if (a == "--out") u.opt.out = next();
    else { Usage(); return a == "--help" || a == "-h" ? 0 : 2; }
  }
  if (u.opt.scale < 1 || u.opt.scale > 16) { std::fprintf(stderr, "--scale 1..16\n"); return 2; }
  if (u.opt.headless && u.opt.shot_after < 0) { std::fprintf(stderr, "--headless needs --screenshot-after\n"); return 2; }

  u.p = u.opt.scale >= 6 ? 2 : 1;
  u.W = kOledW * u.opt.scale;
  if (u.W < 64 * 6 * u.p) u.W = 64 * 6 * u.p;  // panel needs 64 columns
  u.oled_h = kOledH * u.opt.scale;
  u.H = u.oled_h + 4 * u.p + 19 * LineH(u);
  u.px.assign((size_t)u.W * u.H, kOff);

  auto name = [](const std::string &n) { return n == "none" ? nullptr : n.c_str(); };
  if (!simcore::Init(name(u.opt.left), name(u.opt.right))) return 2;
  simcore::SetClock(u.opt.bpm, u.opt.ppb, u.opt.clock_tr);
  simcore::ClockEnable(u.opt.clock);
  for (const auto &t : u.opt.trigs) simcore::ScheduleTrigger(t.first, t.second);
  InjectKeys(u);

  const double frame_ticks = (double)simcore::kTickHz / 60.0;  // ~278
  if (u.opt.headless) {
    double acc = 0;
    while (simcore::Seconds() < u.opt.shot_after) {
      acc += frame_ticks;
      uint32_t n = (uint32_t)acc;
      acc -= n;
      simcore::Run(n);
    }
    DrawOled(u, simcore::Render());
    DrawPanel(u);
    Screenshot(u);
    return 0;
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
  SDL_Window *win = SDL_CreateWindow("O_C host sim", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                     u.W, u.H, 0);
  SDL_Renderer *ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : nullptr;
  if (!ren && win) ren = SDL_CreateRenderer(win, -1, 0);
  SDL_Texture *tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, u.W, u.H) : nullptr;
  if (!tex) { std::fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);

  const double freq = (double)SDL_GetPerformanceFrequency();
  uint64_t last = SDL_GetPerformanceCounter();
  double acc = 0, win_wall = 0, win_sim = 0;
  while (!u.quit) {
    const uint64_t now = SDL_GetPerformanceCounter();
    const double wall_dt = (double)(now - last) / freq;
    last = now;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) u.quit = true;
      else if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP)
        OnKey(u, ev.key.keysym.sym, (ev.key.keysym.mod & KMOD_SHIFT) != 0, ev.type == SDL_KEYDOWN,
              ev.key.repeat != 0);
      else if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        // Key-ups are lost while unfocused: release momentary controls (latches stay).
        for (int ch = 0; ch < 4; ++ch) { u.gate_held[ch] = false; ApplyGate(u, ch); }
        u.up_held = u.down_held = u.dual = false;
      }
    }

    if (!u.paused) {
      acc += wall_dt * simcore::kTickHz;
      uint32_t n = (uint32_t)acc;
      if (n > kMaxTicksPerFrame) { n = kMaxTicksPerFrame; acc = 0; }  // fell behind: drop backlog
      else acc -= n;
      simcore::Run(n);
      win_sim += (double)n / simcore::kTickHz;
      win_wall += wall_dt;
      if (win_wall >= 0.5) { u.ratio = win_sim / win_wall; win_sim = win_wall = 0; }
    } else {
      acc = 0; win_sim = win_wall = 0;
    }
    if (u.opt.shot_after >= 0 && simcore::Seconds() >= u.opt.shot_after) {
      DrawOled(u, simcore::Render());
      DrawPanel(u);
      Screenshot(u);
      u.quit = true;
    }

    DrawOled(u, simcore::Render());
    DrawPanel(u);
    SDL_UpdateTexture(tex, nullptr, u.px.data(), u.W * 4);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, nullptr, nullptr);
    SDL_RenderPresent(ren);
    const double spent = (double)(SDL_GetPerformanceCounter() - now) / freq;
    if (spent < 1.0 / 60) SDL_Delay((Uint32)((1.0 / 60 - spent) * 1000));  // in case vsync is off
  }
  SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
