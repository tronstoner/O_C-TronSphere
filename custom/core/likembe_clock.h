// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// likembe_clock.h -- Likembe Clock page settings (Src, TRn Div, Roll, Rst)
// and the pulse divider. Glue-level (the pattern core never sees them), kept here
// so they compile and test on the host. Rules: docs/CLOCK.md; page: docs/modules/likembe.md.
#pragma once
#include <cstdint>
#include "custom/core/likembe_constants.h"
#include "custom/core/clock_mult_list.h"

namespace custom {

// Packed into bits 48..60 of the applet's uint64 (the core uses all of 0..47):
//  48..50 src (3)         likembe::ClockSrc, >= kSrcCount -> Int
//  51..53 roll_in (3)     likembe::RollIn,   >= kRollCount -> default
//  54..56 rst_in (3)      likembe::RstIn,    >= kRstCount -> Off
//  57..60 div_tr_idx (4)  index into kTrDivs, >= kTrDivCount -> default
//  61..63 unused (0)
// The Src Int Div is the ClockSetup slot multiplier; ClockSetup persists it, not us.
struct LikembeClock {
  static constexpr int kShift = 48;
  uint8_t src;         // likembe::ClockSrc
  uint8_t div_tr_idx;  // Src TRn: pulses per step = kTrDivs[div_tr_idx]
  uint8_t roll_in;     // likembe::RollIn
  uint8_t rst_in;      // likembe::RstIn

  static LikembeClock Defaults() {
    using namespace likembe;
    return {kSrcInt, kDefaultTrDivIdx, kDefaultRoll, kRstOff};
  }
  uint8_t TrDiv() const { return likembe::kTrDivs[div_tr_idx < likembe::kTrDivCount ? div_tr_idx : 0]; }
  void StepTrDiv(int dir) {
    int i = div_tr_idx + dir;
    if (i < 0) i = 0;
    if (i >= likembe::kTrDivCount) i = likembe::kTrDivCount - 1;
    div_tr_idx = (uint8_t)i;
  }
  void Clamp() {
    using namespace likembe;
    if (src >= kSrcCount) src = kSrcInt;
    if (roll_in >= kRollCount) roll_in = kDefaultRoll;
    if (rst_in >= kRstCount) rst_in = kRstOff;
    if (div_tr_idx >= kTrDivCount) div_tr_idx = kDefaultTrDivIdx;
  }
  uint64_t Pack() const {
    const uint64_t d = (uint64_t)(src & 7) | (uint64_t)(roll_in & 7) << 3 |
                       (uint64_t)(rst_in & 7) << 6 | (uint64_t)(div_tr_idx & 0xF) << 9;
    return d << kShift;
  }
  static LikembeClock Unpack(uint64_t d) {
    d >>= kShift;
    LikembeClock c;
    c.src = (uint8_t)(d & 7);
    c.roll_in = (uint8_t)((d >> 3) & 7);
    c.rst_in = (uint8_t)((d >> 6) & 7);
    c.div_tr_idx = (uint8_t)((d >> 9) & 0xF);
    c.Clamp();
    return c;
  }
};

// Src Int Div encoder step: next entry of the shared list, off excluded.
inline int8_t StepIntMult(int cur, int dir) {
  return clock_mult::Step(cur, dir, likembe::kIntMultLo);
}

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
