// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// clock_mult_list.h -- the one clock multiplier list shared by ClockSetup (edited
// under CUSTOM_BUILD) and our applets. Values in ClockManager terms: >0 = xN,
// <0 = /(1-N), 0 = off. Rules: docs/CLOCK.md. Pure constexpr, integer only.
#pragma once
#include <stdint.h>

namespace custom {
namespace clock_mult {

// off /32 /24 /16 /12 /8 /7 /6 /5 /4 /3 /2 x1..x8 x12 x16 x24 (23 entries)
constexpr int8_t kList[] = {0, -31, -23, -15, -11, -7, -6, -5, -4, -3, -2, -1,
                            1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 24};
constexpr int Count() { return (int)(sizeof(kList) / sizeof(kList[0])); }
constexpr int8_t At(int i) { return kList[i < 0 ? 0 : (i >= Count() ? Count() - 1 : i)]; }

// Index of value in kList, -1 if not in the list.
constexpr int IndexOf(int value) {
  for (int i = 0; i < Count(); ++i) if (kList[i] == value) return i;
  return -1;
}

// Sort key: the list is ascending in rate with off below /32.
constexpr int Key(int v) { return v == 0 ? -128 : v; }

// Next list entry in direction dir (sign only), clamped at the ends. A value not in
// the list (e.g. x9 from an old preset) snaps to the nearest entry in that direction.
// lo = lowest selectable index (1 skips off).
constexpr int8_t Step(int value, int dir, int lo = 0) {
  if (dir == 0) return (int8_t)value;
  const int hi = Count() - 1;
  int i = IndexOf(value);
  if (i >= 0) {
    i += dir > 0 ? 1 : -1;
  } else if (dir > 0) {
    i = hi;
    for (int k = hi; k >= 0; --k) if (Key(kList[k]) > Key(value)) i = k;
  } else {
    i = 0;
    for (int k = 0; k <= hi; ++k) if (Key(kList[k]) < Key(value)) i = k;
  }
  if (i < lo) i = lo;
  if (i > hi) i = hi;
  return kList[i];
}

// ClockSetup notation into buf (>= 5 chars): "off", "x4", "/2". Integer only.
inline const char* Label(int m, char* buf) {
  if (m == 0) { buf[0] = 'o'; buf[1] = 'f'; buf[2] = 'f'; buf[3] = 0; return buf; }
  int v = m > 0 ? m : 1 - m;
  int n = 0;
  buf[n++] = m > 0 ? 'x' : '/';
  if (v >= 10) buf[n++] = (char)('0' + v / 10);
  buf[n++] = (char)('0' + v % 10);
  buf[n] = 0;
  return buf;
}

// Division notation "/n" for a pulse divider n >= 1 (Likembe Src TRn Div): "/1".."/32".
// "/1" has no ClockManager value (x1 = 1), hence the special case here.
inline const char* DivLabel(int n, char* buf) {
  if (n <= 1) { buf[0] = '/'; buf[1] = '1'; buf[2] = 0; return buf; }
  return Label(1 - n, buf);
}

}  // namespace clock_mult
}  // namespace custom
