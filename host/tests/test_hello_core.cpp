// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Host unit tests for custom::HelloCore (pure C++17, no stubs).
#include <cstdio>
#include "custom/core/hello_core.h"

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  check failed: %s (line %d)\n", #cond, __LINE__); ok = false; } } while (0)
#define TEST(name) static bool name()
#define RUN(name) do { bool r = name(); std::printf("%s %s\n", r ? "PASS" : "FAIL", #name); if (!r) ++g_fail; } while (0)

using custom::HelloCore;

TEST(pack_unpack_roundtrip) {
  bool ok = true;
  for (int s = HelloCore::kOffsetMin; s <= HelloCore::kOffsetMax; ++s) {
    HelloCore a; a.offset_steps = s;
    HelloCore b; b.Unpack(a.Pack());
    CHECK(b.offset_steps == s);
    CHECK(a.Pack() <= 0xFF);
  }
  return ok;
}

TEST(unpack_clamps_garbage) {
  bool ok = true;
  HelloCore c;
  c.Unpack(0xFF);              // 255 - 60 = 195 -> clamp to max
  CHECK(c.offset_steps == HelloCore::kOffsetMax);
  c.Unpack(0xABCD00);          // only low byte used -> 0 - 60
  CHECK(c.offset_steps == HelloCore::kOffsetMin);
  return ok;
}

TEST(nudge_clamps) {
  bool ok = true;
  HelloCore c;
  for (int i = 0; i < 200; ++i) c.NudgeOffset(+1);
  CHECK(c.offset_steps == HelloCore::kOffsetMax);
  for (int i = 0; i < 500; ++i) c.NudgeOffset(-1);
  CHECK(c.offset_steps == HelloCore::kOffsetMin);
  return ok;
}

TEST(apply_offset) {
  bool ok = true;
  HelloCore c;
  CHECK(c.Apply(1000) == 1000);
  c.NudgeOffset(+12);
  CHECK(c.Apply(0) == 12 * 128);
  c.Reset(); c.NudgeOffset(-3);
  CHECK(c.Apply(-100) == -100 - 3 * 128);
  return ok;
}

TEST(dotx_bounds_long_run) {
  bool ok = true;
  HelloCore c;
  const int w = 60;
  int lo = 1000, hi = -1;
  for (long i = 0; i < 16666L * 20; ++i) {   // 20 simulated seconds
    c.Tick();
    int x = c.DotX(w);
    if (x < 0 || x >= w) { CHECK(x >= 0 && x < w); break; }
    if (x < lo) lo = x;
    if (x > hi) hi = x;
  }
  CHECK(lo == 0 && hi == w - 1);   // full sweep reached both ends
  CHECK(c.DotX(1) == 0 && c.DotX(0) == 0);
  return ok;
}

int main() {
  RUN(pack_unpack_roundtrip);
  RUN(unpack_clamps_garbage);
  RUN(nudge_clamps);
  RUN(apply_offset);
  RUN(dotx_bounds_long_run);
  return g_fail ? 1 : 0;
}
