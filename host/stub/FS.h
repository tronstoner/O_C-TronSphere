// Host stub for Teensy <FS.h>: enough File/String surface for OC_scales.cpp
// (Scala load/save) to compile. Files are always empty on the host.
#pragma once
#include <string>
#include <cstdlib>

class String {
  std::string s_;
public:
  String(const std::string &s = "") : s_(s) {}
  char charAt(unsigned i) const { return i < s_.size() ? s_[i] : 0; }
  long toInt() const { return std::atol(s_.c_str()); }
  float toFloat() const { return (float)std::atof(s_.c_str()); }
  int indexOf(char c) const { auto p = s_.find(c); return p == std::string::npos ? -1 : (int)p; }
  String substring(unsigned a) const { return a < s_.size() ? String(s_.substr(a)) : String(); }
  String substring(unsigned a, unsigned b) const { return a < s_.size() ? String(s_.substr(a, b - a)) : String(); }
};

class File {
public:
  int available() { return 0; }
  String readStringUntil(char) { return String(); }
  void print(const char *) {}
  template <typename... A> void printf(const char *, A...) {}
  explicit operator bool() const { return false; }
  void close() {}
};
