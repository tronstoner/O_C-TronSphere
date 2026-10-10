// hello_core.h -- pure, hardware-independent logic for the Hello applet.
//
// Pattern: custom/core/*.h holds plain C++17 (no Arduino/O_C includes) so it
// can be unit-tested on the host. custom/applets/*.h is thin glue that maps
// O_C I/O, encoder and display onto the core.
#pragma once
#include <cstdint>

namespace custom {

struct HelloCore {
  // Offset in O_C pitch units (12 bit/semitone scale: 128 = 1 semitone).
  static constexpr int kOffsetStep = 128;   // 1/12 V (one semitone)
  static constexpr int kOffsetMin = -60;    // steps
  static constexpr int kOffsetMax = 60;     // steps

  int offset_steps = 0;
  uint32_t ticks = 0;

  void Reset() { offset_steps = 0; ticks = 0; }

  void NudgeOffset(int dir) {
    offset_steps += dir;
    if (offset_steps < kOffsetMin) offset_steps = kOffsetMin;
    if (offset_steps > kOffsetMax) offset_steps = kOffsetMax;
  }

  // Call once per ISR tick (advances the animation clock).
  void Tick() { ++ticks; }

  // in + offset; output range clamping is the glue's job (hardware-specific).
  int Apply(int in) const { return in + offset_steps * kOffsetStep; }

  // Position of a little bouncing dot, 0..(width-1), ~1 px per 1/30 s.
  int DotX(int width) const {
    if (width < 2) return 0;
    const uint32_t period = 2u * (uint32_t)(width - 1);
    const uint32_t p = (ticks / 555u) % period;  // 16666 Hz / 555 ~= 30 px/s
    return (int)(p < (uint32_t)width ? p : period - p);
  }

  // Persistence: offset stored as unsigned 8 bit with bias.
  uint64_t Pack() const { return (uint64_t)(uint8_t)(offset_steps - kOffsetMin); }
  void Unpack(uint64_t data) {
    offset_steps = (int)(data & 0xFF) + kOffsetMin;
    NudgeOffset(0);
  }
};

}  // namespace custom
