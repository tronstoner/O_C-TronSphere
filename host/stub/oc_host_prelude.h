// Force-included (-include) before every firmware TU in the host sim.
//
// Quoted sibling includes like "OC_ADC.h" are resolved from the including
// file's own directory first, so a stub dir on -I cannot shadow them. Instead
// we define the replacement here AND the real header's include guard, which
// turns the real header into a no-op.
#pragma once
#include <Arduino.h>
#include "OC_config.h"
#include "OC_options.h"
#include "util/util_math.h"  // host copy from build/gen (asm-free), see gen.sh

// ---- OC_ADC.h (real one drags in the Teensy ADC register driver) ----------
#define OC_ADC_H_
using ADC_CHANNEL = int;
extern ADC_CHANNEL ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, ADC_CHANNEL_4;
static constexpr int ADC_CHANNEL_LAST = ADC_CHANNEL_COUNT;
// Simulated CV inputs in O_C pitch units (128 per semitone, 1536 = 1 V).
extern int32_t host_adc_pitch[ADC_CHANNEL_COUNT];
namespace OC {
class ADC {
public:
  static constexpr uint16_t kDefaultPitchCVScale = SEMITONES << 7;
  static int32_t value(ADC_CHANNEL c) { return host_adc_pitch[c]; }
  static uint32_t raw_value(ADC_CHANNEL c) { return 2048 - host_adc_pitch[c]; }
  static uint32_t smoothed_raw_value(ADC_CHANNEL c) { return raw_value(c); }
  static int32_t pitch_value(ADC_CHANNEL c) { return host_adc_pitch[c]; }
  static int32_t raw_pitch_value(ADC_CHANNEL c) { return host_adc_pitch[c]; }
};
}  // namespace OC

// ---- OC_FreqMeasure.h (Teensy timer peripheral) ---------------------------
#define OC_FREQMEASURE_H
class FreqMeasureClass {
public:
  static void begin() {}
  static uint8_t available() { return 0; }
  static uint32_t read() { return 0; }
  static float countToFrequency(uint32_t c) { return c ? 60000000.f / c : 0.f; }
  static void end() {}
};
extern FreqMeasureClass FreqMeasure;
