// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// likembe_clock.h -- Likembe Clock page settings (Src, Div per source, Roll, Rst)
// and the pulse divider. Glue-level (the pattern core never sees them), kept here
// so they compile and test on the host. Rules: docs/CLOCK.md; page: docs/modules/likembe.md.
#pragma once
#include <cstdint>
#include "custom/core/likembe_constants.h"

namespace custom {

// Packed into bits 48..63 of the applet's uint64 (the core uses all of 0..47):
//  48..54 inputs (7): src + 5 * roll_in + 25 * rst_in (each 0..4); 125..127 -> defaults
//  55..59 div_int - 1 (5; 24..31 -> 24)
//  60..63 div_tr - 1 (4; 1..16)
// src/roll/rst share one base-5 field: three 3-bit fields plus two 5-bit Divs
// would need 19 bits, only 16 are free.
struct LikembeClock {
  static constexpr int kShift = 48;
  uint8_t src;      // likembe::ClockSrc
  uint8_t div_int;  // pulses per step with Src Int, kClockDivMin..kClockDivIntMax
  uint8_t div_tr;   // pulses per step with Src TRn, kClockDivMin..kClockDivTrMax
  uint8_t roll_in;  // likembe::RollIn
  uint8_t rst_in;   // likembe::RstIn

  static LikembeClock Defaults() {
    using namespace likembe;
    return {kSrcInt, kDefaultDivInt, kDefaultDivTr, kDefaultRoll, kRstOff};
  }
  // Div for the current Src.
  uint8_t Div() const { return src == likembe::kSrcInt ? div_int : div_tr; }
  uint8_t DivMax() const { return src == likembe::kSrcInt ? likembe::kClockDivIntMax : likembe::kClockDivTrMax; }
  void SetDiv(int v) {
    if (v < likembe::kClockDivMin) v = likembe::kClockDivMin;
    if (v > DivMax()) v = DivMax();
    (src == likembe::kSrcInt ? div_int : div_tr) = (uint8_t)v;
  }
  void Clamp() {
    using namespace likembe;
    if (src >= kSrcCount) src = kSrcInt;
    if (roll_in >= kRollCount) roll_in = kDefaultRoll;
    if (rst_in >= kRstCount) rst_in = kRstOff;
    if (div_int < kClockDivMin) div_int = kClockDivMin;
    if (div_int > kClockDivIntMax) div_int = kClockDivIntMax;
    if (div_tr < kClockDivMin) div_tr = kClockDivMin;
    if (div_tr > kClockDivTrMax) div_tr = kClockDivTrMax;
  }
  uint64_t Pack() const {
    const uint64_t in = (uint64_t)(src % 5) + 5u * (roll_in % 5) + 25u * (rst_in % 5);
    const uint64_t d = in | (uint64_t)((div_int - 1) & 0x1F) << 7 | (uint64_t)((div_tr - 1) & 0xF) << 12;
    return d << kShift;
  }
  static LikembeClock Unpack(uint64_t d) {
    d >>= kShift;
    LikembeClock c = Defaults();
    const uint8_t in = (uint8_t)(d & 0x7F);
    if (in < 125) { c.src = in % 5; c.roll_in = (in / 5) % 5; c.rst_in = in / 25; }
    c.div_int = (uint8_t)(((d >> 7) & 0x1F) + 1);
    c.div_tr = (uint8_t)(((d >> 12) & 0xF) + 1);
    c.Clamp();
    return c;
  }
};

// One step every `div` input pulses, decrement only: fires on the first pulse
// after Reset(); a Div lowered below the remaining count fires on the next pulse.
struct PulseDivider {
  uint8_t left = 0;   // pulses until the next step; 0 = fire on the next
  void Reset() { left = 0; }
  bool Tick(uint8_t div) {
    const bool fire = left == 0 || left > div;
    if (fire) left = div;
    --left;
    return fire;
  }
};

}  // namespace custom
