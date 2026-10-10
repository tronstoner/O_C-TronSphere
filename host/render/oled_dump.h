// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Dump a weegfx 128x64 frame (SH1106 page layout) as PBM, scaled PGM, ASCII.
//
// Layout (weegfx.h get_frame_ptr, SH1106 SendPage(index, frame + index*128)):
//   byte = frame[(y >> 3) * 128 + x], pixel on = bit (y & 7), LSB = top row.
#pragma once
#include <cstdint>
#include <cstdio>

namespace host {

constexpr int kW = 128, kH = 64;

inline bool Pixel(const uint8_t *f, int x, int y) {
  return (f[(y >> 3) * kW + x] >> (y & 7)) & 1;
}

// Plain PBM (P1): 1 = black. We write lit pixels as 0 (white on black look).
inline bool WritePbm(const uint8_t *f, const char *path) {
  FILE *fp = std::fopen(path, "w");
  if (!fp) return false;
  std::fprintf(fp, "P1\n%d %d\n", kW, kH);
  for (int y = 0; y < kH; ++y) {
    for (int x = 0; x < kW; ++x) std::fputc(Pixel(f, x, y) ? '0' : '1', fp);
    std::fputc('\n', fp);
  }
  std::fclose(fp);
  return true;
}

// Binary PGM (P5), scaled, 1 px grid gap for an OLED look. Opens in Preview.
inline bool WritePgm(const uint8_t *f, const char *path, int s = 4) {
  FILE *fp = std::fopen(path, "wb");
  if (!fp) return false;
  std::fprintf(fp, "P5\n%d %d\n255\n", kW * s, kH * s);
  for (int Y = 0; Y < kH * s; ++Y)
    for (int X = 0; X < kW * s; ++X) {
      bool gap = s > 2 && ((X % s) == s - 1 || (Y % s) == s - 1);
      std::fputc(Pixel(f, X / s, Y / s) ? (gap ? 120 : 235) : 10, fp);
    }
  std::fclose(fp);
  return true;
}

// Terminal dump: one char per pixel column, two pixel rows per line
// (upper/lower half blocks), framed.
inline void PrintAscii(const uint8_t *f, FILE *out = stdout) {
  std::fputs("+", out);
  for (int x = 0; x < kW; ++x) std::fputs("-", out);
  std::fputs("+\n", out);
  for (int y = 0; y < kH; y += 2) {
    std::fputs("|", out);
    for (int x = 0; x < kW; ++x) {
      bool a = Pixel(f, x, y), b = Pixel(f, x, y + 1);
      std::fputs(a && b ? "█" : a ? "▀" : b ? "▄" : " ", out);
    }
    std::fputs("|\n", out);
  }
  std::fputs("+", out);
  for (int x = 0; x < kW; ++x) std::fputs("-", out);
  std::fputs("+\n", out);
}

}  // namespace host
