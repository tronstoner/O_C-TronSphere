// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// likembe_constants.h -- tuning values for the Likembe pattern core.
// Source: docs/research/Likembe pattern engine technical concept.md and the JS
// reference (docs/research/Congotronics line generator.html). Docs reference the
// names below; values live only here.
#pragma once
#include <cstdint>

namespace custom {
namespace likembe {

// ---- Pitch mapping (O_C pitch CV units) ----
constexpr int kUnitsPerSemitone = 128;
constexpr int kUnitsPerOctave = 1536;
constexpr int kMidiAtZeroUnits = 36;      // 0 units = C2 (MIDI 36) = 0 V
constexpr int kMaxPitchUnits = 9216;      // 6 V; outputs are clamped to 0..this

// ---- Tunings (semitone offsets within one octave) ----
constexpr uint8_t kMaxTuningLen = 7;
struct Tuning { uint8_t n; uint8_t off[kMaxTuningLen]; };
constexpr Tuning kTunings[] = {
  {5, {0, 2, 4, 7, 9}},               // 0 pentatonic (MVP: the only enabled one)
  {5, {0, 3, 5, 7, 10}},              // 1 pentatonic, minor colour (disabled)
  {6, {0, 2, 4, 5, 7, 9}},            // 2 hexatonic (disabled)
  {7, {0, 2, 4, 5, 7, 9, 10}},        // 3 seven notes (disabled)
};
constexpr uint8_t kTuningCount = 1;       // enabled tunings (MVP: pentatonic only)

// ---- Time / loops ----
// Len = pulses per bar (bar line, cycle, loop-fit rule, display step row).
constexpr uint8_t kLenMin = 2, kLenMax = 32;
constexpr uint8_t kDefaultLen = 16;
constexpr uint8_t kMinLoop = 2;
constexpr uint8_t kMaxLoop = 8;           // strokes per thumb figure

enum LoopPolicy : uint8_t { kPolicyFit = 0, kPolicyOneDrift = 1, kPolicyBothDrift = 2 };

// ---- Parts ----
enum Part : uint8_t { kPartBass = 0, kPartLead = 1, kPartMiSolo = 2, kPartSolo = 3 };
constexpr uint8_t kPartCount = 4;

// Range in tenths of n (degrees lo = round(lo10*n/10), hi = round(hi10*n/10)).
struct PartProfile {
  uint8_t anchor_midi;
  uint8_t lo10, hi10;
  uint8_t policy;
  uint8_t density_pct;      // default
  uint8_t variation_pct;    // default (rate per bar x100)
  uint8_t offset;           // start offset in pulses
  uint8_t loops16[2];       // default L/R loop lengths at Len 16
  uint8_t loops12[2];       // default L/R loop lengths at Len 12 (other Len: rolled)
};
constexpr PartProfile kParts[kPartCount] = {
  // Bass: spec. variation 0.075 rounds to 8 %.
  {45, 0, 16, kPolicyOneDrift, 85, 8, 0, {4, 3}, {3, 4}},
  // Lead / accompaniment: spec.
  {57, 4, 20, kPolicyFit, 65, 25, 1, {8, 4}, {6, 3}},
  // Mi-solo: PROVISIONAL. Between accompaniment and solo in register and busyness.
  {60, 6, 22, kPolicyFit, 55, 30, 1, {4, 8}, {3, 6}},
  // Solo: PROVISIONAL. Highest, sparsest, most varied; one thumb may drift.
  // Anchor chosen so the highest reachable degree (hi+2 via octave mutation) stays <= 6 V.
  {64, 10, 26, kPolicyOneDrift, 45, 40, 3, {8, 5}, {6, 4}},
};

// ---- Generation / variation weights (integer-scaled from the spec) ----
// Step weights per part: relative odds of the next key step +1, -1, 0, +2, -2 when a
// thumb figure is generated (tuned in the browser lab; spec was 3, 3, 1.2, 0.8, 0.8).
constexpr int8_t kStepDelta[5] = {1, -1, 0, 2, -2};
constexpr uint8_t kStepWeight[kPartCount][5] = {
  {30, 70, 12, 8, 8},    // Bass
  {20, 70, 12, 5, 10},   // Lead
  {30, 70, 12, 8, 8},    // Mi-solo
  {30, 70, 12, 8, 8},    // Solo
};
// Thumb L figure start key: true = middle key (c / 2, like thumb R), false = lowest key.
constexpr bool kThumbLStartMiddle = true;
// Mutation weights key, rest, octave, loop length (spec 3, 2, 0.6, 0.5).
enum Mutation : uint8_t { kMutKey = 0, kMutRest = 1, kMutOctave = 2, kMutLoop = 3 };
constexpr uint8_t kMutWeight[4] = {30, 20, 6, 5};
// Octave mutation may go up to hi + this many degrees (JS reference: hi + 2).
constexpr uint8_t kOctaveHeadroom = 2;

// Rhythm layer rest bias (0..255, added to a position's rest draw before the
// density compare, saturating at 255 so density 100 never rests): last bar
// position (lighter bar end) and all other non-head positions.
constexpr uint8_t kRestBiasLast = 40;
constexpr uint8_t kRestBiasOther = 0;

constexpr uint8_t kReturnToThemePct = 30;     // ret (x rate)
constexpr uint8_t kLoopChangePct = 40;        // loop-length change at a new passage
constexpr uint8_t kEnergyBuildPct = 0;        // not in MVP (unused)

// ---- Parameters ----
constexpr uint8_t kDensityMin = 0, kDensityMax = 100;
constexpr int8_t kOctaveMin = -2, kOctaveMax = 3;   // Oct parameter (octave offset)
constexpr int8_t kDefaultOctave = 0;
constexpr uint8_t kVariationMax = 100;
constexpr int kDensityCvRange = 35;          // CV1: percent points at +-3 V (HEMISPHERE_3V_CV)
constexpr uint8_t kPassageBars[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};   // 0 = Off (manual only)
constexpr uint8_t kPassageCount = sizeof(kPassageBars);
constexpr uint8_t kDefaultPassageIdx = 0;     // Off
constexpr uint8_t kDefaultRoot = 4;           // E
constexpr uint16_t kDefaultSeed = 1;

// ---- Clock page (glue settings, custom/core/likembe_clock.h; docs/CLOCK.md) ----
// Src: Int = the hemisphere's Clock(0) as upstream routes it (internal clock
// multiplier when running, else trigger map); TRn = the physical jack, rising
// edge of HS::frame.gate_high[n-1] (ClockSetup never affects it).
enum ClockSrc : uint8_t { kSrcInt = 0, kSrcTr1, kSrcTr2, kSrcTr3, kSrcTr4, kSrcCount };
// Div: with Src TRn, one engine step every Div input pulses (decrement counter).
constexpr uint8_t kClockDivMin = 1, kClockDivMax = 24;
constexpr uint8_t kDefaultClockDiv = 1;
// Roll: trigger input (physical jack) for an immediate re-roll. Default = the hemisphere's own
// 2nd input (TR2 left, TR4 right).
enum RollIn : uint8_t { kRollOff = 0, kRollTr1, kRollTr2, kRollTr3, kRollTr4, kRollCount };
constexpr uint8_t kDefaultRollLeft = kRollTr2, kDefaultRollRight = kRollTr4;
// Rst: extra trigger input (physical jack) that restarts at bar start (figures
// kept). Clock start always restarts anyway (upstream calls Reset() on start).
enum RstIn : uint8_t { kRstOff = 0, kRstTr1, kRstTr2, kRstTr3, kRstTr4, kRstCount };
constexpr uint8_t kDefaultRst = kRstOff;

// ---- Out A velocity gate (glue) ----
constexpr int kGateAccentUnits = 5 * 1536;    // 5 V on accented notes
constexpr int kGateNormalUnits = 3 * 1536;    // 3 V on other notes
constexpr uint32_t kGateLengthPct = 50;       // gate length = % of measured engine-pulse interval
constexpr uint32_t kDefaultPulseTicks = 2083; // 125 ms @ 16.667 kHz, before first interval
constexpr uint32_t kMaxPulseTicks = 166670;   // 10 s clamp (stopped clock)

// ---- RNG stream offsets (spec: lead stream = seed + 7919, passages = seed + 31) ----
constexpr uint32_t kPartStreamOffset = 7919;  // multiplied by the part index
constexpr uint32_t kPassageStreamOffset = 31;
constexpr uint32_t kSeedMix = 0x9E3779B1u;
constexpr uint32_t kSeedNonZero = 0x6D2B79F5u;

}  // namespace likembe
}  // namespace custom
