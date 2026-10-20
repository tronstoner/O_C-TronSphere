// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// likembe_engine.js -- 1:1 JS port of custom/core/likembe_core.h (+ the values of
// custom/core/likembe_constants.h). The C++ is the reference; host/tools/parity.sh
// (task host:parity) checks that both produce the same event stream.
// All arithmetic is uint32 like the C++ (Math.imul / >>> 0). The "lab" object holds
// the firmware constants the browser lab exposes; labDefaults() = firmware values.
// Loads in the browser (window.LikembeEngine) and in node (module.exports).
(function (root) {
'use strict';

// ---- likembe_constants.h ----
const C = {
  kUnitsPerSemitone: 128, kMidiAtZeroUnits: 36, kMaxPitchUnits: 9216,
  kTunings: [[0, 2, 4, 7, 9]],
  kLenMin: 2, kLenMax: 32, kDefaultLen: 16, kMinLoop: 2, kMaxLoop: 8,
  kPolicyFit: 0, kPolicyOneDrift: 1, kPolicyBothDrift: 2,
  kPartNames: ['Bass', 'Lead', 'MiSol', 'Solo'],
  kParts: [
    {anchor_midi: 45, lo10: 0, hi10: 16, policy: 1, density_pct: 85, variation_pct: 8, offset: 0, loops16: [4, 3], loops12: [3, 4]},
    {anchor_midi: 57, lo10: 4, hi10: 20, policy: 0, density_pct: 65, variation_pct: 25, offset: 1, loops16: [8, 4], loops12: [6, 3]},
    {anchor_midi: 60, lo10: 6, hi10: 22, policy: 0, density_pct: 55, variation_pct: 30, offset: 1, loops16: [4, 8], loops12: [3, 6]},
    {anchor_midi: 64, lo10: 10, hi10: 26, policy: 1, density_pct: 45, variation_pct: 40, offset: 3, loops16: [8, 5], loops12: [6, 4]},
  ],
  kStepDelta: [1, -1, 0, 2, -2],
  kDensityMax: 100, kOctaveMin: -2, kOctaveMax: 3, kDefaultOctave: 0, kVariationMax: 100,
  kDensityCvRange: 35,
  kPassageBars: [0, 1, 2, 3, 4, 5, 6, 7, 8],
  // Clock page (glue): ClockSrc, kClockDiv*, RollIn, RstIn
  kSrcInt: 0, kSrcTr1: 1, kSrcCount: 5, kClockDivMin: 1, kClockDivMax: 24, kDefaultClockDiv: 1,
  kRollOff: 0, kRollTr1: 1, kRollCount: 5, kDefaultRollLeft: 2, kDefaultRollRight: 4,
  kRstOff: 0, kRstTr1: 1, kRstCount: 5, kDefaultRst: 0,
  kDefaultPulseTicks: 2083, kTickHz: 16666,   // gate length % = lab.gatePct (kGateLengthPct)
  kDefaultPassageIdx: 0, kDefaultRoot: 4, kDefaultSeed: 1,
  kPartStreamOffset: 7919, kPassageStreamOffset: 31, kSeedMix: 0x9E3779B1, kSeedNonZero: 0x6D2B79F5,
};

// Lab-exposed constants. Per part (index = Part); stepW mirrors kStepWeight[kPartCount][5],
// the others are one global in the firmware (export prints a global when all parts agree).
// lStartMiddle mirrors kThumbLStartMiddle (thumb L starts at c / 2 instead of key 0).
// loopCap: lab prototype, not in firmware -- each thumb loop n <= min(Len, kMaxLoop).
function labDefaults() {
  const stepW = [[30, 70, 12, 8, 8], [20, 70, 12, 5, 10], [30, 70, 12, 8, 8], [30, 70, 12, 8, 8]];
  const part = p => ({stepW: stepW[p].slice(), mutW: [30, 20, 6, 5],
    ret: 30, loopChange: 40, headroom: 2});
  return {parts: [part(0), part(1), part(2), part(3)], lStartMiddle: true, loopCap: false, accentV: 5, normalV: 3, gatePct: 50};
}
const cloneLab = l => JSON.parse(JSON.stringify(l));

const u32 = x => x >>> 0;
const mul = (a, b) => Math.imul(a, b) >>> 0;
function seedState(s) {
  s = mul(s, C.kSeedMix); s = u32(s ^ (s >>> 16));
  return s ? s : C.kSeedNonZero;
}
function next(r) {   // r = {s}
  let s = r.s;
  s = u32(s ^ (s << 13)); s = u32(s ^ (s >>> 17)); s = u32(s ^ (s << 5));
  r.s = s; return s;
}
const below = (r, n) => mul(next(r) >>> 16, n) >>> 16;
const chance = (r, p) => mul(next(r) >>> 16, 10000) < mul(p, 65536);
const hiBit = r => (next(r) >>> 31) === 1;
function pickW(r, w) {
  let tot = 0; for (const x of w) tot = u32(tot + x);
  let v = below(r, tot);
  for (let i = 0; i < w.length; ++i) { if (v < w[i]) return i; v -= w[i]; }
  return 0;
}
const clampI = (v, lo, hi) => v < lo ? lo : (v > hi ? hi : v);
const bit = n => (1 << n) >>> 0;

function defaults(part) {
  if (!(part >= 0 && part < 4)) part = 0;
  return {density_pct: C.kParts[part].density_pct, part, root: C.kDefaultRoot, octave: C.kDefaultOctave,
    len: C.kDefaultLen, variation_pct: C.kParts[part].variation_pct, passage_idx: C.kDefaultPassageIdx,
    seed: C.kDefaultSeed};
}
function clampParams(p) {
  p = Object.assign({}, p);
  if (p.density_pct > C.kDensityMax) p.density_pct = C.kDensityMax;
  if (p.part >= 4) p.part = 0;
  if (p.root > 11) p.root = 11;
  p.octave = clampI(p.octave, C.kOctaveMin, C.kOctaveMax);
  p.len = clampI(p.len | 0, C.kLenMin, C.kLenMax);
  if (p.variation_pct > C.kVariationMax) p.variation_pct = C.kVariationMax;
  if (p.passage_idx >= C.kPassageBars.length) p.passage_idx = C.kPassageBars.length - 1;
  p.seed = p.seed & 0xFFFF;
  return p;
}
const newThumb = () => ({n: 2, phase: 0, e: Array.from({length: 8}, () => ({degree: 0, draw: 0}))});
const copyThumb = t => ({n: t.n, phase: t.phase, e: t.e.map(x => ({degree: x.degree, draw: x.draw}))});

class Core {
  constructor(lab) {
    this.lab = cloneLab(lab || labDefaults());
    this.labPending = null;
    this.params = defaults(0);
    this.density_mod = 0;
    this.pulses_per_bar = 16; this.pulse_idx = 0; this.bar_in_passage = 0;
    this.thumb = [newThumb(), newThumb()]; this.theme = [newThumb(), newThumb()];
    this.rng_part = {s: 1}; this.rng_passage = {s: 1};
    this.base_midi = 45; this.key_lo = 0; this.key_hi = 8; this.active_part = 0;
    this.cycle_bars = 1; this.cycle_bar_idx = 1; this.cycle_n = [0, 0];
  }
  L() { return this.lab.parts[this.active_part]; }
  // Lab constants change at the next bar line (like a firmware rebuild would, minus the reset).
  setLab(lab) {
    this.labPending = cloneLab(lab);
    if (lab.loopCap && !this.lab.loopCap) { this.lab.loopCap = true; this.ApplyCap(); }   // cap on: truncate at once
  }
  Cap() { return this.lab.loopCap ? clampI(this.params.len, C.kMinLoop, C.kMaxLoop) : C.kMaxLoop; }
  AllMask() { let a = 0; for (let n = C.kMinLoop; n <= this.Cap(); ++n) a |= bit(n); return a >>> 0; }
  ApplyCap() {   // truncate live loops and theme copies to the cap, phase wrapped
    const cap = this.Cap();
    for (const t of [...this.thumb, ...this.theme]) if (t.n > cap) {
      for (let j = cap; j < C.kMaxLoop; ++j) t.e[j] = {degree: 0, draw: 0};
      t.n = cap; t.phase = t.phase % cap;
    }
  }

  Init(p) { this.params = clampParams(p); this.Regenerate(false); }
  SetParams(p) {
    const np = clampParams(p);
    const regen = np.len !== this.params.len || np.part !== this.params.part || np.seed !== this.params.seed;
    this.params = np;
    if (regen) this.Regenerate(true); else this.UpdatePitchBase();
  }
  SetDensityMod(d) { this.density_mod = d; }
  EffectiveDensity() { return clampI(this.params.density_pct + this.density_mod, 0, C.kDensityMax); }

  Pulse() {
    const ev = this.EventAt(this.pulse_idx);
    if (++this.pulse_idx >= this.pulses_per_bar) {
      this.pulse_idx = 0;
      if (this.labPending) { this.lab = this.labPending; this.labPending = null; }
      this.BarLine();
    }
    return ev;
  }
  EventAt(i) {
    const ev = {note: false, degree: 0, pitch_units: 0, accent: false, thumb: 0};
    const j = i - this.Profile().offset;
    if (j < 0) { ev.pitch_units = this.PitchUnits(0); return ev; }
    ev.thumb = j & 1;
    const t = this.thumb[ev.thumb];
    const idx = this.ElementIndex(ev.thumb, j >> 1);
    const el = t.e[idx];
    ev.degree = el.degree;
    ev.pitch_units = this.PitchUnits(el.degree);
    ev.accent = idx === 0;
    ev.note = !Core.Rests(idx, el, this.EffectiveDensity());
    return ev;
  }
  ElementIndex(th, stroke) { return (this.thumb[th].phase + stroke) % this.thumb[th].n; }
  CurrentElement(th) {
    const j = this.pulse_idx - this.Profile().offset;
    if (j < 0) return this.thumb[th].phase;
    const strokes = th ? (j >> 1) : ((j + 1) >> 1);
    return this.ElementIndex(th, strokes);
  }
  Reroll() { this.DoReroll(); }
  RerollNow() { this.DoReroll(); }
  Reset() {
    this.pulse_idx = 0; this.bar_in_passage = 0;
    this.thumb[0].phase = this.thumb[1].phase = 0;
    this.theme[0].phase = this.theme[1].phase = 0;
    this.SyncCycle(true);
  }
  static Gcd(a, b) { while (b) { const t = a % b; a = b; b = t; } return a; }
  static Lcm(a, b) { return a / Core.Gcd(a, b) * b; }
  static CycleBars(nl, nr, ppb) { return Core.Lcm(Core.Lcm(2 * nl, 2 * nr), ppb) / ppb; }
  SyncCycle(restart) {
    if (restart || this.thumb[0].n !== this.cycle_n[0] || this.thumb[1].n !== this.cycle_n[1]) {
      this.cycle_n = [this.thumb[0].n, this.thumb[1].n];
      const n = Core.CycleBars(this.thumb[0].n, this.thumb[1].n, this.pulses_per_bar);
      this.cycle_bars = n > 255 ? 255 : n;
      this.cycle_bar_idx = 1;
    } else if (++this.cycle_bar_idx > this.cycle_bars) {
      this.cycle_bar_idx = 1;
    }
  }
  ThemeDiffers() { return !Core.SameFigures(this.thumb, this.theme); }

  Profile() { return C.kParts[this.active_part]; }
  TuningRef() { return C.kTunings[0]; }
  PitchUnits(degree) {
    const tu = this.TuningRef(), n = tu.length;
    const oct = degree >= 0 ? Math.trunc(degree / n) : -Math.trunc((-degree + n - 1) / n);
    const idx = degree - oct * n;
    const midi = this.base_midi + 12 * (oct + this.params.octave) + tu[idx];
    return clampI((midi - C.kMidiAtZeroUnits) * C.kUnitsPerSemitone, 0, C.kMaxPitchUnits);
  }
  PitchMidi(degree) { return C.kMidiAtZeroUnits + this.PitchUnits(degree) / C.kUnitsPerSemitone; }
  static Rests(idx, el, density) { return idx > 0 && el.draw * 100 >= density * 256; }

  UpdatePitchBase() {
    const pp = this.Profile();
    let k = (((this.params.root - pp.anchor_midi) % 12) + 12) % 12;
    if (k > 6) k -= 12;
    this.base_midi = pp.anchor_midi + k;
    const n = this.TuningRef().length;
    this.key_lo = Math.trunc((pp.lo10 * n + 5) / 10);
    this.key_hi = Math.trunc((pp.hi10 * n + 5) / 10);
  }
  Keys(th) { const out = []; for (let d = this.key_lo; d <= this.key_hi; ++d) if ((d & 1) === th) out.push(d); return out; }
  IsFit(n) { return this.pulses_per_bar % (2 * n) === 0; }
  FitMask() { let f = 0; for (let n = C.kMinLoop; n <= this.Cap(); ++n) if (this.IsFit(n)) f |= bit(n); return (f & (f - 1)) ? f >>> 0 : this.AllMask(); }
  AllowedMask(other) {
    const all = this.AllMask(), fit = this.FitMask();
    switch (this.Profile().policy) {
      case C.kPolicyBothDrift: return all;
      case C.kPolicyOneDrift: return (fit & bit(other)) ? all : fit;
      default: return fit;
    }
  }
  static PickFromMask(r, mask) {
    const list = []; for (let n = 0; n < 16; ++n) if (mask & bit(n)) list.push(n);
    return list.length ? list[below(r, list.length)] : C.kMinLoop;
  }

  GenThumb(th, n) {
    const keys = this.Keys(th), c = keys.length, L = this.L();
    let k = (th === 0 && !this.lab.lStartMiddle) ? 0 : (c >> 1);
    const t = this.thumb[th];
    t.n = n; t.phase = 0;
    let allEqual = true;
    for (let j = 0; j < n; ++j) {
      if (j) k = clampI(k + C.kStepDelta[pickW(this.rng_part, L.stepW)], 0, c - 1);
      t.e[j].degree = keys[k];
      t.e[j].draw = j ? (next(this.rng_part) >>> 24) : 0;
      if (keys[k] !== t.e[0].degree) allEqual = false;
    }
    if (n > 1 && allEqual) t.e[n - 1].degree = keys[clampI(k + 1, 0, c - 1)];
    for (let j = n; j < C.kMaxLoop; ++j) t.e[j] = {degree: 0, draw: 0};
  }
  GenPart() {
    this.GenThumb(0, this.thumb[0].n);
    this.GenThumb(1, this.thumb[1].n);
    this.theme = [copyThumb(this.thumb[0]), copyThumb(this.thumb[1])];
    this.bar_in_passage = 0;
    this.SyncCycle(true);
  }
  Regenerate(keepPos) {
    const p = this.params, pi = this.pulse_idx, bip = this.bar_in_passage;
    this.pulses_per_bar = p.len;
    this.active_part = p.part;
    this.pulse_idx = keepPos ? pi % this.pulses_per_bar : 0;
    this.UpdatePitchBase();
    this.rng_part = {s: seedState(u32(p.seed + C.kPartStreamOffset * p.part))};
    this.rng_passage = {s: seedState(u32(p.seed + C.kPassageStreamOffset))};
    const d = p.len === 16 ? this.Profile().loops16 : (p.len === 12 ? this.Profile().loops12 : null);
    if (d) { const cap = this.Cap(); this.thumb[0].n = Math.min(d[0], cap); this.thumb[1].n = Math.min(d[1], cap); } else this.RollLoopLengths();
    this.GenPart();
    this.ApplyCap();
    if (keepPos) this.bar_in_passage = bip;
  }
  RollLoopLengths() {
    const f = this.FitMask(), all = this.AllMask(), r = this.rng_passage;
    let l, rr;
    switch (this.Profile().policy) {
      case C.kPolicyFit:
        l = Core.PickFromMask(r, f);
        rr = Core.PickFromMask(r, (f & ~bit(l)) & 0xFFFF);
        break;
      case C.kPolicyOneDrift: {
        const a = Core.PickFromMask(r, f);
        const b = Core.PickFromMask(r, (all & ~bit(a)) & 0xFFFF);
        if (hiBit(r)) { l = a; rr = b; } else { l = b; rr = a; }
        break;
      }
      default:
        l = Core.PickFromMask(r, all);
        rr = Core.PickFromMask(r, (all & ~bit(l)) & 0xFFFF);
    }
    this.thumb[0].n = l; this.thumb[1].n = rr;
  }
  NewPassage(force) {
    if (force || chance(this.rng_passage, this.L().loopChange * 100)) this.RollLoopLengths();
    this.GenPart();
  }
  DoReroll() {
    this.params.seed = next(this.rng_passage) >>> 16;
    this.rng_part = {s: seedState(u32(this.params.seed + C.kPartStreamOffset * this.params.part))};
    this.rng_passage = {s: seedState(u32(this.params.seed + C.kPassageStreamOffset))};
    this.NewPassage(true);
  }
  static SameThumb(a, b) {
    if (a.n !== b.n) return false;
    for (let j = 0; j < a.n; ++j) if (a.e[j].degree !== b.e[j].degree || a.e[j].draw !== b.e[j].draw) return false;
    return true;
  }
  static SameFigures(a, b) { return Core.SameThumb(a[0], b[0]) && Core.SameThumb(a[1], b[1]); }

  Mutate() {
    const r = this.rng_part, L = this.L();
    const th = hiBit(r) ? 1 : 0;
    const t = this.thumb[th];
    const keys = this.Keys(th), c = keys.length;
    const n = this.TuningRef().length;
    switch (pickW(r, L.mutW)) {
      case 0: {   // key
        const j = below(r, t.n);
        const i = keys.indexOf(t.e[j].degree);
        const ni = clampI(i + (hiBit(r) ? 1 : -1), 0, c - 1);
        if (ni === i) return;
        t.e[j].degree = keys[ni];
        return;
      }
      case 1: {   // rest
        if (t.n < 2) return;
        const j = 1 + below(r, t.n - 1);
        const resting = Core.Rests(j, t.e[j], this.EffectiveDensity());
        t.e[j].draw = resting ? 0 : 255;
        return;
      }
      case 2: {   // octave
        const j = below(r, t.n);
        const d = t.e[j].degree;
        const nd = d + n <= this.key_hi + L.headroom ? d + n : (d - n >= this.key_lo ? d - n : d);
        if (nd === d) return;
        t.e[j].degree = nd;
        return;
      }
      default: {  // loop length
        const other = this.thumb[th ^ 1].n;
        const f = (this.AllowedMask(other) & ~bit(t.n) & ~bit(other)) & 0xFFFF;
        if (!f) return;
        let up = 0, dn = 0;
        for (let m = C.kMinLoop; m <= C.kMaxLoop; ++m) if (f & bit(m)) { if (m > t.n) up |= bit(m); else dn |= bit(m); }
        let np = 0;
        const useUp = up && (!dn || hiBit(r));
        if (useUp) { for (let m = C.kMinLoop; m <= C.kMaxLoop; ++m) if (up & bit(m)) { np = m; break; } }
        else { for (let m = C.kMaxLoop; m >= C.kMinLoop; --m) if (dn & bit(m)) { np = m; break; } }
        while (t.n < np) {
          const i = keys.indexOf(t.e[t.n - 1].degree);
          const ni = clampI(i + (hiBit(r) ? 1 : -1), 0, c - 1);
          t.e[t.n].degree = keys[ni]; t.e[t.n].draw = 0;
          ++t.n;
        }
        for (let m = np; m < C.kMaxLoop; ++m) t.e[m] = {degree: 0, draw: 0};
        t.n = np; t.phase = t.phase % np;
        return;
      }
    }
  }
  Evolve() {
    const rate = this.params.variation_pct;
    if (chance(this.rng_part, this.L().ret * rate) && this.ThemeDiffers()) {
      const pl = this.thumb[0].phase, pr = this.thumb[1].phase;
      this.thumb = [copyThumb(this.theme[0]), copyThumb(this.theme[1])];
      this.thumb[0].phase = pl % this.thumb[0].n;
      this.thumb[1].phase = pr % this.thumb[1].n;
    }
    let m = Math.trunc(rate / 100);
    if (chance(this.rng_part, (rate % 100) * 100)) ++m;
    for (let k = 0; k < m; ++k) this.Mutate();
  }
  BarLine() {
    const strokes = this.pulses_per_bar >> 1;
    for (let th = 0; th < 2; ++th) this.thumb[th].phase = (this.thumb[th].phase + strokes) % this.thumb[th].n;
    if (this.bar_in_passage < 255) ++this.bar_in_passage;
    const len = C.kPassageBars[this.params.passage_idx];
    if (len && this.bar_in_passage >= len) { this.NewPassage(false); return; }
    this.Evolve();
    this.SyncCycle(false);
  }
}

// Same line format as host/tools/likembe_dump.cpp: "bar pulse thumb degree rest accent pitch_units".
function dump(params, bars, lab) {
  const core = new Core(lab);
  core.Init(params);
  const out = [];
  for (let b = 0; b < bars; ++b) {
    const B = core.pulses_per_bar;
    for (let p = 0; p < B; ++p) {
      const i = core.pulse_idx, e = core.Pulse();
      out.push(`${b} ${i} ${e.thumb} ${e.degree} ${e.note ? 0 : 1} ${e.accent ? 1 : 0} ${e.pitch_units}`);
    }
  }
  return out.join('\n') + '\n';
}

// ---- custom/core/likembe_clock.h (glue-level Clock page) ----
// LikembeClock::Defaults(right): Src Int, Div 1, Roll = own 2nd input, Rst Off.
function clockDefaults(right) {
  return {src: C.kSrcInt, div: C.kDefaultClockDiv, roll_in: right ? C.kDefaultRollRight : C.kDefaultRollLeft, rst_in: C.kDefaultRst};
}
// PulseDivider::Tick, 1:1: fires on the first pulse after reset (left = 0), then
// every div pulses; a div lowered below the remaining count fires on the next pulse.
function pulseDividerTick(d, div) {   // d = {left}
  const fire = d.left === 0 || d.left > div;
  if (fire) d.left = div;
  d.left -= 1;
  return fire;
}

const E = {C, Core, labDefaults, cloneLab, defaults, clampParams, dump, clockDefaults, pulseDividerTick};
if (typeof module !== 'undefined' && module.exports) module.exports = E; else root.LikembeEngine = E;
})(typeof window !== 'undefined' ? window : this);
