// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// The real upstream ClockSetup applet (applets/ClockSetup.h, with our CUSTOM_BUILD
// edits: shared multiplier list, docs/UPSTREAM.md) on the
// host, plus the minimal hardware stand-ins its Controller() touches:
//   OC::DigitalInputs  (OC_digital_inputs.cpp: pin ISRs + Scan, TR1 sync input)
//   OC::ui._Poke()     (OC_ui.cpp: screensaver wake-up, no-op here)
// and the hooks sim_core uses to show and operate the ClockSetup screen the way
// APP_HEMISPHERE.h does (View() overlay, encoder / encoder-push routing).
// HS::clock_m itself is defined in HemisphereApplet.cpp (already compiled).
// See docs/HOST_SIM.md "Internal clock (ClockSetup)".
// Same define as env custom_min; only this unit needs it (ClockSetup edits).
#ifndef CUSTOM_BUILD
#define CUSTOM_BUILD
#endif
#include "HemisphereApplet.h"
#include "OC_digital_inputs.h"
#include "OC_ui.h"
#include "applets/ClockSetup.h"

// --- OC_apps.cpp ---
ClockSetup ClockSetup_instance;

// --- OC_ui.cpp ---
namespace OC {
Ui ui;
void Ui::_Poke() {}
}  // namespace OC

// --- OC_digital_inputs.cpp (same Scan as upstream; ISRs set by sim_core) ---
uint32_t OC::DigitalInputs::clocked_mask_;
volatile uint32_t OC::DigitalInputs::clocked_[DIGITAL_INPUT_LAST];
void OC::DigitalInputs::Scan() {
  clocked_mask_ = ScanInput<DIGITAL_INPUT_1>() | ScanInput<DIGITAL_INPUT_2>() |
                  ScanInput<DIGITAL_INPUT_3>() | ScanInput<DIGITAL_INPUT_4>();
}
void OC::tr1_ISR() { DigitalInputs::clock<DIGITAL_INPUT_1>(); }
void OC::tr2_ISR() { DigitalInputs::clock<DIGITAL_INPUT_2>(); }
void OC::tr3_ISR() { DigitalInputs::clock<DIGITAL_INPUT_3>(); }
void OC::tr4_ISR() { DigitalInputs::clock<DIGITAL_INPUT_4>(); }

// --- sim_core hooks (sim_core.cpp has no ClockSetup include) ---
void HostTrigIsr(int ch) {  // falling pin edge = rising trigger, like attachInterrupt(FALLING)
  switch (ch & 3) {
    case 0: OC::tr1_ISR(); break;
    case 1: OC::tr2_ISR(); break;
    case 2: OC::tr3_ISR(); break;
    default: OC::tr4_ISR(); break;
  }
}
// Order as on hardware: core ISR scans the inputs, then HSApplication::BaseController
// runs ClockSetup_instance.Controller() before frame.Load().
void HostClockSetupController() {
  OC::DigitalInputs::Scan();
  ClockSetup_instance.Controller();
}

// APP_HEMISPHERE.h View(): "clock screen is an overlay" -> ClockSetup_instance.View()
void HostClockSetupView() { ClockSetup_instance.View(); }
// APP_HEMISPHERE.h DelegateEncoderMovement(): left encoder -> OnLeftEncoderMove,
// right encoder -> OnEncoderMove.
void HostClockSetupEncoder(bool left, int direction) {
  if (left) ClockSetup_instance.OnLeftEncoderMove(direction);
  else ClockSetup_instance.OnEncoderMove(direction);
}
// APP_HEMISPHERE.h DelegateEncoderPush(): either encoder push -> OnButtonPress().
void HostClockSetupPress() { ClockSetup_instance.OnButtonPress(); }
