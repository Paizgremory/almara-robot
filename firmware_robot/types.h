#pragma once

// Timer
#ifndef MAX_TIMERS
#define MAX_TIMERS 10
#endif
struct Timer { uint32_t sec; String label; bool on; };

// Behav
struct Behav { uint8_t val; };

// Trans
enum Trans : uint8_t { TR_NONE, TR_LEFT, TR_RIGHT, TR_UP, TR_DOWN, TR_IRIS };

// Pad
struct Pad {
  uint32_t at;
  bool conn;
  int lx, ly, rx, ry;
  uint16_t btn;
  float dpad;
};
