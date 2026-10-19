// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Likembe -- one likembe-style part (two thumbs, interlocking loops) per
// instance (id 201). Logic: custom/core/likembe_core.h. This header is glue
// only: I/O, cursor/encoder, display, persistence. See docs/modules/likembe.md.
//
// TR1 pulse clock (one trigger = one grid pulse), TR2 re-roll at next bar line,
// CV1 density mod (+-3 V = +-kDensityCvRange %), Out A trigger per note, Out B pitch.
#pragma once
#include "custom/core/likembe_core.h"

class Likembe : public HemisphereApplet {
public:
  const char* applet_name() { return "Likembe"; }

  void Start() {
    core.Init(custom::LikembeParams::Defaults(custom::likembe::kPartBass));
    cursor = 0;
    pitch = core.EventAt(0).pitch_units;
  }

  void Reset() { core.Reset(); }

  void Controller() {
    // CV1 -> density offset (integer, constant divisor).
    core.SetDensityMod((int8_t)(In(0) * custom::likembe::kDensityCvRange / HEMISPHERE_3V_CV));
    if (Clock(1)) core.Reroll();
    if (Clock(0)) {
      custom::LikembeEvent e = core.Pulse();
      if (e.note) {
        pitch = e.pitch_units;
        Out(1, pitch);
        ClockOut(0);
      }
    }
  }

  void View() {
    DrawParam();
    DrawPulseRow();
    DrawThumb(0, kThumbY0);
    DrawThumb(1, kThumbY0 + kThumbPitch);
    DrawContour();
    DrawStatus();
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) { MoveCursor(cursor, direction, kCursorLast); return; }
    using namespace custom::likembe;
    custom::LikembeParams p = core.params;   // core owns the seed after re-rolls
    switch (cursor) {
      case kCurDensity: p.density_pct = (uint8_t)constrain(p.density_pct + direction * kDensityStep, kDensityMin, kDensityMax); break;
      case kCurPart:    p.part = (uint8_t)constrain(p.part + direction, 0, kPartCount - 1); break;
      case kCurRoot:    p.root = (uint8_t)((p.root + 12 + direction) % 12); break;
      case kCurOct:     p.octave = (int8_t)constrain(p.octave + direction, kOctaveMin, kOctaveMax); break;
      case kCurGrid:    p.grid16 = direction > 0 ? 1 : 0; break;
      case kCurVar:     p.variation_pct = (uint8_t)constrain(p.variation_pct + direction * kVarStep, 0, kVariationMax); break;
      case kCurPassage: p.passage_idx = (uint8_t)constrain(p.passage_idx + direction, 0, kPassageCount - 1); break;
      case kCurSeed:    core.RerollNow(); return;   // deliberate edit: new seed immediately
    }
    core.SetParams(p);
  }

  uint64_t OnDataRequest() { return core.Pack(); }
  void OnDataReceive(uint64_t data) {
    core.Init(custom::LikembeCore::UnpackParams(data));
    pitch = core.EventAt(0).pitch_units;
  }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Pulse";
    help[HELP_DIGITAL2] = "Reroll";
    help[HELP_CV1]      = "Density";
    help[HELP_CV2]      = "";
    help[HELP_OUT1]     = "Trig";
    help[HELP_OUT2]     = "Pitch";
    help[HELP_EXTRA1]   = "1 pulse=16th/trip8th";
    help[HELP_EXTRA2]   = "Seed enc: new pattern";
    //                    "---------------------" <-- Extra text size guide
  }

private:
  enum Cursor { kCurDensity, kCurPart, kCurRoot, kCurOct, kCurGrid, kCurVar, kCurPassage, kCurSeed };
  static constexpr int kCursorLast = kCurSeed;
  static constexpr int kDensityStep = 5;
  static constexpr int kVarStep = 5;
  // Layout (y = top of element)
  static constexpr int kTextY = 14;
  static constexpr int kPulseY = 25;     // 6 px tall
  static constexpr int kPulseH = 6;
  static constexpr int kThumbY0 = 36;    // two rows, 6 px tall, 8 px pitch
  static constexpr int kThumbPitch = 8;
  static constexpr int kThumbCell = 7;
  static constexpr int kThumbX = 7;      // after the L/R label; 8 cells end at x 61 (2 px margin)
  static constexpr int kContourY = 52;   // 11 px tall strip
  static constexpr int kContourH = 11;

  custom::LikembeCore core;
  int cursor = 0;
  int pitch = 0;

  int CellW() const { return 64 / core.pulses_per_bar; }   // 4 (16) or 5 (12)

  void DrawParam() {
    static const char* const kNames[] = {"Dens", "Part", "Root", "Oct", "Grid", "Var", "Pass", "Seed"};
    static const char* const kParts[] = {"Bass", "Lead", "MiSol", "Solo"};
    static const char* const kNotes[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    const custom::LikembeParams& p = core.params;
    gfxPrint(1, kTextY, kNames[cursor]);
    const int vx = 30;
    gfxPrint(vx, kTextY, "");
    switch (cursor) {
      case kCurDensity: gfxPrint(core.EffectiveDensity()); gfxPrint("%"); break;
      case kCurPart:    gfxPrint(kParts[p.part]); break;
      case kCurRoot:    gfxPrint(kNotes[p.root]); break;
      case kCurOct:     if (p.octave > 0) gfxPrint("+"); gfxPrint(p.octave); break;
      case kCurGrid:    gfxPrint(p.grid16 ? 16 : 12); break;
      case kCurVar:     gfxPrint(p.variation_pct); gfxPrint("%"); break;
      case kCurPassage:
        if (custom::likembe::kPassageBars[p.passage_idx] == 0) gfxPrint("Off");
        else { gfxPrint(custom::likembe::kPassageBars[p.passage_idx]); gfxPrint(":"); gfxPrint(core.bar_in_passage); }
        break;
      case kCurSeed:    gfxPrint((int)p.seed); break;
    }
    gfxCursor(vx, kTextY + 8, 33);
  }

  void DrawPulseRow() {
    const int w = CellW();
    for (int i = 0; i < core.pulses_per_bar; ++i) {
      custom::LikembeEvent e = core.EventAt((uint8_t)i);
      const int x = i * w;
      if (e.note) {
        const int h = e.accent ? kPulseH : kPulseH - 2;
        gfxRect(x, kPulseY + kPulseH - h, w - 1, h);
      } else {
        gfxPixel(x + (w - 1) / 2, kPulseY + kPulseH - 1);   // rest / offset: baseline dot
      }
    }
    // current pulse: bar under the cell about to play
    const int cx = core.pulse_idx * w;
    gfxRect(cx, kPulseY + kPulseH + 1, w - 1, 2);
  }

  void DrawThumb(uint8_t th, int y) {
    const custom::LikembeThumb& t = core.thumb[th];
    const uint8_t cur = core.CurrentElement(th);
    const uint8_t dens = core.EffectiveDensity();
    for (uint8_t j = 0; j < t.n; ++j) {
      const int x = kThumbX + j * kThumbCell;
      const bool rest = custom::LikembeCore::Rests(j, t.e[j], dens);
      if (j == cur) gfxRect(x, y, kThumbCell - 1, 6);
      else if (rest) gfxFrame(x, y, kThumbCell - 1, 6, true);
      else gfxFrame(x, y, kThumbCell - 1, 6);
    }
    gfxPrint(0, y - 1, th ? "R" : "L");
  }

  void DrawContour() {
    const int w = CellW();
    const int lo = core.key_lo;
    const int span = core.key_hi + custom::likembe::kOctaveHeadroom - lo;
    int px = -1, py = 0;
    for (int i = 0; i < core.pulses_per_bar; ++i) {
      custom::LikembeEvent e = core.EventAt((uint8_t)i);
      if (!e.note) continue;
      const int x = i * w + (w - 1) / 2;
      const int y = kContourY + kContourH - 1 - ((int)e.degree - lo) * (kContourH - 1) / span;
      if (px >= 0) gfxDottedLine(px, py, x, y, 2);
      gfxRect(x - 1, y - 1, 2, 2);
      px = x; py = y;
    }
  }

  void DrawStatus() {
    // Top-right of the contour strip: blinking '*' = re-roll pending,
    // '~' = figures currently differ from the passage theme.
    if (core.reroll_pending) { if (CursorBlink()) gfxPrint(58, kContourY, "*"); }
    else if (core.ThemeDiffers()) gfxPrint(58, kContourY, "~");
  }
};
