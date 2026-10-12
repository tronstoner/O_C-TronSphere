// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// See sim_core.h. Compiled with the firmware include paths and -w.
#include "sim_core.h"
#include <cstdio>
#include <string>
#include <strings.h>
#include <vector>
#include "host_applets.h"

static_assert(simcore::kTickHz == OC_CORE_ISR_FREQ, "tick rate mismatch");
static_assert(simcore::kFrameBytes == weegfx::Graphics::kFrameSize, "frame size mismatch");
static_assert(simcore::kTrigTicks == HEMISPHERE_CLOCK_TICKS, "trigger width mismatch");

namespace simcore {
namespace {
HemisphereApplet *applets[2] = {nullptr, nullptr};
uint8_t fb[weegfx::Graphics::kFrameSize];
const uint8_t kPins[4] = {TR1, TR2, TR3, TR4};
HemisphereApplet *A(Side s) { return applets[s]; }

// Gate inputs: manual level OR pulse (clock source / scheduled trigger).
bool manual[4] = {};
uint32_t pulse_until[4] = {};  // pin high while ticks < pulse_until

struct Clock {
  bool on = false;
  double bpm = 120, period = 0;  // period in ticks
  int ppb = 4, tr = 0;
  double next = 0, last = -1;    // tick of next / last pulse start
} clk;
double Period(double bpm, int ppb) { return (double)kTickHz * 60.0 / (bpm * ppb); }

struct Pending { uint32_t tick; int tr; };
std::vector<Pending> pending;

OutListener *listener = nullptr;
int32_t prev_out[4] = {};
int prev_countdown[4] = {};
int64_t last_trig[4] = {-1, -1, -1, -1};

void Pulse(int tr, uint32_t now) { pulse_until[tr & 3] = now + kTrigTicks; }
void ApplyPins(uint32_t now) {
  for (int ch = 0; ch < 4; ++ch)
    host_pin_level[kPins[ch]] = (manual[ch] || now < pulse_until[ch]) ? LOW : HIGH;  // active low
}
HemisphereApplet *Make(const char *name, bool *ok) {
  if (!name) return nullptr;
  for (const auto &e : kHostApplets)
    if (strcasecmp(e.name, name) == 0) return e.make();
  std::fprintf(stderr, "unknown applet '%s' (available: %s)\n", name, AppletNames());
  *ok = false;
  return nullptr;
}
}  // namespace

bool Init(const char *left, const char *right) {
  bool ok = true;
  HemisphereApplet *l = Make(left, &ok), *r = Make(right, &ok);
  if (!ok) { delete l; delete r; return false; }
  HS::frame.Init();
  HS::Init();  // quantizers + default I/O mappings (cvmap/trigmap), as at boot
  applets[kLeft] = l;
  applets[kRight] = r;
  if (l) l->BaseStart(LEFT_HEMISPHERE);
  if (r) r->BaseStart(RIGHT_HEMISPHERE);
  for (int ch = 0; ch < 4; ++ch) prev_out[ch] = HS::frame.ViewOut((DAC_CHANNEL)ch);
  return true;
}
const char *AppletNames() {
  static std::string s;
  if (s.empty())
    for (const auto &e : kHostApplets) s += (s.empty() ? "" : ", ") + std::string(e.name);
  return s.c_str();
}
bool Active(Side s) { return applets[s] != nullptr; }

// One ISR tick, mirroring HSApplication::BaseController + HemisphereManager.
void Tick() {
  const uint32_t now = ++OC::CORE::ticks;
  host_micros += 1000000 / OC_CORE_ISR_FREQ;
  // Input sources first, so frame.Load() sees this tick's pin levels.
  if (clk.on && now >= clk.next) {
    Pulse(clk.tr, now);
    clk.last = clk.next;
    clk.next += clk.period;
    if (clk.next <= now) clk.next = now + clk.period;  // never burst
  }
  for (size_t i = 0; i < pending.size();)
    if (pending[i].tick <= now) { Pulse(pending[i].tr, now); pending.erase(pending.begin() + i); }
    else ++i;
  ApplyPins(now);

  HS::frame.Load();
  for (int h = 0; h < 2; ++h)
    if (applets[h]) applets[h]->Controller();
  HemisphereApplet::ProcessCursors();
  HS::frame.Send();

  // Output observation.
  const int32_t gate_level = PULSE_VOLTAGE * ONE_OCTAVE;
  const double t = Seconds();
  for (int ch = 0; ch < 4; ++ch) {
    const int32_t v = HS::frame.ViewOut((DAC_CHANNEL)ch);
    const int cd = HS::frame.clock_countdown[ch];
    const bool clock_trig = cd > (prev_countdown[ch] > 0 ? prev_countdown[ch] - 1 : 0);
    const bool gate_trig = !clock_trig && v == gate_level && prev_out[ch] < gate_level;
    if (v != prev_out[ch] && listener) listener->OnChange(t, ch, v);
    if (clock_trig || gate_trig) {
      last_trig[ch] = now;
      if (listener) listener->OnTrigger(t, ch, clock_trig ? kTrigClockOut : kTrigGateOut);
    }
    prev_out[ch] = v;
    prev_countdown[ch] = cd;
  }
}
void Run(uint32_t n) { for (uint32_t i = 0; i < n; ++i) Tick(); }

// Draw like the Hemisphere app does (header + View() per half).
const uint8_t *Render() {
  graphics.Begin(fb, weegfx::CLEAR_FRAME_ENABLE);
  for (int h = 0; h < 2; ++h)
    if (applets[h]) applets[h]->BaseView();
  graphics.End();
  return fb;
}
uint32_t Ticks() { return OC::CORE::ticks; }
double Seconds() { return (double)OC::CORE::ticks / OC_CORE_ISR_FREQ; }

void SetCv(int ch, int32_t units) { host_adc_pitch[ch & 3] = units; }
int32_t Cv(int ch) { return host_adc_pitch[ch & 3]; }
void SetGate(int ch, bool high) { manual[ch & 3] = high; ApplyPins(OC::CORE::ticks); }
bool Gate(int ch) { return host_pin_level[kPins[ch & 3]] == LOW; }
int32_t Out(int ch) { return HS::frame.ViewOut(ch & 3); }

void SetClock(double bpm, int ppb, int tr) {
  clk.bpm = bpm; clk.ppb = ppb; clk.tr = tr & 3;
  clk.period = Period(bpm, ppb);
}
void SetClockBpm(double bpm) {
  clk.bpm = bpm;
  clk.period = Period(bpm, clk.ppb);
  if (clk.on && clk.last >= 0) {
    clk.next = clk.last + clk.period;
    if (clk.next <= OC::CORE::ticks) clk.next = OC::CORE::ticks + 1;
  }
}
void ClockEnable(bool on) {
  if (on && !clk.on) { clk.next = OC::CORE::ticks + 1; clk.last = -1; }
  clk.on = on;
}
bool ClockEnabled() { return clk.on; }
double ClockBpm() { return clk.bpm; }
int ClockPpb() { return clk.ppb; }
int ClockTr() { return clk.tr; }
void ScheduleTrigger(int tr, double t) {
  long k = (long)(t * kTickHz + 0.5);
  pending.push_back({(uint32_t)(k < 1 ? 1 : k), tr & 3});
}

void SetOutListener(OutListener *l) { listener = l; }
int64_t LastOutTrigTick(int ch) { return last_trig[ch & 3]; }

void EncoderMove(Side s, int n) {
  if (auto *a = A(s))
    for (int k = 0; k < (n < 0 ? -n : n); ++k) a->OnEncoderMove(n > 0 ? 1 : -1);
}
void EncoderPress(Side s) { if (auto *a = A(s)) a->OnButtonPress(); }
bool SelectButton(Side s) {
  auto *a = A(s);
  if (!a || !a->EditMode()) return false;
  a->AuxButton();
  return true;
}
void ResetApplet(Side s) { if (auto *a = A(s)) { a->Reset(); a->Start(); } }
bool EditMode(Side s) { auto *a = A(s); return a && a->EditMode(); }
const char *AppletName(Side s) { auto *a = A(s); return a ? a->applet_name() : "-"; }

}  // namespace simcore
