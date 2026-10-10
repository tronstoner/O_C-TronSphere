// Tier 3: host harness for a single Hemisphere applet.
//
// Arguments form a script executed left to right, e.g.
//   sim --press --encoder +3 --in0 1536 --seconds 2
// The frame is rendered (PBM/PGM/ASCII) and outputs printed at the end.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "HemisphereApplet.h"
#include "custom/applets/Hello.h"
#include "oled_dump.h"

static void Usage() {
  std::puts(
      "usage: sim [--side L|R] [script...]\n"
      "  --seconds S     run Controller() at OC_CORE_ISR_FREQ for S seconds\n"
      "  --ticks N       run N ticks\n"
      "  --inK V         set CV input K (0..3) in pitch units (1536 = 1 V)\n"
      "  --gateK 0|1     set digital input K (0..3)\n"
      "  --encoder N     N encoder detents (+/-)\n"
      "  --press         encoder button press (toggles edit mode)\n"
      "  --out DIR       output dir for sim.pbm / sim_x4.pgm (default .)\n"
      "  --quiet         no ASCII frame dump");
}


// One ISR tick, mirroring HSApplication::BaseController + HemisphereManager.
static void host_tick(HemisphereApplet &a) {
  ++OC::CORE::ticks;
  host_micros += 1000000 / OC_CORE_ISR_FREQ;
  HS::frame.Load();
  a.Controller();
  HemisphereApplet::ProcessCursors();
  HS::frame.Send();
}

// Draw the applet like the Hemisphere app does (header + View()).
static uint8_t *host_render(HemisphereApplet &a) {
  static uint8_t fb[weegfx::Graphics::kFrameSize];
  graphics.Begin(fb, weegfx::CLEAR_FRAME_ENABLE);
  a.BaseView();
  graphics.End();
  return fb;
}

static uint32_t ScriptTicks(double seconds) { return (uint32_t)(seconds * OC_CORE_ISR_FREQ + 0.5); }

int main(int argc, char **argv) {
  HEM_SIDE side = LEFT_HEMISPHERE;
  std::string outdir = ".";
  bool quiet = false;

  HS::frame.Init();
  HS::Init();  // quantizers + default I/O mappings (cvmap/trigmap), as at boot
  Hello applet;
  applet.BaseStart(side);

  auto run = [&](uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) host_tick(applet);
  };

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() -> const char * {
      if (i + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", a.c_str()); std::exit(2); }
      return argv[++i];
    };
    if (a == "--seconds") run(ScriptTicks(std::atof(next())));
    else if (a == "--ticks") run((uint32_t)std::atol(next()));
    else if (a.rfind("--in", 0) == 0 && a.size() == 5) host_adc_pitch[(a[4] - '0') & 3] = std::atoi(next());
    else if (a.rfind("--gate", 0) == 0 && a.size() == 7) {
      int k = (a[6] - '0') & 3;  // trigger inputs are active low
      const uint8_t pins[4] = {TR1, TR2, TR3, TR4};
      host_pin_level[pins[k]] = std::atoi(next()) ? LOW : HIGH;
    }
    else if (a == "--encoder") {
      int n = std::atoi(next());
      for (int k = 0; k < std::abs(n); ++k) applet.OnEncoderMove(n > 0 ? 1 : -1);
    }
    else if (a == "--press") applet.OnButtonPress();
    else if (a == "--out") outdir = next();
    else if (a == "--quiet") quiet = true;
    else if (a == "--side") { side = (next()[0] == 'R') ? RIGHT_HEMISPHERE : LEFT_HEMISPHERE; applet.BaseStart(side); }
    else { Usage(); return a == "--help" || a == "-h" ? 0 : 2; }
  }

  uint8_t *fb = host_render(applet);
  std::printf("t=%.3fs ticks=%u edit=%d\n", (double)OC::CORE::ticks / OC_CORE_ISR_FREQ,
              (unsigned)OC::CORE::ticks, (int)applet.EditMode());
  for (int ch = 0; ch < DAC_CHANNEL_COUNT; ++ch)
    std::printf("out%d=%d ", ch, HS::frame.ViewOut(ch));
  std::printf("\n");
  std::string p = outdir + "/sim.pbm";
  host::WritePbm(fb, p.c_str());
  host::WritePgm(fb, (outdir + "/sim_x4.pgm").c_str());
  std::printf("wrote %s\n", p.c_str());
  if (!quiet) host::PrintAscii(fb);
  return 0;
}
