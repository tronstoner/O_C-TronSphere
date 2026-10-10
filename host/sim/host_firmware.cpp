// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Host replacements for firmware definitions whose real .cpp files are
// hardware-bound (OC_core.cpp, OC_DAC.cpp, OC_gpio.cpp, OC_ADC.cpp, display,
// hemisphere_config.h registry). Keep this list minimal; see docs/HOST_SIM.md.
#include "HemisphereApplet.h"

// --- OC_core.cpp ---
volatile uint32_t OC::CORE::ticks = 0;
int OC::CORE::FreeRam() { return 32 * 1024; }

// --- display.cpp ---
weegfx::Graphics graphics;

// --- OC_gpio.cpp (pin numbers; any distinct values < 64 work) ---
uint8_t TR1 = 0, TR2 = 1, TR3 = 2, TR4 = 3;

// --- OC_ADC.cpp ---
ADC_CHANNEL ADC_CHANNEL_1 = 0, ADC_CHANNEL_2 = 1, ADC_CHANNEL_3 = 2, ADC_CHANNEL_4 = 3;
FreqMeasureClass FreqMeasure;

// --- OC_DAC.cpp: identity-ish calibration so Send() can run ---
DAC_CHANNEL DAC_CHANNEL_A = 0, DAC_CHANNEL_B = 1, DAC_CHANNEL_C = 2, DAC_CHANNEL_D = 3;
#ifdef VOR
int OC::DAC::kOctaveZero = 3;
#endif
static OC::DAC::CalibrationData host_dac_cal = [] {
  OC::DAC::CalibrationData c{};
  for (int ch = 0; ch < DAC_CHANNEL_COUNT; ++ch)
    for (int o = 0; o <= OCTAVES; ++o) c.calibrated_octaves[ch][o] = (uint16_t)(o * 6553);
  return c;
}();
OC::DAC::CalibrationData *OC::DAC::calibration_data_ = &host_dac_cal;
uint32_t OC::DAC::values_[DAC_CHANNEL_COUNT];
uint8_t OC::DAC::get_voltage_scaling(uint8_t) { return 0; }

// --- hemisphere_config.h registry: the sim hosts one applet, nothing hidden ---
namespace HS {
bool applet_is_hidden(const int &) { return false; }
const char *get_applet_name(const int) { return "sim"; }
const uint8_t *get_applet_icon(const int) { return ZAP_ICON; }
}  // namespace HS
