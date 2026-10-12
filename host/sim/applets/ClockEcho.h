// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// ClockEcho -- host-only sanity applet for the simulator (never in firmware).
//
// Out A: ClockOut() on every Clock(0) (TR1 of this hemisphere).
// Out B: GateOut() follows Clock(1): high on the tick TR2 is clocked, low after
//        ~1 ms (17 ticks), so both output-trigger paths are exercised.
// Counts both inputs on screen. Used to verify --clock / --trig / --log-*.
#pragma once

class ClockEcho : public HemisphereApplet {
public:
  const char* applet_name() { return "ClkEcho"; }

  void Start() { count[0] = count[1] = 0; gate_ticks = 0; }

  void Controller() {
    if (Clock(0)) { ClockOut(0); ++count[0]; }
    if (Clock(1)) { GateOut(1, true); gate_ticks = HEMISPHERE_CLOCK_TICKS; ++count[1]; }
    else if (gate_ticks > 0 && --gate_ticks == 0) GateOut(1, false);
  }

  void View() {
    gfxPrint(1, 15, "TR1 ");
    gfxPrint(count[0]);
    gfxPrint(1, 25, "TR2 ");
    gfxPrint(count[1]);
  }

  void OnEncoderMove(int) {}
  uint64_t OnDataRequest() { return 0; }
  void OnDataReceive(uint64_t) {}

protected:
  void SetHelp() {
    help[HELP_DIGITAL1] = "Clk>A";
    help[HELP_DIGITAL2] = "Clk>B";
    help[HELP_CV1]      = "";
    help[HELP_CV2]      = "";
    help[HELP_OUT1]     = "Trig";
    help[HELP_OUT2]     = "Gate";
    help[HELP_EXTRA1]   = "host sim only";
    help[HELP_EXTRA2]   = "";
  }

private:
  uint32_t count[2];
  int gate_ticks;
};
