// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// likembe_core.h -- pure pattern logic for the Likembe applet (one part per
// instance). Port of the pattern engine in docs/research/Congotronics line
// generator.html (JS reference) / "Likembe pattern engine technical concept".
// Integer only, no allocation, no STL. Advanced by Pulse() (one call per engine
// pulse); variation and passage changes happen once per bar line inside the
// Pulse() that ends the bar; setting changes and re-rolls apply immediately. See docs/modules/likembe.md.
#pragma once
#include <cstdint>
#include "likembe_constants.h"

namespace custom {

struct LikembeParams {
  uint8_t density_pct;    // 0..100
  uint8_t part;           // likembe::Part
  uint8_t root;           // pitch class 0..11 (lowest key)
  int8_t octave;          // octave offset kOctaveMin..kOctaveMax
  uint8_t len;            // pulses per bar kLenMin..kLenMax
  uint8_t variation_pct;  // 0..100 = 0..1 mutations per bar
  uint8_t passage_idx;    // index into kPassageBars (0 = Off)
  uint16_t seed;

  static LikembeParams Defaults(uint8_t part = likembe::kPartBass) {
    using namespace likembe;
    if (part >= kPartCount) part = kPartBass;
    LikembeParams p;
    p.density_pct = kParts[part].density_pct;
    p.part = part;
    p.root = kDefaultRoot;
    p.octave = kDefaultOctave;
    p.len = kDefaultLen;
    p.variation_pct = kParts[part].variation_pct;
    p.passage_idx = kDefaultPassageIdx;
    p.seed = kDefaultSeed;
    return p;
  }

  void Clamp() {
    using namespace likembe;
    if (density_pct > kDensityMax) density_pct = kDensityMax;
    if (part >= kPartCount) part = kPartBass;
    if (root > 11) root = 11;
    if (octave < kOctaveMin) octave = kOctaveMin;
    if (octave > kOctaveMax) octave = kOctaveMax;
    if (len < kLenMin) len = kLenMin;
    if (len > kLenMax) len = kLenMax;
    if (variation_pct > kVariationMax) variation_pct = kVariationMax;
    if (passage_idx >= kPassageCount) passage_idx = kPassageCount - 1;
  }
};

struct LikembeEvent {
  bool note;            // false = rest (glue keeps the previous pitch)
  uint8_t degree;       // scale degree of the element (0 on offset pulses)
  int16_t pitch_units;  // O_C pitch CV, 0 = C2, clamped 0..kMaxPitchUnits
  bool accent;          // bar head (rhythm) or first element of the thumb's figure
  uint8_t thumb;        // 0 = L, 1 = R
};

struct LikembeElement {
  int8_t degree;        // pitch only; rests live in the rhythm layer (LikembeCore::rhythm)
};

struct LikembeThumb {
  uint8_t n;            // loop length in strokes (kMinLoop..kMaxLoop)
  uint8_t phase;        // index of the element played on the bar's first stroke
  LikembeElement e[likembe::kMaxLoop];
};

struct LikembeCore {
  // ---------------- state (read-only for the glue / display) ----------------
  LikembeParams params;
  int8_t density_mod = 0;     // CV1 offset in percent
  uint8_t pulses_per_bar = 16;
  uint8_t pulse_idx = 0;      // next pulse to be played (0..pulses_per_bar-1)
  uint8_t bar_in_passage = 0; // bars completed in the current passage (saturates 255)
  LikembeThumb thumb[2];      // current figures (0 = L, 1 = R)
  LikembeThumb theme[2];      // figures as generated at passage start
  // Rhythm layer: one rest draw 0..255 per bar position (all kLenMax filled, so
  // a Len change needs none). Position RhythmHead() always plays (bar accent);
  // others rest iff min(255, draw + bias) * 100 >= density * 256 (RestAt).
  uint8_t rhythm[likembe::kLenMax] = {};
  uint8_t theme_rhythm[likembe::kLenMax] = {};
  uint32_t rng_part = 1, rng_passage = 1;
  int16_t base_midi = 45;     // MIDI of degree 0
  uint8_t key_lo = 0, key_hi = 8; // part degree range
  uint8_t active_part = 0;    // part the current figures were generated for
  // Realignment cycle: both thumbs and the bar line coincide again after
  // cycle_bars = lcm(2*nL, 2*nR, pulses_per_bar) / pulses_per_bar bars (the
  // part's start offset shifts both thumbs equally, so it does not change N).
  // cycle_bar_idx = bar playing now, 1..cycle_bars. Restarts at 1 on Reset(),
  // whenever figures are (re)generated (GenPart: Init, Len/part/seed change,
  // re-roll, new passage -- phases restart at 0) and when a mutation changes a
  // loop length; otherwise advances at every bar line and wraps N -> 1.
  uint8_t cycle_bars = 1;
  uint8_t cycle_bar_idx = 1;
  uint8_t cycle_n[2] = {0, 0}; // loop lengths cycle_bars was computed for

  // ---------------- API ----------------
  void Init(const LikembeParams& p) {
    params = p; params.Clamp();
    Regenerate(false);
  }

  // Live changes. Density, variation, passage length: immediate. Root, octave: pitch
  // mapping immediate (figures are degrees). Len, part, seed: full
  // regeneration from the seed, immediately; the bar position (pulse_idx,
  // wrapped to the new Len) and bar_in_passage are kept, phases restart at 0.
  void SetParams(const LikembeParams& p) {
    LikembeParams np = p; np.Clamp();
    bool regen = np.len != params.len || np.part != params.part || np.seed != params.seed;
    params = np;
    if (regen) Regenerate(true); else UpdatePitchBase();
  }

  void SetDensityMod(int8_t pct_delta) { density_mod = pct_delta; }

  uint8_t EffectiveDensity() const {
    int d = (int)params.density_pct + density_mod;
    if (d < 0) d = 0;
    if (d > likembe::kDensityMax) d = likembe::kDensityMax;
    return (uint8_t)d;
  }

  // Advance one engine pulse and return its event.
  LikembeEvent Pulse() {
    LikembeEvent ev = EventAt(pulse_idx);
    if (++pulse_idx >= pulses_per_bar) { pulse_idx = 0; BarLine(); }
    return ev;
  }

  // Event for pulse i of the current bar (no state change). Used by Pulse()
  // and by the display to draw the bar.
  LikembeEvent EventAt(uint8_t i) const {
    LikembeEvent ev = {false, 0, 0, false, 0};
    int j = (int)i - Profile().offset;
    if (j < 0) { ev.pitch_units = PitchUnits(0); return ev; }
    ev.thumb = (uint8_t)(j & 1);
    const LikembeThumb& t = thumb[ev.thumb];
    uint8_t idx = ElementIndex(ev.thumb, (uint8_t)(j >> 1));
    const LikembeElement& el = t.e[idx];
    ev.degree = (uint8_t)el.degree;
    ev.pitch_units = PitchUnits(el.degree);
    ev.accent = i == RhythmHead() || idx == 0;
    ev.note = !RestAt(i);
    return ev;
  }

  // Rhythm layer. Head = first played position of the bar (the part offset);
  // positions before it are offset pulses (silent).
  uint8_t RhythmHead() const { return Profile().offset; }
  bool RestAt(uint8_t i) const { return RestAt(i, EffectiveDensity()); }
  bool RestAt(uint8_t i, uint8_t density) const {
    uint8_t h = RhythmHead();
    if (i < h) return true;
    if (i == h) return false;
    uint32_t bias = i + 1 >= pulses_per_bar ? likembe::kRestBiasLast : likembe::kRestBiasOther;
    uint32_t v = rhythm[i] + bias;
    if (v > 255u) v = 255u;
    return v * 100u >= (uint32_t)density * 256u;
  }

  uint8_t ElementIndex(uint8_t th, uint8_t stroke) const {
    return (uint8_t)((thumb[th].phase + stroke) % thumb[th].n);
  }

  // Element the given thumb plays next (for the display cursor).
  uint8_t CurrentElement(uint8_t th) const {
    int j = (int)pulse_idx - Profile().offset;
    if (j < 0) return thumb[th].phase;
    int strokes = th ? (j >> 1) : ((j + 1) >> 1);  // strokes of this thumb already played this bar
    return ElementIndex(th, (uint8_t)strokes);
  }

  // New seed + new passage (forced new loop lengths, new theme), immediately
  // (pulse position kept, phases restart at 0, passage counter restarts).
  void Reroll() { DoReroll(); }
  void RerollNow() { DoReroll(); }
  // Bar position and phases to zero; figures kept.
  void Reset() {
    pulse_idx = 0; bar_in_passage = 0;
    thumb[0].phase = thumb[1].phase = 0;
    theme[0].phase = theme[1].phase = 0;
    SyncCycle(true);
  }

  static uint32_t Gcd(uint32_t a, uint32_t b) { while (b) { uint32_t t = a % b; a = b; b = t; } return a; }
  static uint32_t Lcm(uint32_t a, uint32_t b) { return a / Gcd(a, b) * b; }
  // Bars until both thumbs realign with the bar line.
  static uint32_t CycleBars(uint8_t nl, uint8_t nr, uint8_t ppb) {
    return Lcm(Lcm(2u * nl, 2u * nr), ppb) / ppb;
  }
  // Called once per bar line (after the bar's work) and on (re)generation / Reset.
  void SyncCycle(bool restart) {
    if (restart || thumb[0].n != cycle_n[0] || thumb[1].n != cycle_n[1]) {
      cycle_n[0] = thumb[0].n; cycle_n[1] = thumb[1].n;
      uint32_t n = CycleBars(thumb[0].n, thumb[1].n, pulses_per_bar);
      cycle_bars = (uint8_t)(n > 255u ? 255u : n);
      cycle_bar_idx = 1;
    } else if (++cycle_bar_idx > cycle_bars) {
      cycle_bar_idx = 1;
    }
  }

  bool ThemeDiffers() const {
    if (!SameFigures(thumb, theme)) return true;
    for (uint8_t i = 0; i < likembe::kLenMax; ++i) if (rhythm[i] != theme_rhythm[i]) return true;
    return false;
  }

  // Persistence (settings only). Bit layout, LSB first (bits 48..63 are left
  // to the glue: Likembe.h packs its clock page there, custom/core/likembe_clock.h):
  //  0..6 density_pct (7)  7..8 part (2)  9..12 root (4)
  // 13..17 len - kLenMin (5; 31 -> Len 33 clamps to 32)  18..24 variation_pct (7)
  // 25..28 passage_idx (4)  29..31 octave - kOctaveMin (3; 6, 7 clamp to +3)  32..47 seed (16)
  static constexpr int kPackBits = 48;
  uint64_t Pack() const {
    uint64_t d = 0;
    d |= (uint64_t)(params.density_pct & 0x7F);
    d |= (uint64_t)(params.part & 0x3) << 7;
    d |= (uint64_t)(params.root & 0xF) << 9;
    d |= (uint64_t)((params.len - likembe::kLenMin) & 0x1F) << 13;
    d |= (uint64_t)(params.variation_pct & 0x7F) << 18;
    d |= (uint64_t)(params.passage_idx & 0xF) << 25;
    d |= (uint64_t)((params.octave - likembe::kOctaveMin) & 0x7) << 29;
    d |= (uint64_t)params.seed << 32;
    return d;
  }
  static LikembeParams UnpackParams(uint64_t d) {
    LikembeParams p;
    p.density_pct = (uint8_t)(d & 0x7F);
    p.part = (uint8_t)((d >> 7) & 0x3);
    p.root = (uint8_t)((d >> 9) & 0xF);
    p.len = (uint8_t)(((d >> 13) & 0x1F) + likembe::kLenMin);
    p.variation_pct = (uint8_t)((d >> 18) & 0x7F);
    p.passage_idx = (uint8_t)((d >> 25) & 0xF);
    p.octave = (int8_t)(((d >> 29) & 0x7) + likembe::kOctaveMin);
    p.seed = (uint16_t)((d >> 32) & 0xFFFF);
    p.Clamp();
    return p;
  }
  void Unpack(uint64_t d) { Init(UnpackParams(d)); }

  const likembe::PartProfile& Profile() const { return likembe::kParts[active_part]; }
  const likembe::Tuning& TuningRef() const { return likembe::kTunings[0]; }

  int16_t PitchUnits(int degree) const {
    const likembe::Tuning& tu = TuningRef();
    int n = tu.n;
    int oct = degree >= 0 ? degree / n : -((-degree + n - 1) / n);
    int idx = degree - oct * n;
    int midi = base_midi + 12 * (oct + params.octave) + tu.off[idx];
    int u = (midi - likembe::kMidiAtZeroUnits) * likembe::kUnitsPerSemitone;
    if (u < 0) u = 0;
    if (u > likembe::kMaxPitchUnits) u = likembe::kMaxPitchUnits;
    return (int16_t)u;
  }

  // ---------------- internals ----------------
  static uint32_t SeedState(uint32_t s) {
    s *= likembe::kSeedMix; s ^= s >> 16;
    return s ? s : likembe::kSeedNonZero;
  }
  static uint32_t Next(uint32_t& s) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s;
  }
  static uint32_t Below(uint32_t& s, uint32_t n) {   // uniform-ish 0..n-1
    return ((Next(s) >> 16) * n) >> 16;
  }
  static bool Chance(uint32_t& s, uint32_t pct_x100) { // probability pct_x100 / 10000
    return (Next(s) >> 16) * 10000u < pct_x100 * 65536u;
  }
  static uint8_t PickW(uint32_t& s, const uint8_t* w, uint8_t count) {
    uint32_t tot = 0;
    for (uint8_t i = 0; i < count; ++i) tot += w[i];
    uint32_t r = Below(s, tot);
    for (uint8_t i = 0; i < count; ++i) { if (r < w[i]) return i; r -= w[i]; }
    return 0;
  }

  void UpdatePitchBase() {
    const likembe::PartProfile& pp = Profile();
    int k = (((int)params.root - pp.anchor_midi) % 12 + 12) % 12;
    if (k > 6) k -= 12;
    base_midi = (int16_t)(pp.anchor_midi + k);
    int n = TuningRef().n;
    key_lo = (uint8_t)((pp.lo10 * n + 5) / 10);
    key_hi = (uint8_t)((pp.hi10 * n + 5) / 10);
  }

  // Thumb keys: degrees in [key_lo, key_hi] with parity th. Returns count.
  uint8_t Keys(uint8_t th, int8_t* out) const {
    uint8_t c = 0;
    for (int d = key_lo; d <= key_hi; ++d) if ((d & 1) == th) out[c++] = (int8_t)d;
    return c;
  }
  static int KeyIndex(const int8_t* keys, uint8_t c, int8_t d) {
    for (uint8_t i = 0; i < c; ++i) if (keys[i] == d) return i;
    return -1;
  }
  static int ClampI(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

  bool IsFit(uint8_t n) const { return pulses_per_bar % (2 * n) == 0; }

  // Allowed lengths for a thumb given the other's length (JS allowedLoops), as a bit mask.
  uint16_t AllowedMask(uint8_t other) const {
    uint16_t all = AllMask(), fit = FitMask();
    switch (Profile().policy) {
      case likembe::kPolicyBothDrift: return all;
      case likembe::kPolicyOneDrift: return (fit & (1u << other)) ? all : fit;
      default: return fit;
    }
  }
  static uint8_t PickFromMask(uint32_t& s, uint16_t mask) {
    uint8_t c = 0, list[16];
    for (uint8_t n = 0; n < 16; ++n) if (mask & (1u << n)) list[c++] = n;
    return c ? list[Below(s, c)] : likembe::kMinLoop;
  }
  // Lengths that fit the bar (2n divides Len). Fewer than two fit (e.g. odd
  // Len) -> every length counts as fit, so the policies always have a choice.
  uint16_t FitMask() const {
    uint16_t f = 0;
    for (uint8_t n = likembe::kMinLoop; n <= likembe::kMaxLoop; ++n) if (IsFit(n)) f |= (uint16_t)(1u << n);
    return (f & (f - 1)) ? f : AllMask();
  }
  static uint16_t AllMask() {
    uint16_t a = 0;
    for (uint8_t n = likembe::kMinLoop; n <= likembe::kMaxLoop; ++n) a |= (uint16_t)(1u << n);
    return a;
  }

  void GenThumb(uint8_t th, uint8_t n) {
    int8_t keys[16]; uint8_t c = Keys(th, keys);
    int k = (th == 0 && !likembe::kThumbLStartMiddle) ? 0 : c / 2;
    LikembeThumb& t = thumb[th];
    t.n = n; t.phase = 0;
    bool all_equal = true;
    for (uint8_t j = 0; j < n; ++j) {
      if (j) k = ClampI(k + likembe::kStepDelta[PickW(rng_part, likembe::kStepWeight[active_part], 5)], 0, c - 1);
      t.e[j].degree = keys[k];
      if (keys[k] != t.e[0].degree) all_equal = false;
    }
    if (n > 1 && all_equal) t.e[n - 1].degree = keys[ClampI(k + 1, 0, c - 1)];
    for (uint8_t j = n; j < likembe::kMaxLoop; ++j) t.e[j] = {0};
  }

  // Draw order: thumb L degrees, thumb R degrees, then rhythm[1..kLenMax-1]
  // (one Next(rng_part) >> 24 each, ascending); rhythm[0] = 0 (no draw).
  void GenRhythm() {
    rhythm[0] = 0;
    for (uint8_t i = 1; i < likembe::kLenMax; ++i) rhythm[i] = (uint8_t)(Next(rng_part) >> 24);
  }

  void GenPart() {
    GenThumb(0, thumb[0].n);
    GenThumb(1, thumb[1].n);
    GenRhythm();
    theme[0] = thumb[0]; theme[1] = thumb[1];
    for (uint8_t i = 0; i < likembe::kLenMax; ++i) theme_rhythm[i] = rhythm[i];
    bar_in_passage = 0;
    SyncCycle(true);
  }

  // Everything from the seed (Init: keep_pos false; Len/part/seed change:
  // keep_pos true keeps pulse_idx (wrapped to the new Len) and bar_in_passage).
  void Regenerate(bool keep_pos) {
    using namespace likembe;
    uint8_t pi = pulse_idx, bip = bar_in_passage;
    pulses_per_bar = params.len;
    active_part = params.part;
    pulse_idx = keep_pos ? (uint8_t)(pi % pulses_per_bar) : 0;
    UpdatePitchBase();
    rng_part = SeedState((uint32_t)params.seed + kPartStreamOffset * params.part);
    rng_passage = SeedState((uint32_t)params.seed + kPassageStreamOffset);
    // Default loop lengths at Len 16 / 12; any other Len rolls them under the
    // part policy from the lengths allowed for that Len (passage stream).
    const uint8_t* d = params.len == 16 ? Profile().loops16 : (params.len == 12 ? Profile().loops12 : nullptr);
    if (d) { thumb[0].n = d[0]; thumb[1].n = d[1]; } else RollLoopLengths();
    GenPart();
    if (keep_pos) bar_in_passage = bip;
  }

  // New loop lengths under the part's policy (JS newPassage), passage stream.
  void RollLoopLengths() {
    uint16_t f = FitMask(), all = AllMask();
    uint8_t l, r;
    switch (Profile().policy) {
      case likembe::kPolicyFit:
        l = PickFromMask(rng_passage, f);
        r = PickFromMask(rng_passage, (uint16_t)(f & ~(1u << l)));
        break;
      case likembe::kPolicyOneDrift: {
        uint8_t a = PickFromMask(rng_passage, f);
        uint8_t b = PickFromMask(rng_passage, (uint16_t)(all & ~(1u << a)));
        if (Next(rng_passage) & 0x80000000u) { l = a; r = b; } else { l = b; r = a; }
        break;
      }
      default:
        l = PickFromMask(rng_passage, all);
        r = PickFromMask(rng_passage, (uint16_t)(all & ~(1u << l)));
    }
    thumb[0].n = l; thumb[1].n = r;
  }

  void NewPassage(bool force_loops) {
    if (force_loops || Chance(rng_passage, (uint32_t)likembe::kLoopChangePct * 100u)) RollLoopLengths();
    GenPart();
  }

  void DoReroll() {
    params.seed = (uint16_t)(Next(rng_passage) >> 16);
    rng_part = SeedState((uint32_t)params.seed + likembe::kPartStreamOffset * params.part);
    rng_passage = SeedState((uint32_t)params.seed + likembe::kPassageStreamOffset);
    NewPassage(true);
  }

  static bool SameThumb(const LikembeThumb& a, const LikembeThumb& b) {
    if (a.n != b.n) return false;
    for (uint8_t j = 0; j < a.n; ++j)
      if (a.e[j].degree != b.e[j].degree) return false;
    return true;
  }
  static bool SameFigures(const LikembeThumb* a, const LikembeThumb* b) {
    return SameThumb(a[0], b[0]) && SameThumb(a[1], b[1]);
  }

  void Mutate() {
    using namespace likembe;
    uint8_t th = (Next(rng_part) & 0x80000000u) ? 1 : 0;   // JS: rng() < .5 ? L : R
    LikembeThumb& t = thumb[th];
    int8_t keys[16]; uint8_t c = Keys(th, keys);
    int n = TuningRef().n;
    switch (PickW(rng_part, kMutWeight, 4)) {
      case kMutKey: {
        uint8_t j = (uint8_t)Below(rng_part, t.n);
        int i = KeyIndex(keys, c, t.e[j].degree);
        int ni = ClampI(i + ((Next(rng_part) & 0x80000000u) ? 1 : -1), 0, c - 1);
        if (ni == i) return;
        t.e[j].degree = keys[ni];
        return;
      }
      case kMutRest: {   // rhythm layer: toggle a bar position after the head (thumb draw unused)
        uint8_t h = RhythmHead();
        if (h + 1 >= pulses_per_bar) return;
        uint8_t i = (uint8_t)(h + 1 + Below(rng_part, pulses_per_bar - 1 - h));
        rhythm[i] = RestAt(i) ? 0 : 255;
        return;
      }
      case kMutOctave: {
        uint8_t j = (uint8_t)Below(rng_part, t.n);
        int d = t.e[j].degree;
        int nd = d + n <= key_hi + kOctaveHeadroom ? d + n : (d - n >= key_lo ? d - n : d);
        if (nd == d) return;
        t.e[j].degree = (int8_t)nd;
        return;
      }
      default: {   // loop length
        uint8_t other = thumb[th ^ 1].n;
        uint16_t f = (uint16_t)(AllowedMask(other) & ~(1u << t.n) & ~(1u << other));
        if (!f) return;
        uint16_t up = 0, dn = 0;
        for (uint8_t m = kMinLoop; m <= kMaxLoop; ++m) if (f & (1u << m)) { if (m > t.n) up |= (uint16_t)(1u << m); else dn |= (uint16_t)(1u << m); }
        uint8_t np = 0;
        // JS: (up.length && (!dn.length || rng() < .5)) -- rng only drawn if both exist
        bool use_up = up && (!dn || (Next(rng_part) & 0x80000000u));
        if (use_up) { for (uint8_t m = kMinLoop; m <= kMaxLoop; ++m) if (up & (1u << m)) { np = m; break; } }
        else { for (uint8_t m = kMaxLoop; m >= kMinLoop; --m) if (dn & (1u << m)) { np = m; break; } }
        while (t.n < np) {
          int i = KeyIndex(keys, c, t.e[t.n - 1].degree);
          int ni = ClampI(i + ((Next(rng_part) & 0x80000000u) ? 1 : -1), 0, c - 1);
          t.e[t.n].degree = keys[ni];
          ++t.n;
        }
        for (uint8_t m = np; m < kMaxLoop; ++m) t.e[m] = {0};
        t.n = np; t.phase = (uint8_t)(t.phase % np);
        return;
      }
    }
  }

  void Evolve() {
    uint32_t rate = params.variation_pct;  // percent
    if (Chance(rng_part, likembe::kReturnToThemePct * rate) && ThemeDiffers()) {
      uint8_t pl = thumb[0].phase, pr = thumb[1].phase;
      thumb[0] = theme[0]; thumb[1] = theme[1];
      for (uint8_t i = 0; i < likembe::kLenMax; ++i) rhythm[i] = theme_rhythm[i];
      thumb[0].phase = (uint8_t)(pl % thumb[0].n);
      thumb[1].phase = (uint8_t)(pr % thumb[1].n);
    }
    uint32_t m = rate / 100u;
    if (Chance(rng_part, (rate % 100u) * 100u)) ++m;
    for (uint32_t k = 0; k < m; ++k) Mutate();
  }

  void BarLine() {
    uint8_t strokes = (uint8_t)(pulses_per_bar / 2);
    for (uint8_t th = 0; th < 2; ++th) thumb[th].phase = (uint8_t)((thumb[th].phase + strokes) % thumb[th].n);
    if (bar_in_passage < 255) ++bar_in_passage;
    uint8_t len = likembe::kPassageBars[params.passage_idx];
    if (len && bar_in_passage >= len) { NewPassage(false); return; }
    Evolve();
    SyncCycle(false);
  }
};

}  // namespace custom
