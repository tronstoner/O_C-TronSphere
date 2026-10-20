// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// likembe_clock.h -- Likembe Clock page settings (Src, Div, Roll, Rst) and the
// pulse divider. Glue-level (the pattern core never sees them), kept here so
// they compile and test on the host. Rules: docs/CLOCK.md; page: docs/modules/likembe.md.
#pragma once
#include <cstdint>
#include "custom/core/likembe_constants.h"

namespace custom {

// Packed into bits 48..61 of the applet's uint64 (the core uses 0..47):
//  48..50 src (3; 5..7 -> Int)  51..55 div - kClockDivMin (5; 24..31 -> 24)
//  56..58 roll_in (3; 5..7 -> hemisphere default)
//  59..61 rst (3): 0 Off, 2..5 TR1..TR4 (stored rst_in + 1); 1 (former Int), 6, 7 -> Off
struct LikembeClock {
  static constexpr int kShift = 48;
  uint8_t src;      // likembe::ClockSrc
  uint8_t div;      // kClockDivMin..kClockDivMax, used with Src TRn only
  uint8_t roll_in;  // likembe::RollIn
  uint8_t rst_in;   // likembe::RstIn

  static LikembeClock Defaults(bool right) {
    using namespace likembe;
    return {kSrcInt, kDefaultClockDiv, right ? kDefaultRollRight : kDefaultRollLeft, kDefaultRst};
  }
  void Clamp(bool right) {
    using namespace likembe;
    if (src >= kSrcCount) src = kSrcInt;
    if (div < kClockDivMin) div = kClockDivMin;
    if (div > kClockDivMax) div = kClockDivMax;
    if (roll_in >= kRollCount) roll_in = Defaults(right).roll_in;
    if (rst_in >= kRstCount) rst_in = kDefaultRst;
  }
  uint64_t Pack() const {
    uint64_t d = (uint64_t)(src & 0x7) | (uint64_t)((div - likembe::kClockDivMin) & 0x1F) << 3 |
                 (uint64_t)(roll_in & 0x7) << 8 | (uint64_t)(rst_in == likembe::kRstOff ? 0 : (rst_in + 1) & 0x7) << 11;
    return d << kShift;
  }
  static LikembeClock Unpack(uint64_t d, bool right) {
    d >>= kShift;
    LikembeClock c;
    c.src = (uint8_t)(d & 0x7);
    c.div = (uint8_t)(((d >> 3) & 0x1F) + likembe::kClockDivMin);
    c.roll_in = (uint8_t)((d >> 8) & 0x7);
    const uint8_t r = (uint8_t)((d >> 11) & 0x7);
    c.rst_in = r >= 2 ? (uint8_t)(r - 1) : likembe::kRstOff;   // 1 = former Int -> Off
    c.Clamp(right);
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
