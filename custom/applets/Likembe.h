// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Likembe -- one likembe-style part (two thumbs, interlocking loops) per
// instance (id 201). Logic: custom/core/likembe_core.h. This header is glue
// only: I/O, cursor/encoder, display, persistence. See docs/modules/likembe.md.
//
// Clock page (main row "Clock >"; settings in custom/core/likembe_clock.h, rules
// in docs/CLOCK.md): Src Int = Clock(0) as upstream routes it, one step per
// pulse; Src TRn = physical jack n (rising edge of HS::frame.gate_high[n-1],
// ClockSetup never affects it) divided by Div. Roll TRn = immediate re-roll,
// Rst TRn = restart at bar start. Clock start restarts via upstream Reset().
// CV1 density mod (+-3 V = +-kDensityCvRange %), Out A velocity gate per note, Out B pitch.
#pragma once
#include "custom/core/likembe_core.h"
#include "custom/core/likembe_clock.h"

class Likembe : public HemisphereApplet {
public:
  const char* applet_name() { return "Likembe"; }

  void Start() {
    core.Init(custom::LikembeParams::Defaults(custom::likembe::kPartBass));
    clk = custom::LikembeClock::Defaults(Right());
    cursor = 0;
    page = false;
    pitch = core.EventAt(0).pitch_units;
  }

  // Bar start; the next input pulse steps. The interval across a stop/reset is
  // not measured: the next gate uses the last measured step interval.
  void Reset() { core.Reset(); divider.Reset(); have_pulse = false; }

  void Controller() {
    using namespace custom::likembe;
    if (since_pulse < kMaxPulseTicks) ++since_pulse;
    if (gate_ticks && --gate_ticks == 0) Out(0, 0);
    // CV1 -> density offset (integer, constant divisor).
    core.SetDensityMod((int8_t)(In(0) * kDensityCvRange / HEMISPHERE_3V_CV));
    // Physical jack rising edges (TR1..TR4 as bits), independent of ClockSetup.
    uint8_t level = 0;
    for (int i = 0; i < 4; ++i) level |= (uint8_t)(HS::frame.gate_high[i] ? 1 << i : 0);
    jack_rise = level & ~jack_level;
    jack_level = level;
    if (clk.roll_in != kRollOff && JackRose(clk.roll_in)) core.Reroll();
    if (clk.rst_in != kRstOff && JackRose(clk.rst_in)) Reset();
    if (StepPulse()) {
      if (have_pulse) pulse_ticks = since_pulse;   // already clamped to kMaxPulseTicks
      have_pulse = true;
      since_pulse = 0;
      custom::LikembeEvent e = core.Pulse();
      if (e.note) {
        pitch = e.pitch_units;
        last_degree = e.degree; last_accent = e.accent; have_note = true;
        Out(1, pitch);
        Out(0, e.accent ? kGateAccentUnits : kGateNormalUnits);
        gate_ticks = pulse_ticks * kGateLengthPct / 100;
        if (gate_ticks == 0) gate_ticks = 1;
      }   // rest: no new gate; a running gate ends on its countdown
    }
  }

  void View() {
    if (page) { DrawPage(); return; }
    DrawParam();
    DrawTines();
    DrawSteps();
    DrawSquares();
    DrawThumb(0, kThumbY0);
    DrawThumb(1, kThumbY0 + kThumbPitch);
    DrawStatus();
  }

  void OnButtonPress() {
    if (page) {
      if (!EditMode()) {
        if (page_cursor == kPgBack) { LeavePage(); return; }
        if (page_cursor == kPgDiv && clk.src == custom::likembe::kSrcInt) return;   // "-": not editable
      }
      CursorToggle();
      return;
    }
    if (!EditMode() && cursor == kCurClock) { page = true; page_cursor = kPgSrc; ResetCursor(); return; }
    CursorToggle();
  }

  // Other hemisphere's button while editing: cancel the edit; on the page also back to the list.
  void AuxButton() {
    CancelEdit();
    if (page) LeavePage();
  }

  void OnEncoderMove(int direction) {
    using namespace custom::likembe;
    if (page) {
      if (!EditMode()) { MoveCursor(page_cursor, direction, kPgLast); return; }
      switch (page_cursor) {
        case kPgSrc:  clk.src = (uint8_t)constrain(clk.src + direction, 0, kSrcCount - 1); divider.Reset(); break;
        case kPgDiv:  if (clk.src != kSrcInt) clk.div = (uint8_t)constrain(clk.div + direction, kClockDivMin, kClockDivMax); break;
        case kPgRoll: clk.roll_in = (uint8_t)constrain(clk.roll_in + direction, 0, kRollCount - 1); break;
        case kPgRst:  clk.rst_in = (uint8_t)constrain(clk.rst_in + direction, 0, kRstCount - 1); break;
      }
      return;
    }
    if (!EditMode()) { MoveCursor(cursor, direction, kCursorLast); return; }
    custom::LikembeParams p = core.params;   // core owns the seed after re-rolls
    switch (cursor) {
      case kCurDensity: p.density_pct = (uint8_t)constrain(p.density_pct + direction * kDensityStep, kDensityMin, kDensityMax); break;
      case kCurVar:     p.variation_pct = (uint8_t)constrain(p.variation_pct + direction * kVarStep, 0, kVariationMax); break;
      case kCurPart:    p.part = (uint8_t)constrain(p.part + direction, 0, kPartCount - 1); break;
      case kCurLen:     p.len = (uint8_t)constrain(p.len + direction, kLenMin, kLenMax); break;
      case kCurRoot:    p.root = (uint8_t)((p.root + 12 + direction) % 12); break;
      case kCurOct:     p.octave = (int8_t)constrain(p.octave + direction, kOctaveMin, kOctaveMax); break;
      case kCurPassage: p.passage_idx = (uint8_t)constrain(p.passage_idx + direction, 0, kPassageCount - 1); break;
      case kCurSeed:    core.RerollNow(); return;   // deliberate edit: new seed immediately
      case kCurClock:   return;                     // opens the page on press
    }
    core.SetParams(p);
  }

  uint64_t OnDataRequest() { return core.Pack() | clk.Pack(); }
  void OnDataReceive(uint64_t data) {
    core.Init(custom::LikembeCore::UnpackParams(data));
    clk = custom::LikembeClock::Unpack(data, Right());
    divider.Reset();
    pitch = core.EventAt(0).pitch_units;
  }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "Clock";
    help[HELP_DIGITAL2] = "Reroll";
    help[HELP_CV1]      = "Density";
    help[HELP_CV2]      = "";
    help[HELP_OUT1]     = "Gate acc";
    help[HELP_OUT2]     = "Pitch";
    help[HELP_EXTRA1]   = "Clock>: Src Div Roll";
    help[HELP_EXTRA2]   = "Seed enc: new pattern";
    //                    "---------------------" <-- Extra text size guide
  }

private:
  enum Cursor { kCurDensity, kCurVar, kCurPart, kCurLen, kCurRoot, kCurOct, kCurPassage, kCurSeed, kCurClock };
  static constexpr int kCursorLast = kCurClock;
  enum PageCursor { kPgSrc, kPgDiv, kPgRoll, kPgRst, kPgBack };
  static constexpr int kPgLast = kPgBack;
  static constexpr int kPageRowY0 = 14;  // Clock page: 5 rows, 10 px pitch (14..61)
  static constexpr int kPageRowH = 10;
  static constexpr int kPageValX = 34;   // value column; cursor 29 px to x 62
  static constexpr int kDensityStep = 5;
  static constexpr int kVarStep = 5;
  // Layout (y = top of element)
  static constexpr int kTextY = 14;
  static constexpr int kBridgeY = 24;    // tine bridge line; tines hang below it
  static constexpr int kTineMaxLen = 12; // centre tine 25..36
  static constexpr int kTineMinLen = 6;
  static constexpr int kTinePitch = 4;
  static constexpr int kTineCx = 31;
  static constexpr int kMaxTines = 15;
  static constexpr int kStepY = 38;      // step row, 6 px tall (38-43)
  static constexpr int kStepH = 6;
  static constexpr int kBeatPulses = 4;
  static constexpr int kTickY = 46;      // beat ticks (current-step bar at 44-45)
  static constexpr int kSqY = 48;        // bar-cycle squares, 3 px tall (48-50)
  static constexpr int kSqMaxW = 50;     // squares end before x 50 (theme glyph at 58)
  static constexpr int kThumbY0 = 52;    // two rows, 5 px tall, 6 px pitch (52-56, 58-62)
  static constexpr int kThumbPitch = 6;
  static constexpr int kThumbCell = 7;
  static constexpr int kThumbX = 5;      // after the 3x5 L/R label; 5x5 cells, 2 px gap, 8 cells end at x 58
  static constexpr int kCellS = 5;

  custom::LikembeCore core;
  custom::LikembeClock clk = custom::LikembeClock::Defaults(false);
  custom::PulseDivider divider;
  int cursor = 0;
  bool page = false;          // Clock page shown
  int page_cursor = 0;
  uint8_t jack_level = 0;     // TR1..TR4 levels last tick (bits), for edges
  uint8_t jack_rise = 0;      // TR1..TR4 rising edges this tick (bits)
  int pitch = 0;
  uint32_t since_pulse = 0;   // ticks since last pulse clock, clamped
  uint32_t pulse_ticks = custom::likembe::kDefaultPulseTicks;   // measured interval
  uint32_t gate_ticks = 0;    // Out A countdown; 0 = low
  bool have_pulse = false;
  uint8_t last_degree = 0;    // display only: last struck degree / accent
  bool last_accent = false;
  bool have_note = false;

  bool Right() const { return io_offset != 0; }
  void LeavePage() { page = false; cursor = kCurClock; ResetCursor(); }

  // Engine step: Src Int = Clock(0) (upstream routing), one step per pulse;
  // Src TRn = rising edge of physical jack n, one step every Div pulses.
  bool JackRose(uint8_t tr) const { return (jack_rise >> ((tr - 1) & 3)) & 1; }   // tr 1..4
  bool StepPulse() {
    using namespace custom::likembe;
    if (clk.src == kSrcInt) return Clock(0);
    return JackRose(clk.src) && divider.Tick(clk.div);
  }

  // Formatting for the Clock page values (one place).
  static const char* InName(uint8_t tr) {   // tr 1..4
    static const char* const kTr[] = {"TR1", "TR2", "TR3", "TR4"};
    return kTr[(tr - 1) & 3];
  }
  void PrintPageValue(int row) {
    using namespace custom::likembe;
    switch (row) {
      case kPgSrc:  gfxPrint(clk.src == kSrcInt ? "Int" : InName(clk.src)); break;
      case kPgDiv:  if (clk.src == kSrcInt) gfxPrint("-"); else { gfxPrint("/"); gfxPrint(clk.div); } break;
      case kPgRoll: gfxPrint(clk.roll_in == kRollOff ? "Off" : InName(clk.roll_in)); break;
      case kPgRst:
        gfxPrint(clk.rst_in == kRstOff ? "Off" : InName(clk.rst_in)); break;
    }
  }

  void DrawPage() {
    static const char* const kRows[] = {"Src", "Div", "Roll", "Rst", "Back"};
    for (int r = 0; r <= kPgLast; ++r) {
      const int y = kPageRowY0 + r * kPageRowH;
      gfxPrint(1, y, kRows[r]);
      if (r != kPgBack) { gfxPrint(kPageValX, y, ""); PrintPageValue(r); }
    }
    const int y = kPageRowY0 + page_cursor * kPageRowH;
    if (page_cursor == kPgBack) gfxCursor(1, y + 8, 25);
    else gfxCursor(kPageValX - 1, y + 8, 63 - kPageValX);
  }

  void DrawParam() {
    static const char* const kNames[] = {"Dens", "Var", "Part", "Len", "Root", "Oct", "Roll", "Seed", "Clock"};
    static const char* const kParts[] = {"Bass", "Lead", "MiSol", "Solo"};
    static const char* const kNotes[] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    const custom::LikembeParams& p = core.params;
    gfxPrint(1, kTextY, kNames[cursor]);
    const int vx = 30;
    if (cursor == kCurClock) {   // "Clock >": press opens the page
      gfxPrint(vx + 9, kTextY, ">");
      gfxCursor(vx, kTextY + 8, 33);
      return;
    }
    gfxPrint(vx, kTextY, "");
    switch (cursor) {
      case kCurDensity: gfxPrint(core.EffectiveDensity()); gfxPrint("%"); break;
      case kCurPart:    gfxPrint(kParts[p.part]); break;
      case kCurRoot:    gfxPrint(kNotes[p.root]); break;
      case kCurOct:     if (p.octave > 0) gfxPrint("+"); gfxPrint(p.octave); break;
      case kCurLen:     gfxPrint(p.len); break;
      case kCurVar:     gfxPrint(p.variation_pct); gfxPrint("%"); break;
      case kCurPassage:
        if (custom::likembe::kPassageBars[p.passage_idx] == 0) gfxPrint("Off");
        else { gfxPrint(custom::likembe::kPassageBars[p.passage_idx]); gfxPrint(":"); gfxPrint(core.bar_in_passage); }
        break;
      case kCurSeed:    gfxPrint((int)p.seed); break;
    }
    gfxCursor(vx, kTextY + 8, 33);
  }

  // Kalimba tines, r = degree - key_lo. Sides follow the core's thumb split by
  // ABSOLUTE degree parity: even degree (thumb L) left from x 29, odd degree
  // (thumb R) right from x 33, each side's lowest degree nearest the centre.
  // Column within a side = (d - first same-parity degree) / 2; length shrinks
  // 1 px per column outward. Tops hang from a bridge line.
  int TineCount() const {
    int n = core.key_hi + custom::likembe::kOctaveHeadroom - core.key_lo + 1;
    return n > kMaxTines ? kMaxTines : n;
  }
  int TineCol(int r) const {
    const int lo = core.key_lo, d = lo + r;
    return (d - (lo + ((d ^ lo) & 1))) / 2;
  }
  int TineX(int r) const {
    const int c = TineCol(r);
    return ((core.key_lo + r) & 1) ? kTineCx + 2 + c * kTinePitch : kTineCx - 2 - c * kTinePitch;
  }
  int TineLen(int r) const { const int l = kTineMaxLen - TineCol(r); return l < kTineMinLen ? kTineMinLen : l; }

  void DrawTines() {
    const int n = TineCount();
    int xl = kTineCx, xr = kTineCx;
    for (int r = 0; r < n; ++r) {
      const int x = TineX(r);
      if (x < xl) xl = x; if (x > xr) xr = x;
      gfxLine(x, kBridgeY + 1, x, kBridgeY + TineLen(r));
    }
    gfxLine(xl - 2, kBridgeY, xr + 2, kBridgeY);
    if (!have_note) return;
    const int r = (int)last_degree - core.key_lo;
    if (r < 0 || r >= n) return;
    const int x = TineX(r), tip = kBridgeY + TineLen(r);
    if (gate_ticks) {
      gfxRect(x - 1, kBridgeY + 1, 3, TineLen(r));
      if (last_accent) gfxRect(x - 2, tip - 2, 5, 3);   // accent: wider tip
    } else {
      gfxLine(x - 1, tip + 1, x + 1, tip + 1);            // last struck: 3 px foot under the tip
    }
  }

  // Step row = rhythm layer: this bar's positions (tall = bar head, short =
  // plays, dot = rest / offset); the pulse just played is marked by a bar below (running light).
  // Cells 4 px up to 16 pulses, 2 px above (32 x 2 = 64). Beat ticks every
  // kBeatPulses pulses.
  void DrawSteps() {
    const int b = core.pulses_per_bar, w = b <= 16 ? 4 : 2;
    for (int i = 0; i < b; ++i) {
      const int x = i * w;
      if (!core.RestAt((uint8_t)i)) {
        const int h = i == core.RhythmHead() ? kStepH : kStepH - 3;
        gfxRect(x, kStepY + kStepH - h, w - 1, h);
      } else {
        gfxPixel(x + (w - 1) / 2, kStepY + kStepH - 1);
      }
    }
    if (have_pulse) {   // running light: 2 px bar under the pulse just played
      const int cur = core.pulse_idx == 0 ? b - 1 : core.pulse_idx - 1;
      gfxRect(cur * w, kStepY + kStepH, w - 1, 2);
    }
    for (int i = 0; i < b; i += kBeatPulses) gfxLine(i * w, kTickY, i * w + (i == 0 ? 1 : 0), kTickY);
  }

  // Realignment cycle: one 3x3 square per bar, current filled; thin segments if many.
  void DrawSquares() {
    const int n = core.cycle_bars, cur = core.cycle_bar_idx - 1;
    if (n * 4 <= kSqMaxW) {
      for (int i = 0; i < n; ++i) {
        if (i == cur) gfxRect(i * 4, kSqY, 3, 3); else gfxFrame(i * 4, kSqY, 3, 3);
      }
    } else {
      const int w = kSqMaxW / n > 1 ? kSqMaxW / n : 1;
      for (int i = 0; i < n && i * w < kSqMaxW; ++i) {
        const int x = i * w;
        if (i == cur) gfxRect(x, kSqY, w > 1 ? w - 1 : 1, 3);
        else gfxPixel(x, kSqY + 2);
      }
    }
  }

  void DrawThumb(uint8_t th, int y) {
    const custom::LikembeThumb& t = core.thumb[th];
    const uint8_t cur = core.CurrentElement(th);
    for (uint8_t j = 0; j < t.n; ++j) {
      const int x = kThumbX + j * kThumbCell;
      if (j == cur) gfxRect(x, y, kCellS, kCellS);
      else gfxFrame(x, y, kCellS, kCellS);
    }
    // 3x5 pixel label (the 8 px font would collide with the squares row)
    gfxLine(0, y, 0, y + 4);
    if (th) {   // R
      gfxLine(1, y, 1, y); gfxPixel(2, y + 1); gfxPixel(1, y + 2); gfxPixel(2, y + 3); gfxPixel(2, y + 4);
    } else {    // L
      gfxLine(1, y + 4, 2, y + 4);
    }
  }


  void DrawStatus() {
    // Right of the squares row, 3 px pixel glyphs (font glyphs would hit the
    // tick and loop rows): '~' at x 58 = figures differ from the passage theme.
    const int y = kSqY;
    if (core.ThemeDiffers()) {
      gfxPixel(58, y + 1); gfxPixel(59, y); gfxPixel(60, y + 1); gfxPixel(61, y + 2); gfxPixel(62, y + 1);
    }
  }
};
