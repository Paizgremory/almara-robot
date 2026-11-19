// demo_wajah_warna.ino — contoh berdiri sendiri untuk mencoba wajah.h baru (full-color) di layar
// TFT 1,8" 128x160 board Almara v9.6 (ESP32-S3). Langsung flash, tidak perlu bagian lain project.
//
// Siklus: IDLE (senyum + jam) -> LISTEN (gelombang) -> THINK (titik mikir) -> TALK (mulut ikut
// "suara" palsu) -> ulang. Ganti logikanya nanti sesuai event asli robot (mic, hasil AI, dst).

#include "wajah_warna.h"

void setup() {
  faceInit();
}

void loop() {
  uint32_t t = millis() % 12000;  // siklus demo 12 detik

  if (t < 4000) {
    faceSetState(FACE_IDLE);
    faceSetTime(13, 22, true);        // ganti dengan jam RTC asli
  } else if (t < 6500) {
    faceSetState(FACE_LISTEN);
    faceSetAudioLevel(0.4f + 0.5f * fabsf(sinf(millis() / 150.0f)));  // ganti dgn level mic asli
  } else if (t < 8500) {
    faceSetState(FACE_THINK);
  } else {
    faceSetState(FACE_TALK);
    faceSetAudioLevel(0.3f + 0.6f * fabsf(sinf(millis() / 90.0f)));   // ganti dgn level speaker asli
  }

  faceUpdate();
}
