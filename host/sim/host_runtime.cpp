// Definitions for the globals that the host stubs (host/stub) declare.
#include <Arduino.h>
#include "OC_config.h"

uint32_t host_micros = 0;
usb_midi_class usbMIDI;
int32_t host_adc_pitch[ADC_CHANNEL_COUNT] = {0};

struct HostPinInit {
  HostPinInit() { for (auto &p : host_pin_level) p = HIGH; }  // inputs idle
};
uint8_t host_pin_level[64];
static HostPinInit host_pin_init;
