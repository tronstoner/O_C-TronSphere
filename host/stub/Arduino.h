// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ralf Traunsteiner
// Host stub for <Arduino.h>: just enough for the O_C sources compiled on macOS.
// Shadows the Teensy core header because host/stub comes first on -I.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <algorithm>

#define PROGMEM
#define FLASHMEM
#define FASTRUN
#define DMAMEM
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#define F(s) (s)

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

using std::min;
using std::max;
using std::abs;
#ifndef constrain
#define constrain(amt, lo, hi) ((amt) < (lo) ? (lo) : ((amt) > (hi) ? (hi) : (amt)))
#endif

typedef bool boolean;
typedef uint8_t byte;

// Simulated time, advanced by the host harness (host_time.cpp / sim).
extern uint32_t host_micros;
inline uint32_t micros() { return host_micros; }
inline uint32_t millis() { return host_micros / 1000; }
inline void delay(uint32_t) {}
inline void delayMicroseconds(uint32_t) {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline void digitalWriteFast(int, int) {}
// Simulated pin levels (O_C trigger inputs are active low: idle = HIGH).
extern uint8_t host_pin_level[64];
inline int digitalRead(int p) { return host_pin_level[p & 63]; }
inline int digitalReadFast(int p) { return host_pin_level[p & 63]; }
inline void noInterrupts() {}
inline void interrupts() {}
inline long random(long hi) { return hi > 0 ? ::random() % hi : 0; }
inline long random(long lo, long hi) { return hi > lo ? lo + ::random() % (hi - lo) : lo; }
inline void randomSeed(unsigned long s) { ::srandom((unsigned)s); }

// Teensy 3.x core / CMSIS bits referenced by util_profiling.h etc.
#define F_CPU 120000000
inline volatile uint32_t &host_reg_dummy() { static volatile uint32_t r; return r; }
#define ARM_DWT_CYCCNT (host_reg_dummy())
#define ARM_DEMCR (host_reg_dummy())
#define ARM_DWT_CTRL (host_reg_dummy())
#define ARM_DEMCR_TRCENA 0
#define ARM_DWT_CTRL_CYCCNTENA 0

// Teensy usbMIDI (normally pulled in by the core via usb_midi.h). Only the
// message-type constants and a do-nothing interface; nothing is ever received.
class usb_midi_class {
public:
  enum { InvalidType = 0, NoteOff = 0x80, NoteOn = 0x90, AfterTouchPoly = 0xA0,
         ControlChange = 0xB0, ProgramChange = 0xC0, AfterTouchChannel = 0xD0,
         PitchBend = 0xE0, SystemExclusive = 0xF0, TimeCodeQuarterFrame = 0xF1,
         SongPosition = 0xF2, SongSelect = 0xF3, TuneRequest = 0xF6,
         Clock = 0xF8, Start = 0xFA, Continue = 0xFB, Stop = 0xFC,
         ActiveSensing = 0xFE, SystemReset = 0xFF };
  bool read(uint8_t = 0) { return false; }
  uint8_t getType() { return 0; }
  uint8_t getChannel() { return 0; }
  uint8_t getData1() { return 0; }
  uint8_t getData2() { return 0; }
  uint8_t *getSysExArray() { return nullptr; }
  uint16_t getSysExArrayLength() { return 0; }
  void sendNoteOn(uint8_t, uint8_t, uint8_t, uint8_t = 0) {}
  void sendNoteOff(uint8_t, uint8_t, uint8_t, uint8_t = 0) {}
  void sendControlChange(uint8_t, uint8_t, uint8_t, uint8_t = 0) {}
  void sendAfterTouch(uint8_t, uint8_t, uint8_t = 0) {}
  void sendAfterTouchPoly(uint8_t, uint8_t, uint8_t, uint8_t = 0) {}
  void sendPitchBend(int, uint8_t, uint8_t = 0) {}
  void sendProgramChange(uint8_t, uint8_t, uint8_t = 0) {}
  void sendSysEx(uint32_t, const uint8_t *, bool = false, uint8_t = 0) {}
  void sendRealTime(uint8_t, uint8_t = 0) {}
  void send_now() {}
};
extern usb_midi_class usbMIDI;

class elapsedMillis {
  uint32_t ms_;
public:
  elapsedMillis() : ms_(millis()) {}
  elapsedMillis(uint32_t v) : ms_(millis() - v) {}
  operator uint32_t() const { return millis() - ms_; }
  elapsedMillis &operator=(uint32_t v) { ms_ = millis() - v; return *this; }
};
class elapsedMicros {
  uint32_t us_;
public:
  elapsedMicros() : us_(micros()) {}
  elapsedMicros(uint32_t v) : us_(micros() - v) {}
  operator uint32_t() const { return micros() - us_; }
  elapsedMicros &operator=(uint32_t v) { us_ = micros() - v; return *this; }
};
