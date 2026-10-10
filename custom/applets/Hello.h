// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Hello -- smoke-test applet and template for all custom applets (id 200).
//
// Core/glue split: all logic lives in custom/core/hello_core.h (pure C++17,
// host-testable). This header only adapts it to the HemisphereApplet API:
// I/O (In/Out), encoder, display, persistence and help text.
//
// Behaviour: Out(ch) = In(ch) + offset (semitone steps, -60..+60).
#pragma once
#include "custom/core/hello_core.h"

class Hello : public HemisphereApplet {
public:
  const char* applet_name() { return "Hello"; }

  void Start() { core.Reset(); }

  void Controller() {
    core.Tick();
    ForEachChannel(ch) {
      Out(ch, constrain(core.Apply(In(ch)), HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV));
    }
  }

  void View() {
    gfxPrint(1, 15, "Hello!");
    gfxPrint(1, 28, "Ofs ");
    if (core.offset_steps >= 0) gfxPrint("+");
    gfxPrint(core.offset_steps);
    gfxCursor(25, 37, 30);
    gfxRect(core.DotX(60) + 1, 50, 3, 3);
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) return;  // single parameter: no cursor movement
    core.NudgeOffset(direction);
  }

  uint64_t OnDataRequest() { return core.Pack(); }
  void OnDataReceive(uint64_t data) { core.Unpack(data); }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "";
    help[HELP_DIGITAL2] = "";
    help[HELP_CV1]      = "In A";
    help[HELP_CV2]      = "In B";
    help[HELP_OUT1]     = "A+ofs";
    help[HELP_OUT2]     = "B+ofs";
    help[HELP_EXTRA1]   = "Enc: offset in";
    help[HELP_EXTRA2]   = "semitones";
    //                    "---------------------" <-- Extra text size guide
  }

private:
  custom::HelloCore core;

};
