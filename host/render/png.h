// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Minimal PNG writer (8-bit gray or RGB, zlib "stored" blocks, no compression).
// Dependency-free; good enough for screenshots that open in Preview.
#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>

namespace host {

inline uint32_t Crc32(const uint8_t *p, size_t n, uint32_t c = 0) {
  static uint32_t table[256];
  static bool init = false;
  if (!init) {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t v = i;
      for (int k = 0; k < 8; ++k) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
      table[i] = v;
    }
    init = true;
  }
  c ^= 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}

// channels: 1 = gray, 3 = RGB. pixels: h rows of w*channels bytes.
inline bool WritePng(const char *path, int w, int h, int channels, const uint8_t *pixels) {
  auto be32 = [](std::vector<uint8_t> &v, uint32_t x) {
    for (int s = 24; s >= 0; s -= 8) v.push_back((uint8_t)(x >> s));
  };
  // zlib stream: header, stored blocks of filtered rows, adler32.
  std::vector<uint8_t> raw;
  const size_t row = (size_t)w * channels;
  raw.reserve((row + 1) * h);
  for (int y = 0; y < h; ++y) {
    raw.push_back(0);  // filter: none
    raw.insert(raw.end(), pixels + y * row, pixels + (y + 1) * row);
  }
  std::vector<uint8_t> z = {0x78, 0x01};
  for (size_t off = 0; off < raw.size() || off == 0;) {
    size_t n = raw.size() - off;
    if (n > 65535) n = 65535;
    const bool last = off + n == raw.size();
    z.push_back(last ? 1 : 0);
    z.push_back((uint8_t)n); z.push_back((uint8_t)(n >> 8));
    z.push_back((uint8_t)~n); z.push_back((uint8_t)(~n >> 8));
    z.insert(z.end(), raw.begin() + off, raw.begin() + off + n);
    off += n;
    if (last) break;
  }
  uint32_t a = 1, b = 0;
  for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
  be32(z, (b << 16) | a);

  FILE *fp = std::fopen(path, "wb");
  if (!fp) return false;
  auto chunk = [&](const char *type, const std::vector<uint8_t> &data) {
    std::vector<uint8_t> c;
    be32(c, (uint32_t)data.size());
    c.insert(c.end(), type, type + 4);
    c.insert(c.end(), data.begin(), data.end());
    be32(c, Crc32(c.data() + 4, c.size() - 4));
    std::fwrite(c.data(), 1, c.size(), fp);
  };
  static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  std::fwrite(sig, 1, 8, fp);
  std::vector<uint8_t> ihdr;
  be32(ihdr, (uint32_t)w); be32(ihdr, (uint32_t)h);
  ihdr.push_back(8); ihdr.push_back(channels == 3 ? 2 : 0);
  ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
  chunk("IHDR", ihdr);
  chunk("IDAT", z);
  chunk("IEND", {});
  return std::fclose(fp) == 0;
}

}  // namespace host
