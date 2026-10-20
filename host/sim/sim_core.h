// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Shared simulator core: the one ISR tick loop and frame render used by both
// the scripted harness (sim_main.cpp) and the SDL2 window (ui_main.cpp).
//
// This header deliberately includes no firmware headers, so front ends can be
// compiled with -Wall -Wextra while sim_core.cpp (which includes the real,
// noisy upstream headers) is compiled with -w.
#pragma once
#include <cstdint>

namespace simcore {

constexpr int kTickHz = 16666;       // OC_CORE_ISR_FREQ (checked in sim_core.cpp)
constexpr int kUnitsPerVolt = 1536;  // O_C pitch units: 128 per semitone
constexpr int kChannels = 4;         // CV in / TR in / outputs A-D
constexpr int kFrameBytes = 128 * 64 / 8;

enum Side { kLeft = 0, kRight = 1 };

// Boot: frame, HS::Init (default cvmap/trigmap), start applets.
// left/right: applet name from the host registry (host_applets.h, matched
// case-insensitively), nullptr = empty hemisphere. Returns false (and boots
// nothing) on an unknown name.
bool Init(const char *left, const char *right);
const char *AppletNames();           // "Hello, ClockEcho, ..." for usage texts
bool Active(Side s);

void Tick();                         // one ISR tick (all active applets)
void Run(uint32_t n);                // n ticks
const uint8_t *Render();             // header + View() of each active applet
uint32_t Ticks();
double Seconds();

// Inputs
void SetCv(int ch, int32_t units);
int32_t Cv(int ch);
void SetGate(int ch, bool high);
bool Gate(int ch);

// Periodic clock source: pulses of kTrigTicks on physical TR `tr` (0..3),
// bpm * ppb pulses per minute. Gate level = manual gate OR any pulse.
constexpr int kTrigTicks = 17;       // ~1 ms, HEMISPHERE_CLOCK_TICKS
void SetClock(double bpm, int ppb, int tr);
void SetClockBpm(double bpm);        // keeps phase (next pulse = last + period)
void ClockEnable(bool on);           // enabling fires the first pulse on the next tick
bool ClockEnabled();
double ClockBpm();
int ClockPpb();
int ClockTr();
// One kTrigTicks pulse on TR `tr` starting at simulated time t seconds
// (fires on the next tick if t already passed).
void ScheduleTrigger(int tr, double t);

// Internal clock: the real HS::clock_m driven by the real ClockSetup::Controller()
// each tick (host_clocksetup.cpp). External sync = TR1 pulses at the input PPQN.
void IntStart(bool armed = false);   // clock_m.Start(): running now; armed = paused, starts on TR1
void IntStop();
bool IntRunning();                   // clock_m.IsRunning()
bool IntArmed();                     // clock_m.IsPaused()
void IntSetBpm(int bpm);             // SetTempoBPM (1..65535)
int IntBpm();
// Per-slot multiplier (slot 0..3 = left in 1, 2, right in 1, 2): 1..24 = x1..x24,
// -1..-31 = /2../32, 0 = off (slot uses the trigger map even while running).
void IntSetMultiply(int slot, int m);
int IntMultiply(int slot);
void IntSetPpqn(int ppqn);           // external clock pulses per beat (0..24)
int IntPpqn();

// Outputs (pitch units, frame.ViewOut)
int32_t Out(int ch);
// Output observation, evaluated after every tick (after frame.Send()).
// Change: any ViewOut value change. Trigger: ClockOut() fired (countdown
// started/restarted) or the value jumped up to exactly the GateOut level.
enum TrigKind { kTrigClockOut = 0, kTrigGateOut = 1 };
struct OutListener {
  virtual ~OutListener() {}
  virtual void OnChange(double t, int ch, int32_t units) { (void)t; (void)ch; (void)units; }
  virtual void OnTrigger(double t, int ch, TrigKind kind) { (void)t; (void)ch; (void)kind; }
};
void SetOutListener(OutListener *l);  // nullptr = none
// Tick of the last output trigger on ch, or -1 if none yet.
int64_t LastOutTrigTick(int ch);

// UI controls, mirroring what APP_HEMISPHERE delegates on button release.
void EncoderMove(Side s, int detents);   // OnEncoderMove per detent
void EncoderPress(Side s);               // OnButtonPress (toggles edit mode)
// Up (A, left) / Down (B, right) button single click: AuxButton() while the
// applet edits, otherwise APP_HEMISPHERE would toggle select mode (no manager
// here). Returns true if it reached the applet.
bool SelectButton(Side s);
void ResetApplet(Side s);                // Reset() then Start()
bool EditMode(Side s);
const char *AppletName(Side s);

}  // namespace simcore
