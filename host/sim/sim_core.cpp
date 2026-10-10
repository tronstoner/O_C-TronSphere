// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// See sim_core.h. Compiled with the firmware include paths and -w.
#include "sim_core.h"
#include "HemisphereApplet.h"
#include "custom/applets/Hello.h"

static_assert(simcore::kTickHz == OC_CORE_ISR_FREQ, "tick rate mismatch");
static_assert(simcore::kFrameBytes == weegfx::Graphics::kFrameSize, "frame size mismatch");

namespace simcore {
namespace {
Hello applets[2];
bool active[2] = {false, false};
uint8_t fb[weegfx::Graphics::kFrameSize];
const uint8_t kPins[4] = {TR1, TR2, TR3, TR4};
HemisphereApplet *A(Side s) { return active[s] ? &applets[s] : nullptr; }
}  // namespace

void Init(bool left, bool right) {
  HS::frame.Init();
  HS::Init();  // quantizers + default I/O mappings (cvmap/trigmap), as at boot
  active[kLeft] = left;
  active[kRight] = right;
  if (left) applets[kLeft].BaseStart(LEFT_HEMISPHERE);
  if (right) applets[kRight].BaseStart(RIGHT_HEMISPHERE);
}
bool Active(Side s) { return active[s]; }

// One ISR tick, mirroring HSApplication::BaseController + HemisphereManager.
void Tick() {
  ++OC::CORE::ticks;
  host_micros += 1000000 / OC_CORE_ISR_FREQ;
  HS::frame.Load();
  for (int h = 0; h < 2; ++h)
    if (active[h]) applets[h].Controller();
  HemisphereApplet::ProcessCursors();
  HS::frame.Send();
}
void Run(uint32_t n) { for (uint32_t i = 0; i < n; ++i) Tick(); }

// Draw like the Hemisphere app does (header + View() per half).
const uint8_t *Render() {
  graphics.Begin(fb, weegfx::CLEAR_FRAME_ENABLE);
  for (int h = 0; h < 2; ++h)
    if (active[h]) applets[h].BaseView();
  graphics.End();
  return fb;
}
uint32_t Ticks() { return OC::CORE::ticks; }
double Seconds() { return (double)OC::CORE::ticks / OC_CORE_ISR_FREQ; }

void SetCv(int ch, int32_t units) { host_adc_pitch[ch & 3] = units; }
int32_t Cv(int ch) { return host_adc_pitch[ch & 3]; }
void SetGate(int ch, bool high) { host_pin_level[kPins[ch & 3]] = high ? LOW : HIGH; }  // active low
bool Gate(int ch) { return host_pin_level[kPins[ch & 3]] == LOW; }
int32_t Out(int ch) { return HS::frame.ViewOut(ch & 3); }

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
