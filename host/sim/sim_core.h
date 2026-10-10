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
// left/right: whether a Hello instance runs in that hemisphere.
void Init(bool left, bool right);
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

// Outputs (pitch units, frame.ViewOut)
int32_t Out(int ch);

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
