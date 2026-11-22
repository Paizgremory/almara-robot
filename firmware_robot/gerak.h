// gerak.h — 6 servo sesuai CASE FISIK (video + konfirmasi langsung dari pemilik robot):
//   2 di bahu/tangan, dan SEPASANG kaki yang MASING-MASING punya 2 sendi bertingkat:
//     - PAHA   : memutar seluruh kaki ke KIRI/KANAN (yaw, bukan angkat-turun)
//     - ENGKEL : ayun NAIK-TURUN BESAR, persis seperti gerakan tangan (bukan "melawan paha")
// Gerakan halus (non-blocking), dipicu dari event wajah/joget yang sudah ada di robot_ngobrol.ino.
//
// Dari video: posisi DIAM tangan = MENGGANTUNG LURUS KE BAWAH, gestur ayun tangan = SAPUAN BESAR
// dari bawah sampai hampir ke atas. Engkel kaki dibuat memakai kurva ayun YANG SAMA dengan tangan
// (netral = menggantung/turun, gestur = sapu ke atas). Paha kaki dibuat memutar kiri-kanan di
// sekitar netral (netral = lurus ke depan), dipakai untuk kesan jalan di tempat / joget / kaget.
//
// Wiring servo (semua servo: merah=5V, coklat/hitam=GND, oranye/kuning=sinyal ke GPIO di bawah).
// PENTING: servo JANGAN diberi daya dari pin 5V ESP32 — pakai BEC/regulator 5V terpisah
// (minimal 2A) dengan GND disatukan ke GND ESP32, kalau tidak board bisa reset sendiri saat servo narik arus.
//
//   Tangan kiri      -> GPIO 17   (bekas BT_TX, sudah tidak dipakai jembatan BT)
//   Tangan kanan     -> GPIO 18   (bekas BT_RX, sudah tidak dipakai jembatan BT)
//   Paha kiri        -> GPIO 14   (putar kaki kiri ke kiri/kanan)
//   Paha kanan       -> GPIO 21   (putar kaki kanan ke kiri/kanan)
//   Engkel kiri       -> GPIO 12   (ayun kaki kiri naik/turun, seperti tangan)
//   Engkel kanan      -> GPIO 40   (ayun kaki kanan naik/turun, seperti tangan)
//
// Catatan pin: GPIO 19/20 (USB bawaan) tidak dikeluarkan di board ESP32-S3 SuperMini, jadi tidak dipakai.
// Kanal LEDC: 0..5 (kanal 6 sudah dipakai lampu latar TFT di layar_tft.h, jadi tidak disentuh di sini).
//
// Pakai di robot_ngobrol.ino:
//   #include "gerak.h"      // taruh setelah #include "layar_tft.h"
//   setup()  -> gerakBegin();
//   loop()   -> gerakUpdate();                         // panggil tiap putaran loop(), tidak nge-block
//   saat faceEvent(BH_DISCO) / disp.joget == true -> gerakJoget(true);  lalu gerakJoget(false) saat berhenti
//   saat ingin melambai (mis. sapaan awal) -> gerakLambai();
//   saat BH_CONFETTI / kaget senang -> gerakSenang();
#pragma once
#include <Arduino.h>

// ---- pin & kalibrasi ----------------------------------------------------------------------
#define GRK_N 6
// urutan: 0=tangan kiri, 1=tangan kanan, 2=paha kiri, 3=paha kanan, 4=engkel kiri, 5=engkel kanan
static const uint8_t grkPin[GRK_N]     = {17, 18, 14, 21, 12, 40};
static const uint8_t grkCh[GRK_N]      = {0,  1,  2,  3,  4,  5};
// "Derajat logis" per servo, posisi DIAM (netral):
//   Tangan  : 0   = menggantung lurus ke bawah.
//   Paha    : 90  = kaki lurus ke depan (tengah-tengah putaran kiri/kanan).
//   Engkel  : 0   = turun/rapat, sama seperti tangan menggantung.
// UBAH angka ini per servo kalau belum pas di robot asli (beda tiap servo karena pemasangan horn
// tidak selalu pas 0/90 derajat) — nyalakan robot, lihat posisi diam, geser sampai pas.
static int8_t grkNetral[GRK_N]         = {0, 0, 90, 90, 0, 0};
// Arah: 1 = normal, -1 = dibalik (servo sisi kanan biasanya dipasang cermin dari sisi kiri)
static const int8_t grkArah[GRK_N]     = {1, -1, 1, -1, 1, -1};
// Batas aman tiap servo (derajat, sebelum arah dibalik) supaya tidak membentur casing.
// Tangan & engkel: 0 (turun) s/d ~165 (terangkat penuh ke atas, sesuai sapuan di video).
// Paha: putaran kiri-kanan di sekitar netral (90) — mulai sempit (±25°) biar aman, lebarkan
// sendiri (grkMin/grkMax) kalau sudah yakin tidak mentok casing.
static const uint8_t grkMin[GRK_N]     = {0,   0,   65, 65, 0,   0};
static const uint8_t grkMax[GRK_N]     = {165, 165, 115, 115, 165, 165};

#define GRK_HZ 50
#define GRK_RES_BITS 14                 // resolusi duty LEDC
#define GRK_US_MIN 500                  // pulsa servo 0 derajat (us) — sesuaikan kalau servo gemetar/mentok
#define GRK_US_MAX 2500                 // pulsa servo 180 derajat (us)

// ---- state ---------------------------------------------------------------------------------
static float grkCur[GRK_N];             // posisi sekarang (derajat, sudah lewat arah)
static float grkTgt[GRK_N];             // target
static uint32_t grkLastMs = 0;
static bool grkOn = false;              // false = servo lepas tenaga (hemat baterai & tidak panas) setelah diam

static bool grkJoget = false;
static uint32_t grkJogetT0 = 0;
static uint8_t grkGesture = 0;          // 0 = tidak ada, lihat GRK_G_*
static uint32_t grkGestureT0 = 0;
#define GRK_G_LAMBAI 1
#define GRK_G_SENANG 2
#define GRK_G_KAGET  3
#define GRK_G_HANDR  4
#define GRK_G_HANDL  5

static inline int grkDegToDuty(float deg) {
  if (deg < 0) deg = 0; if (deg > 180) deg = 180;
  long us = GRK_US_MIN + (long)((GRK_US_MAX - GRK_US_MIN) * (deg / 180.0f));
  long duty = (long)(((float)us / (1000000.0f / GRK_HZ)) * ((1 << GRK_RES_BITS) - 1));
  return (int)duty;
}

// Tulis derajat "logis" (0=netral arah asli) ke satu servo, sudah dibatasi grkMin/Max dan arah.
static void grkWrite(uint8_t i, float deg) {
  float raw = grkArah[i] > 0 ? deg : (180.0f - deg);
  if (raw < grkMin[i]) raw = grkMin[i];
  if (raw > grkMax[i]) raw = grkMax[i];
  ledcWrite(grkPin[i], grkDegToDuty(raw));
}

static void gerakBegin() {
  for (int i = 0; i < GRK_N; i++) {
    ledcAttachChannel(grkPin[i], GRK_HZ, GRK_RES_BITS, grkCh[i]);
    grkCur[i] = grkTgt[i] = grkNetral[i];
    grkWrite(i, grkCur[i]);
  }
  grkOn = true;
  grkLastMs = millis();
}

// Lepas tenaga semua servo (berhenti kirim pulsa) — dipanggil otomatis beberapa detik setelah diam.
static void gerakRelax() {
  for (int i = 0; i < GRK_N; i++) ledcWrite(grkPin[i], 0);
  grkOn = false;
}

// Set target satu servo (indeks 0..5), derajat 0..180 relatif netral servo itu sendiri.
static inline void gerakSetTarget(uint8_t i, float deg) { if (i < GRK_N) grkTgt[i] = deg; }

static void gerakPose(const int8_t p[GRK_N]) {
  for (int i = 0; i < GRK_N; i++) grkTgt[i] = p[i];
}

// ---- gerakan siap pakai ----------------------------------------------------------------------
static void gerakNetral() { gerakPose(grkNetral); }

static void gerakLambai() { grkGesture = GRK_G_LAMBAI; grkGestureT0 = millis(); }
static void gerakSenang() { grkGesture = GRK_G_SENANG; grkGestureT0 = millis(); }
static void gerakKaget()  { grkGesture = GRK_G_KAGET;  grkGestureT0 = millis(); }
// Gerak satu tangan saja, dipicu tombol remote (lihat ble_pad.h) — sapu sekali dari bawah ke atas
// lalu balik menggantung, PERSIS seperti di video.
static void gerakTanganKanan() { grkGesture = GRK_G_HANDR; grkGestureT0 = millis(); }
static void gerakTanganKiri()  { grkGesture = GRK_G_HANDL; grkGestureT0 = millis(); }

static inline void gerakJoget(bool on) {
  if (on && !grkJoget) grkJogetT0 = millis();
  if (!on && grkJoget) gerakNetral();
  grkJoget = on;
}

// ---- update tiap loop (TIDAK memakai delay, aman dipanggil tiap putaran loop()) --------------
static void gerakUpdate() {
  uint32_t now = millis();
  uint32_t dt = now - grkLastMs;
  if (dt < 15) return;                  // ~60x/detik cukup halus untuk servo
  grkLastMs = now;

  // --- gestur sesaat (lambai / senang / kaget) menimpa target sementara ---
  if (grkGesture) {
    float t = (now - grkGestureT0) / 1000.0f;
    if (grkGesture == GRK_G_LAMBAI) {           // tangan kanan: sapuan naik-turun 2x dari bawah ke atas
      if (t > 1.8f) { grkGesture = 0; gerakNetral(); }
      else {
        float a = fmaxf(0.0f, sinf(t * 7.0f));           // 0..1, naik lalu balik ke 0 (netral/bawah)
        grkTgt[1] = grkNetral[1] + a * (160 - grkNetral[1]);
        grkTgt[0] = grkNetral[0];
        for (int i = 2; i < GRK_N; i++) grkTgt[i] = grkNetral[i];
      }
    } else if (grkGesture == GRK_G_SENANG) {    // dua tangan naik bersama sebentar lalu turun
      if (t > 1.0f) { grkGesture = 0; gerakNetral(); }
      else {
        float a = fmaxf(0.0f, sinf(t * 3.4f));
        grkTgt[0] = grkNetral[0] + a * 110; grkTgt[1] = grkNetral[1] + a * 110;
        for (int i = 2; i < GRK_N; i++) grkTgt[i] = grkNetral[i];
      }
    } else if (grkGesture == GRK_G_KAGET) {     // sentakan cepat: tangan+engkel naik dikit, paha muter dikit, lalu balik
      if (t > 0.5f) { grkGesture = 0; gerakNetral(); }
      else {
        float a = (t < 0.15f) ? t / 0.15f : (1.0f - (t - 0.15f) / 0.35f);
        grkTgt[0] = grkNetral[0] + 40 * a; grkTgt[1] = grkNetral[1] + 40 * a;   // tangan naik dikit
        grkTgt[2] = grkNetral[2] + 15 * a; grkTgt[3] = grkNetral[3] - 15 * a;   // paha: muter berlawanan dikit
        grkTgt[4] = grkNetral[4] + 25 * a; grkTgt[5] = grkNetral[5] + 25 * a;   // engkel: naik dikit (kayak tangan)
      }
    } else if (grkGesture == GRK_G_HANDR || grkGesture == GRK_G_HANDL) {  // satu tangan: sapu sekali ke atas lalu turun
      uint8_t idx = grkGesture == GRK_G_HANDR ? 1 : 0;
      if (t > 0.9f) { grkGesture = 0; gerakNetral(); }
      else {
        float a = sinf(t * 3.5f) * (1.0f - t / 0.9f);     // naik cepat, turun pelan (mirip video)
        if (a < 0) a = 0;
        for (int i = 0; i < GRK_N; i++) grkTgt[i] = grkNetral[i] + (i == idx ? a * (160 - grkNetral[idx]) : 0);
      }
    }
  } else if (grkJoget) {
    // --- berjoget: tangan gantian naik-turun (ayun besar), engkel juga gantian naik-turun (ayun
    // besar, kayak "jalan di tempat"), paha muter kiri-kanan pelan (kesan goyang pinggul) ---
    float t = (now - grkJogetT0) / 1000.0f;
    float sA = fmaxf(0.0f, sinf(t * 5.0f)), sB = fmaxf(0.0f, sinf(t * 5.0f + PI));
    grkTgt[0] = grkNetral[0] + sA * 90; grkTgt[1] = grkNetral[1] + sB * 90;      // tangan
    grkTgt[4] = grkNetral[4] + sB * 50; grkTgt[5] = grkNetral[5] + sA * 50;      // engkel (gantian dgn tangan sisi lawan)
    float pahaA = 18 * sinf(t * 2.5f);
    grkTgt[2] = grkNetral[2] + pahaA; grkTgt[3] = grkNetral[3] + pahaA;          // paha muter bareng (goyang pinggul)
  }

  // --- gerak halus menuju target (ease) + deteksi diam untuk hemat daya ---
  bool moving = false;
  for (int i = 0; i < GRK_N; i++) {
    float d = grkTgt[i] - grkCur[i];
    if (fabsf(d) > 0.3f) {
      if (!grkOn) { grkOn = true; }
      grkCur[i] += d * 0.25f;           // 0.25 = kecepatan easing; perbesar supaya lebih cepat
      grkWrite(i, grkCur[i]);
      moving = true;
    }
  }
  static uint32_t idleSince = 0;
  if (moving) idleSince = now;
  else if (grkOn && !grkJoget && !grkGesture && now - idleSince > 4000) gerakRelax();  // diam 4 dtk -> lepas tenaga
}
