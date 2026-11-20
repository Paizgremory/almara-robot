#pragma once
// Tipe & definisi yang dulu ada di robot_ngobrol.ino tapi dipakai file .h / prototipe otomatis Arduino.
// Harus di-include SETELAH wajah.h (butuh enum Behav) dan SEBELUM ble_pad.h.
#include <Arduino.h>

volatile uint8_t reactReq = 0;   // tingkah wajah yang diminta (dari task mana pun), dijalankan loop utama
inline void react(Behav b) { reactReq = (uint8_t)b; }

#define PB_A 0x01
#define PB_B 0x02
#define PB_X 0x04
#define PB_Y 0x08
#define PB_LB 0x10
#define PB_RB 0x20
#define PD_UP 0x01
#define PD_DOWN 0x02
#define PD_RIGHT 0x04
#define PD_LEFT 0x08
#define PM_START 0x04

struct Pad {
  bool conn = false;
  int16_t lx = 0, ly = 0, rx = 0, ry = 0;
  uint16_t btn = 0;
  uint8_t dpad = 0, misc = 0;
  int16_t l2 = 0, r2 = 0;  // pemicu analog 0..1023
  uint32_t at = 0;
};
Pad padNow;
portMUX_TYPE padMux = portMUX_INITIALIZER_UNLOCKED;

enum Trans : uint8_t { TR_NONE, TR_LEFT, TR_RIGHT, TR_UP, TR_DOWN, TR_IRIS };

struct Timer {
  bool on;
  uint32_t due;
  char label[48];
};

struct Phrase {
  const char *key;
  const char *text;
};

// Prototipe otomatis Arduino tidak membawa [[noreturn]], jadi dideklarasikan di sini lebih dulu.
[[noreturn]] static void otaViaHotspot(const String &alasan);
