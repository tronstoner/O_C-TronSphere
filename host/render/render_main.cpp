// Tier 2: render a test frame with the real weegfx on the host.
#include <cassert>
#include <cstdio>
#include "drivers/weegfx.h"
#include "oled_dump.h"

uint32_t host_micros = 0;

int main(int argc, char **argv) {
  const char *outdir = argc > 1 ? argv[1] : ".";
  static uint8_t frame[weegfx::Graphics::kFrameSize];
  weegfx::Graphics gfx;
  gfx.Begin(frame, weegfx::CLEAR_FRAME_ENABLE);

  gfx.drawFrame(0, 0, 128, 64);
  gfx.drawStr(4, 3, "weegfx on host");
  gfx.setPrintPos(4, 14);
  gfx.print("int:");
  gfx.print(-1234);
  gfx.drawLine(4, 26, 60, 58);
  gfx.drawCircle(96, 40, 14);
  gfx.drawRect(70, 26, 6, 6);
  gfx.invertRect(2, 2, 90, 10);
  gfx.End();

  // Layout self-check: pixel (x,y) must land in byte (y/8)*128+x, bit y%8.
  static uint8_t f2[weegfx::Graphics::kFrameSize];
  gfx.Begin(f2, weegfx::CLEAR_FRAME_ENABLE);
  gfx.setPixel(5, 0); gfx.setPixel(127, 63); gfx.setPixel(10, 9);
  gfx.End();
  bool ok = f2[5] == 0x01 && f2[7 * 128 + 127] == 0x80 && f2[1 * 128 + 10] == 0x02 &&
            host::Pixel(f2, 10, 9) && !host::Pixel(f2, 10, 8);
  std::printf("%s layout_check\n", ok ? "PASS" : "FAIL");

  char p[512];
  std::snprintf(p, sizeof p, "%s/render.pbm", outdir); host::WritePbm(frame, p);
  std::printf("wrote %s\n", p);
  std::snprintf(p, sizeof p, "%s/render_x4.pgm", outdir); host::WritePgm(frame, p);
  std::printf("wrote %s\n", p);
  host::PrintAscii(frame);
  return ok ? 0 : 1;
}
