// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Host unit tests for custom::LikembeCore (structural/property tests; the RNG
// differs from the JS reference, so no bit-exact comparison).
#include <cstdio>
#include <cstring>
#include "custom/core/likembe_core.h"
#include "custom/core/likembe_clock.h"

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  check failed: %s (line %d)\n", #cond, __LINE__); ok = false; } } while (0)
#define TEST(name) static bool name()
#define RUN(name) do { bool r = name(); std::printf("%s %s\n", r ? "PASS" : "FAIL", #name); if (!r) ++g_fail; } while (0)

using custom::LikembeCore;
using custom::LikembeParams;
using custom::LikembeEvent;
namespace lk = custom::likembe;

// ASCII dump of the current bar: one row per thumb, 'x' = note, 'o' = rest
// stroke, '.' = other thumb / offset. Printed on failure.
static void Dump(const LikembeCore& c, const char* tag) {
  char row[2][33];
  for (int th = 0; th < 2; ++th) {
    for (int i = 0; i < c.pulses_per_bar; ++i) {
      int j = i - c.Profile().offset;
      LikembeEvent e = c.EventAt((uint8_t)i);
      row[th][i] = (j < 0 || (j & 1) != th) ? '.' : (e.note ? 'x' : 'o');
    }
    row[th][c.pulses_per_bar] = 0;
  }
  std::printf("  [%s] part %d seed %u nL=%d phL=%d nR=%d phR=%d\n  L %s\n  R %s\n", tag, c.params.part,
              c.params.seed, c.thumb[0].n, c.thumb[0].phase, c.thumb[1].n, c.thumb[1].phase, row[0], row[1]);
}

static const uint8_t kTestLens[] = {12, 16, 24, 32};
static LikembeCore Make(uint8_t part, uint16_t seed, uint8_t len = lk::kDefaultLen) {
  LikembeParams p = LikembeParams::Defaults(part);
  p.seed = seed; p.len = len;
  LikembeCore c; c.Init(p);
  return c;
}
static void RunBar(LikembeCore& c, LikembeEvent* out = nullptr) {
  int n = c.pulses_per_bar;
  for (int i = 0; i < n; ++i) { LikembeEvent e = c.Pulse(); if (out) out[i] = e; }
}

TEST(determinism) {
  bool ok = true;
  for (uint8_t part = 0; part < lk::kPartCount; ++part) {
    LikembeCore a = Make(part, 1234), b = Make(part, 1234);
    a.params.passage_idx = b.params.passage_idx = 2;
    for (int i = 0; i < 64 * 16; ++i) {
      LikembeEvent x = a.Pulse(), y = b.Pulse();
      if (std::memcmp(&x, &y, sizeof x) != 0) { CHECK(false); break; }
    }
    LikembeCore c = Make(part, 1235);
    bool differ = false;
    LikembeCore d = Make(part, 1234);
    for (int i = 0; i < 16 * 16; ++i) { LikembeEvent x = c.Pulse(), y = d.Pulse(); if (x.note != y.note || x.degree != y.degree) differ = true; }
    CHECK(differ);
  }
  return ok;
}

// Fit: lead / mi-solo policy keeps both loop lengths dividing the bar; thumbs unequal;
// one-drift parts always have at least one fitting thumb. Checked across rerolls,
// passages and loop mutations.
TEST(loop_policies_and_unequal) {
  bool ok = true;
  for (uint8_t part = 0; part < lk::kPartCount; ++part)
    for (uint8_t g : kTestLens) {
      LikembeCore c = Make(part, 77, g);
      c.params.variation_pct = 100; c.params.passage_idx = 1;
      for (int bar = 0; bar < 300; ++bar) {
        if (bar % 37 == 0) c.Reroll();
        RunBar(c);
        uint8_t l = c.thumb[0].n, r = c.thumb[1].n;
        bool good = l != r && l >= lk::kMinLoop && r >= lk::kMinLoop && l <= lk::kMaxLoop && r <= lk::kMaxLoop;
        if (c.Profile().policy == lk::kPolicyFit) good = good && c.IsFit(l) && c.IsFit(r);
        if (c.Profile().policy == lk::kPolicyOneDrift) good = good && (c.IsFit(l) || c.IsFit(r));
        if (!good) { CHECK(good); Dump(c, "policy"); break; }
      }
    }
  return ok;
}

// Degrees within [lo, hi + kOctaveHeadroom] under heavy variation; on a fresh
// theme within [lo, hi] and L even / R odd.
TEST(degree_range_and_ownership) {
  bool ok = true;
  for (uint8_t part = 0; part < lk::kPartCount; ++part)
    for (uint16_t seed = 1; seed < 40; ++seed) {
      LikembeCore c = Make(part, seed);
      for (int th = 0; th < 2; ++th)
        for (int j = 0; j < c.thumb[th].n; ++j) {
          int d = c.thumb[th].e[j].degree;
          CHECK(d >= c.key_lo && d <= c.key_hi);
          CHECK((d & 1) == th);
        }
      c.params.variation_pct = 100;
      for (int bar = 0; bar < 50; ++bar) {
        RunBar(c);
        for (int th = 0; th < 2; ++th)
          for (int j = 0; j < c.thumb[th].n; ++j) {
            int d = c.thumb[th].e[j].degree;
            if (d < c.key_lo || d > c.key_hi + lk::kOctaveHeadroom) { CHECK(false); Dump(c, "range"); }
          }
      }
    }
  return ok;
}

TEST(element0_never_rests) {
  bool ok = true;
  for (uint8_t part = 0; part < lk::kPartCount; ++part)
    for (uint16_t seed = 1; seed < 60; ++seed) {
      LikembeCore c = Make(part, seed);
      c.params.density_pct = 0; c.params.variation_pct = 100;
      for (int bar = 0; bar < 20; ++bar) {
        for (int i = 0; i < c.pulses_per_bar; ++i) {
          LikembeEvent e = c.Pulse();
          if (e.accent && !e.note) { CHECK(false); Dump(c, "el0"); }
        }
      }
    }
  return ok;
}

TEST(alternation_and_offset) {
  bool ok = true;
  for (uint8_t part = 0; part < lk::kPartCount; ++part) {
    LikembeCore c = Make(part, 9);
    c.params.density_pct = 100;
    int off = c.Profile().offset;
    LikembeEvent ev[16];
    RunBar(c, ev);
    for (int i = 0; i < 16; ++i) {
      int j = i - off;
      if (j < 0) { CHECK(!ev[i].note); continue; }
      CHECK(ev[i].thumb == (j & 1));
      CHECK(ev[i].note);   // 100 % density: no rests except offset
    }
  }
  return ok;
}

TEST(phase_advance_per_bar) {
  bool ok = true;
  for (uint8_t g : kTestLens) {
    LikembeCore c = Make(lk::kPartBass, 5, g);
    c.params.variation_pct = 0;
    for (int bar = 0; bar < 20; ++bar) {
      uint8_t p0 = c.thumb[0].phase, p1 = c.thumb[1].phase;
      RunBar(c);
      int s = c.pulses_per_bar / 2;
      CHECK(c.thumb[0].phase == (p0 + s) % c.thumb[0].n);
      CHECK(c.thumb[1].phase == (p1 + s) % c.thumb[1].n);
    }
  }
  return ok;
}

static int CountNotes(uint8_t part, uint8_t density, int bars) {
  int notes = 0;
  for (uint16_t seed = 1; seed <= 20; ++seed) {
    LikembeCore c = Make(part, seed);
    c.params.density_pct = density; c.params.variation_pct = 0;
    for (int i = 0; i < bars * c.pulses_per_bar; ++i) notes += c.Pulse().note;
  }
  return notes;
}

TEST(density_statistics) {
  bool ok = true;
  for (uint8_t part = 0; part < lk::kPartCount; ++part) {
    int lo = CountNotes(part, 0, 16), mid = CountNotes(part, 65, 16), hi = CountNotes(part, 100, 16);
    int off = lk::kParts[part].offset;
    CHECK(lo < mid && mid < hi);
    CHECK(hi == 20 * 16 * (16 - off));
  }
  // CV mod: effective density clamps, and lowers note count live.
  LikembeCore c = Make(lk::kPartLead, 3);
  c.SetDensityMod(-100); CHECK(c.EffectiveDensity() == 0);
  c.SetDensityMod(100); CHECK(c.EffectiveDensity() == lk::kDensityMax);
  return ok;
}

TEST(variation_rate_zero_static) {
  bool ok = true;
  LikembeCore c = Make(lk::kPartLead, 42);
  c.params.variation_pct = 0;
  custom::LikembeThumb t0 = c.thumb[0], t1 = c.thumb[1];
  for (int bar = 0; bar < 200; ++bar) RunBar(c);
  CHECK(LikembeCore::SameThumb(t0, c.thumb[0]) && LikembeCore::SameThumb(t1, c.thumb[1]));
  CHECK(!c.ThemeDiffers());
  return ok;
}

TEST(variation_rate_full_changes_and_returns) {
  bool ok = true;
  LikembeCore c = Make(lk::kPartLead, 42);
  c.params.variation_pct = 100;
  bool changed = false, returned = false;
  for (int bar = 0; bar < 200; ++bar) {
    bool before = c.ThemeDiffers();
    RunBar(c);
    if (c.ThemeDiffers()) changed = true;
    if (before && !c.ThemeDiffers()) {
      returned = true;
      CHECK(LikembeCore::SameFigures(c.thumb, c.theme));
      CHECK(c.thumb[0].phase < c.thumb[0].n && c.thumb[1].phase < c.thumb[1].n);
    }
  }
  CHECK(changed);
  CHECK(returned);
  return ok;
}

TEST(reroll_immediate) {
  bool ok = true;
  LikembeCore c = Make(lk::kPartBass, 100);
  c.params.variation_pct = 0;
  RunBar(c);
  for (int i = 0; i < 5; ++i) c.Pulse();
  uint16_t seed = c.params.seed;
  c.Reroll();   // TR2: applied now, pulse position kept, phases restart
  CHECK(c.params.seed != seed && c.pulse_idx == 5);
  CHECK(c.bar_in_passage == 0 && c.thumb[0].phase == 0 && c.thumb[1].phase == 0 && !c.ThemeDiffers());
  CHECK(c.cycle_bar_idx == 1);
  for (int i = 5; i < 16; ++i) c.Pulse();
  CHECK(c.pulse_idx == 0 && c.bar_in_passage == 1);
  // RerollNow is the same path
  for (int i = 0; i < 3; ++i) c.Pulse();
  seed = c.params.seed; c.RerollNow();
  CHECK(c.params.seed != seed && c.pulse_idx == 3);
  // Reset keeps figures, zeroes position
  custom::LikembeThumb t1 = c.thumb[1];
  RunBar(c); c.Reset();
  CHECK(c.pulse_idx == 0 && c.thumb[0].phase == 0 && c.thumb[1].phase == 0 && c.bar_in_passage == 0);
  CHECK(t1.n == c.thumb[1].n);
  return ok;
}

TEST(passage_length) {
  bool ok = true;
  LikembeCore c = Make(lk::kPartLead, 8);
  c.params.passage_idx = 8;  // 8 bars
  c.params.variation_pct = 100;
  for (int bar = 1; bar <= 8; ++bar) RunBar(c);
  CHECK(c.bar_in_passage == 0 && !c.ThemeDiffers());
  // Off: counter keeps running, no new passage
  c.params.passage_idx = 0; c.params.variation_pct = 0;
  for (int bar = 1; bar <= 70; ++bar) RunBar(c);
  CHECK(c.bar_in_passage == 70);
  return ok;
}

TEST(params_live_immediate) {
  bool ok = true;
  LikembeCore c = Make(lk::kPartBass, 8);
  c.params.passage_idx = 0;
  RunBar(c); RunBar(c);
  for (int i = 0; i < 14; ++i) c.Pulse();
  CHECK(c.bar_in_passage == 2 && c.pulse_idx == 14);
  // Len change: regenerated now, pulse position wrapped to the new Len, passage counter kept
  LikembeParams p = c.params; p.len = 12;
  c.SetParams(p);
  CHECK(c.pulses_per_bar == 12 && c.pulse_idx == 14 % 12 && c.bar_in_passage == 2);
  CHECK(c.thumb[0].n == lk::kParts[0].loops12[0] && c.thumb[1].n == lk::kParts[0].loops12[1]);
  CHECK(c.thumb[0].phase == 0 && c.thumb[1].phase == 0 && c.cycle_bar_idx == 1);
  // Len back to 16 at pulse 2: position kept
  p = c.params; p.len = 16; c.SetParams(p);
  CHECK(c.pulses_per_bar == 16 && c.pulse_idx == 2);
  while (c.pulse_idx != 0) c.Pulse();
  CHECK(c.bar_in_passage == 3);
  // seed change: regenerated now, same figures as a fresh core with that seed
  p = c.params; p.seed = 4321; c.SetParams(p);
  LikembeCore f = Make(lk::kPartBass, 4321);
  CHECK(LikembeCore::SameFigures(c.thumb, f.thumb) && c.bar_in_passage == 3);
  // root: immediate pitch change, no regeneration
  int16_t u = c.EventAt(0).pitch_units;
  for (int i = 0; i < 3; ++i) c.Pulse();
  custom::LikembeThumb t0 = c.thumb[0];
  p = c.params; p.root = 5; c.SetParams(p);
  CHECK(c.EventAt(0).pitch_units == u + 128 && c.pulse_idx == 3 && c.thumb[0].phase == t0.phase);
  // part change: offset/range/figures switch to the new part now
  p = c.params; p.part = lk::kPartLead; c.SetParams(p);
  CHECK(c.Profile().offset == lk::kParts[lk::kPartLead].offset && !c.EventAt(0).note && c.pulse_idx == 3);
  return ok;
}

TEST(pitch_mapping_and_range) {
  bool ok = true;
  // Root E, bass: base = E2 (MIDI 40) -> degree 0 = 4 semitones above C2 = 512.
  LikembeCore c = Make(lk::kPartBass, 1);
  CHECK(c.base_midi == 40 && c.PitchUnits(0) == 512);
  CHECK(c.PitchUnits(5) == 512 + 1536);
  // Root A, bass -> A2 = 1152; lead -> A3 = 2688.
  LikembeParams p = LikembeParams::Defaults(lk::kPartBass); p.root = 9; c.Init(p);
  CHECK(c.PitchUnits(0) == 1152);
  p = LikembeParams::Defaults(lk::kPartLead); p.root = 9; c.Init(p);
  CHECK(c.PitchUnits(0) == 2688);
  // Oct shifts by 1536 units per octave; big offsets simply clamp to 0..kMaxPitchUnits.
  p = LikembeParams::Defaults(lk::kPartBass); p.root = 9; c.Init(p);
  for (int o = -1; o <= 3; ++o) {
    p.octave = (int8_t)o; c.SetParams(p);
    int want = 1152 + 1536 * o;
    if (want < 0) want = 0;
    if (want > lk::kMaxPitchUnits) want = lk::kMaxPitchUnits;
    CHECK(c.PitchUnits(0) == want);
  }
  p.octave = -2; c.SetParams(p); CHECK(c.PitchUnits(0) == 0);     // A2 - 2 oct < C2 -> 0
  p = LikembeParams::Defaults(lk::kPartSolo); p.octave = 3; c.Init(p);
  CHECK(c.PitchUnits(c.key_hi) == lk::kMaxPitchUnits);              // clamps at 6 V
  p.octave = 0;
  // Every part, every root, Oct 0: lowest and highest reachable degree inside 0..6 V unclamped.
  for (uint8_t part = 0; part < lk::kPartCount; ++part)
    for (uint8_t root = 0; root < 12; ++root) {
      p = LikembeParams::Defaults(part); p.root = root; c.Init(p);
      int hi = c.key_hi + lk::kOctaveHeadroom;
      const lk::Tuning& tu = c.TuningRef();
      int midi_hi = c.base_midi + 12 * (hi / tu.n) + tu.off[hi % tu.n];
      int midi_lo = c.base_midi + 12 * (c.key_lo / tu.n) + tu.off[c.key_lo % tu.n];
      CHECK((midi_hi - lk::kMidiAtZeroUnits) * 128 <= lk::kMaxPitchUnits);
      CHECK(midi_lo >= lk::kMidiAtZeroUnits);
    }
  return ok;
}

TEST(pack_unpack) {
  bool ok = true;
  LikembeParams p; p.density_pct = 77; p.part = 3; p.root = 11; p.octave = -2; p.len = 27;
  p.variation_pct = 100; p.passage_idx = 8; p.seed = 0xBEEF;
  LikembeCore a; a.Init(p);
  uint64_t d = a.Pack();
  CHECK(d >> LikembeCore::kPackBits == 0 && ((d >> 13) & 0x1F) == 26 && ((d >> 29) & 0x7) == 0 && ((d >> 32) & 0xFFFF) == 0xBEEF);
  LikembeCore b; b.Unpack(d);
  CHECK(std::memcmp(&a.params, &b.params, sizeof p) == 0);
  CHECK(LikembeCore::SameFigures(a.thumb, b.thumb));
  // glue bits 48..63 do not disturb the core fields
  {
    LikembeParams q = LikembeCore::UnpackParams(d | (~0ull << 48));
    CHECK(std::memcmp(&q, &a.params, sizeof q) == 0);
  }
  // garbage
  LikembeParams g = LikembeCore::UnpackParams(~0ull);
  CHECK(g.density_pct == 100 && g.octave == lk::kOctaveMax && g.root == 11 && g.variation_pct == 100 && g.passage_idx == lk::kPassageCount - 1
        && g.len == lk::kLenMax);
  g = LikembeCore::UnpackParams(0);
  CHECK(g.density_pct == 0 && g.octave == lk::kOctaveMin && g.len == lk::kLenMin);
  for (int o = lk::kOctaveMin; o <= lk::kOctaveMax; ++o) {
    p.octave = (int8_t)o; a.Init(p);
    CHECK(LikembeCore::UnpackParams(a.Pack()).octave == o);
  }
  // Clamp() on garbage octave values
  p.octave = 100; p.Clamp(); CHECK(p.octave == lk::kOctaveMax);
  p.octave = -100; p.Clamp(); CHECK(p.octave == lk::kOctaveMin);
  LikembeCore z; z.Unpack(0xDEADBEEFCAFEF00Dull);
  CHECK(z.thumb[0].n >= 2 && z.thumb[0].n != z.thumb[1].n);
  std::printf("  sizeof(LikembeCore) = %zu\n", sizeof(LikembeCore));
  CHECK(sizeof(LikembeCore) < 200);
  return ok;
}

// Clock page settings: pack into bits 48..60 only, round trip of every field
// value, clamp garbage, same defaults on both sides; TRn Div list; Int multiplier
// list stepping (snap from off-list values) and ClockSetup notation.
TEST(clock_pack_unpack) {
  bool ok = true;
  using custom::LikembeClock;
  LikembeClock d = LikembeClock::Defaults();
  CHECK(d.src == lk::kSrcInt && d.div_tr_idx == 0 && d.TrDiv() == 1 && d.roll_in == lk::kRollTr2 && d.rst_in == lk::kRstOff);
  d.StepTrDiv(-1); CHECK(d.TrDiv() == 1);
  for (int i = 0; i < 20; ++i) d.StepTrDiv(1);
  CHECK(d.TrDiv() == 16 && d.div_tr_idx == lk::kTrDivCount - 1);
  d.StepTrDiv(-1); CHECK(d.TrDiv() == 12);
  for (int s = 0; s < lk::kSrcCount; ++s)
    for (int r = 0; r < lk::kRollCount; ++r)
      for (int t = 0; t < lk::kRstCount; ++t)
        for (int v = 0; v < lk::kTrDivCount; ++v) {
          LikembeClock c{(uint8_t)s, (uint8_t)v, (uint8_t)r, (uint8_t)t};
          const uint64_t p = c.Pack();
          CHECK((p & ((1ull << 48) - 1)) == 0 && (p >> 61) == 0);
          LikembeClock u = LikembeClock::Unpack(p | 0xFFFFFFFFFFFFull);
          CHECK(u.src == c.src && u.div_tr_idx == c.div_tr_idx && u.roll_in == c.roll_in && u.rst_in == c.rst_in);
        }
  LikembeClock g = LikembeClock::Unpack(~0ull);   // all fields out of range -> defaults
  CHECK(g.src == lk::kSrcInt && g.roll_in == lk::kRollTr2 && g.rst_in == lk::kRstOff && g.div_tr_idx == lk::kDefaultTrDivIdx);
  g = LikembeClock::Unpack(0);
  CHECK(g.src == lk::kSrcInt && g.div_tr_idx == 0 && g.roll_in == lk::kRollOff && g.rst_in == lk::kRstOff);
  // core + clock share one uint64 without overlap
  LikembeCore a = Make(lk::kPartLead, 77);
  LikembeClock k{lk::kSrcTr2, 7, lk::kRollTr1, lk::kRstTr3};
  uint64_t all = a.Pack() | k.Pack();
  LikembeParams q = LikembeCore::UnpackParams(all);
  CHECK(std::memcmp(&q, &a.params, sizeof q) == 0);
  LikembeClock k2 = LikembeClock::Unpack(all);
  CHECK(k2.src == k.src && k2.div_tr_idx == 7 && k2.roll_in == k.roll_in && k2.rst_in == k.rst_in);
  // Int multiplier list: /4 /3 /2 x1 x2 x3 x4 x5 x6 x7 x8 x12
  CHECK(custom::StepIntMult(4, 1) == 5 && custom::StepIntMult(4, -1) == 3);
  CHECK(custom::StepIntMult(1, -1) == -1 && custom::StepIntMult(-1, -1) == -2);
  CHECK(custom::StepIntMult(-3, -1) == -3 && custom::StepIntMult(12, 1) == 12);
  CHECK(custom::StepIntMult(9, 1) == 12 && custom::StepIntMult(9, -1) == 8);     // off-list snaps
  CHECK(custom::StepIntMult(0, 1) == 1 && custom::StepIntMult(0, -1) == -1);
  CHECK(custom::StepIntMult(-31, 1) == -3 && custom::StepIntMult(24, -1) == 12);
  char b[6];
  CHECK(std::strcmp(custom::FormatMult(4, b), "x4") == 0 && std::strcmp(custom::FormatMult(-1, b), "/2") == 0);
  CHECK(std::strcmp(custom::FormatMult(12, b), "x12") == 0 && std::strcmp(custom::FormatMult(-31, b), "/32") == 0);
  CHECK(std::strcmp(custom::FormatMult(0, b), "off") == 0);
  CHECK(std::strcmp(lk::MultNoteName(4), "1/16") == 0 && std::strcmp(lk::MultNoteName(-3), "1/1") == 0);
  CHECK(lk::MultNoteName(5) == nullptr && lk::MultNoteName(9) == nullptr);
  return ok;
}

// Divider: fires on the first pulse, then every Div pulses; Reset re-arms;
// lowering Div below the remaining count fires on the next pulse.
TEST(pulse_divider) {
  bool ok = true;
  custom::PulseDivider d;
  const char* want3 = "100100100";
  for (int i = 0; i < 9; ++i) CHECK(d.Tick(3) == (want3[i] == '1'));
  for (int i = 0; i < 5; ++i) CHECK(d.Tick(1));
  d.Reset(); CHECK(d.Tick(8)); CHECK(!d.Tick(8)); CHECK(!d.Tick(8));   // 5 left
  CHECK(d.Tick(3));                                                    // 5 > 3: fire now
  CHECK(!d.Tick(3)); CHECK(!d.Tick(3)); CHECK(d.Tick(3));
  d.Reset(); CHECK(d.Tick(24));
  int n = 0; for (int i = 0; i < 23; ++i) n += d.Tick(24);
  CHECK(n == 0 && d.Tick(24));
  return ok;
}

// Bar counter: N = lcm(2nL, 2nR, B) / B, idx 1..N wraps, phases realign after N
// bars, restarts on Reset(), regeneration and loop-length mutation.
TEST(cycle_bars_and_index) {
  bool ok = true;
  CHECK(LikembeCore::CycleBars(4, 3, 16) == 3);
  CHECK(LikembeCore::CycleBars(3, 4, 12) == 2);
  CHECK(LikembeCore::CycleBars(8, 4, 16) == 1);
  CHECK(LikembeCore::CycleBars(6, 3, 12) == 1);
  LikembeCore b = Make(lk::kPartBass, 1);
  CHECK(b.thumb[0].n == 4 && b.thumb[1].n == 3);
  CHECK(b.cycle_bars == 3 && b.cycle_bar_idx == 1);
  LikembeCore b12 = Make(lk::kPartBass, 1, 12);
  CHECK(b12.cycle_bars == 2);
  LikembeCore l = Make(lk::kPartLead, 1);
  CHECK(l.cycle_bars == 1 && l.cycle_bar_idx == 1);
  RunBar(l); CHECK(l.cycle_bar_idx == 1);
  b.params.variation_pct = 0;
  const uint8_t want[] = {2, 3, 1, 2, 3, 1};
  for (int k = 0; k < 6; ++k) {
    RunBar(b); CHECK(b.cycle_bar_idx == want[k]);
    if (b.cycle_bar_idx == 1) CHECK(b.thumb[0].phase == 0 && b.thumb[1].phase == 0);
  }
  RunBar(b); CHECK(b.cycle_bar_idx == 2);
  b.Reset(); CHECK(b.cycle_bar_idx == 1);
  RunBar(b); CHECK(b.cycle_bar_idx == 2);
  LikembeParams p = b.params; p.len = 12; b.SetParams(p);
  CHECK(b.cycle_bar_idx == 1 && b.cycle_bars == 2);
  RunBar(b); CHECK(b.cycle_bar_idx == 2);
  b.Reroll(); CHECK(b.cycle_bar_idx == 1);
  CHECK(b.cycle_bars == LikembeCore::CycleBars(b.thumb[0].n, b.thumb[1].n, b.pulses_per_bar));
  // invariant under full variation: N matches current lengths, idx in 1..N
  LikembeCore v = Make(lk::kPartSolo, 9);
  v.params.variation_pct = 100;
  for (int bar = 0; bar < 500; ++bar) {
    uint8_t nl = v.thumb[0].n, nr = v.thumb[1].n;
    uint8_t idx = v.cycle_bar_idx;
    RunBar(v);
    bool good = v.cycle_bars == LikembeCore::CycleBars(v.thumb[0].n, v.thumb[1].n, v.pulses_per_bar)
             && v.cycle_bar_idx >= 1 && v.cycle_bar_idx <= v.cycle_bars;
    if (nl != v.thumb[0].n || nr != v.thumb[1].n) good = good && v.cycle_bar_idx == 1;
    else good = good && v.cycle_bar_idx == (idx % v.cycle_bars) + 1;
    if (!good) { CHECK(good); Dump(v, "cycle"); break; }
  }
  return ok;
}

// Free Len: every Len 2..32 x part runs, stays in range, cycle_bars matches,
// fit-policy lengths divide the bar when at least two lengths fit.
TEST(len_sweep) {
  bool ok = true;
  for (uint8_t len = lk::kLenMin; len <= lk::kLenMax; ++len)
    for (uint8_t part = 0; part < lk::kPartCount; ++part) {
      LikembeCore c = Make(part, (uint16_t)(100 + len), len);
      c.params.variation_pct = 100; c.params.passage_idx = 1;
      CHECK(c.pulses_per_bar == len);
      if (len == 16) CHECK(c.thumb[0].n == lk::kParts[part].loops16[0] && c.thumb[1].n == lk::kParts[part].loops16[1]);
      for (int bar = 0; bar < 40; ++bar) {
        RunBar(c);
        bool good = c.pulse_idx == 0 && c.cycle_bars == LikembeCore::CycleBars(c.thumb[0].n, c.thumb[1].n, len)
                 && c.cycle_bar_idx >= 1 && c.cycle_bar_idx <= c.cycle_bars;
        for (int th = 0; th < 2; ++th)
          good = good && c.thumb[th].n >= lk::kMinLoop && c.thumb[th].n <= lk::kMaxLoop && c.thumb[th].phase < c.thumb[th].n;
        uint16_t fit = 0;
        for (uint8_t n = lk::kMinLoop; n <= lk::kMaxLoop; ++n) if (len % (2 * n) == 0) fit |= (uint16_t)(1u << n);
        if (lk::kParts[part].policy == lk::kPolicyFit && (fit & (fit - 1)))
          good = good && (fit & (1u << c.thumb[0].n)) && (fit & (1u << c.thumb[1].n));
        if (!good) { CHECK(good); std::printf("  len %d part %d bar %d\n", len, part, bar); Dump(c, "len"); break; }
      }
    }
  return ok;
}

int main() {
  RUN(determinism);
  RUN(loop_policies_and_unequal);
  RUN(degree_range_and_ownership);
  RUN(element0_never_rests);
  RUN(alternation_and_offset);
  RUN(phase_advance_per_bar);
  RUN(density_statistics);
  RUN(variation_rate_zero_static);
  RUN(variation_rate_full_changes_and_returns);
  RUN(reroll_immediate);
  RUN(passage_length);
  RUN(params_live_immediate);
  RUN(pitch_mapping_and_range);
  RUN(pack_unpack);
  RUN(clock_pack_unpack);
  RUN(pulse_divider);
  RUN(cycle_bars_and_index);
  RUN(len_sweep);
  { LikembeCore c = Make(lk::kPartLead, 1); Dump(c, "example lead"); }
  return g_fail ? 1 : 0;
}
