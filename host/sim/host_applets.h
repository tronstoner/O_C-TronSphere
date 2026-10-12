// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Host applet registry: name -> factory, used by --left / --right of both
// front ends (sim_core.cpp is the only includer). The firmware registry in
// hemisphere_config.h is not compiled on the host (see host_firmware.cpp).
//
// Add new applets here: include the header and add one kHostApplets row.
// Names are matched case-insensitively.
#pragma once
#include "HemisphereApplet.h"
#include "custom/applets/Hello.h"
#include "custom/applets/Likembe.h"
#include "host/sim/applets/ClockEcho.h"

namespace simcore {

struct HostApplet {
  const char *name;
  HemisphereApplet *(*make)();
};

template <class T> HemisphereApplet *MakeApplet() { return new T(); }

// ---- add new applets here (one line each) ----
inline const HostApplet kHostApplets[] = {
    {"Hello", MakeApplet<Hello>},
    {"ClockEcho", MakeApplet<ClockEcho>},
    {"Likembe", MakeApplet<Likembe>},
};

}  // namespace simcore
