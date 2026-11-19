// wajah_warna.h — wajah Almara FULL-COLOR untuk TFT ST7735 1,8" 128x160, dipasang LANDSCAPE (160x128).
//
// Berbeda dari wajah.h versi lama (yang menggambar ke kanvas 1-bit lalu diberi satu warna tint),
// modul ini menggambar LANGSUNG ke layar berwarna: hijau utama, pipi coklat, kerlip putih/oranye —
// seperti referensi "wajah lucu" yang kamu kirim, disesuaikan untuk layar lebar 160x128.
//
// Dibuat berdiri sendiri (tidak bergantung pada wajah.h lama yang hilang dari zip) supaya bisa
// langsung dicoba di board Almara v9.6 kamu. Pinout & kecepatan SPI mengikuti WIRING.md /
// layar_tft.h project aslimu.
//
// Cara pakai singkat (lihat juga demo_wajah_warna.ino):
//   #include "wajah_warna.h"
//   void setup() { faceInit(); }
//   void loop() {
//     faceSetState(FACE_IDLE);     // atau FACE_LISTEN / FACE_THINK / FACE_TALK
//     faceSetAudioLevel(0.0f);     // 0..1, dipakai saat FACE_TALK & FACE_LISTEN
//     faceSetTime(13, 22, true);   // jam, menit, timeOk
//     faceUpdate();                // panggil tiap loop, redraw diatur otomatis per ~33ms (30fps)
//   }

#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ---------------- pin (sama seperti layar_tft.h / WIRING.md Almara v9.6) ----------------
#define FW_SCK   45
#define FW_MOSI  41
#define FW_CS    47
#define FW_DC    42
#define FW_RST   38
#define FW_BLK   20
#define FW_BLK_CH 6
#define FW_SPI_HZ 27000000UL

static SPIClass fwSpi(FSPI);
static Adafruit_ST7735 fwTft(&fwSpi, FW_CS, FW_DC, FW_RST);

// ---------------- palet warna (RGB565) ----------------
#define FC(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
static const uint16_t FW_BG       = FC(10, 12, 16);     // hitam kebiruan, bukan hitam pekat (lebih "hidup")
static const uint16_t FW_EYE      = FC(60, 200, 120);   // hijau mata, senada referensi
static const uint16_t FW_EYE_DIM  = FC(30, 110, 70);    // hijau gelap (kedip / bayangan)
static const uint16_t FW_HILITE   = FC(255, 255, 255);  // kerlip putih di mata
static const uint16_t FW_BLUSH    = FC(150, 95, 55);    // pipi coklat
static const uint16_t FW_SPARK_W  = FC(255, 255, 255);  // kerlip putih kecil
static const uint16_t FW_SPARK_O  = FC(255, 165, 40);   // kerlip oranye kecil
static const uint16_t FW_MOUTH    = FC(235, 235, 240);  // senyum / mulut
static const uint16_t FW_TEXT     = FC(190, 200, 210);  // teks jam/petunjuk (abu lembut)

// ---------------- state ----------------
enum FaceState : uint8_t { FACE_IDLE, FACE_LISTEN, FACE_THINK, FACE_TALK };

static FaceState fwState = FACE_IDLE;
static float fwLevel = 0;                 // 0..1 tingkat suara (TALK/LISTEN)
static int fwHH = 0, fwMM = 0;
static bool fwTimeOk = false;
static uint32_t fwLastDraw = 0;
static const uint16_t FW_W = 160, FW_H = 128;  // landscape (setRotation(1))

inline void faceSetState(FaceState s) { fwState = s; }
inline void faceSetAudioLevel(float lvl) { fwLevel = lvl < 0 ? 0 : (lvl > 1 ? 1 : lvl); }
inline void faceSetTime(int hh, int mm, bool ok) { fwHH = hh; fwMM = mm; fwTimeOk = ok; }

inline void faceInit() {
  pinMode(FW_BLK, OUTPUT);
  ledcAttach(FW_BLK, 5000, 8);
  ledcWrite(FW_BLK, 255);  // backlight full
  fwSpi.begin(FW_SCK, -1, FW_MOSI, FW_CS);
  fwTft.initR(INITR_BLACKTAB);
  fwTft.setSPISpeed(FW_SPI_HZ);
  fwTft.setRotation(1);  // landscape 160x128. Kalau arahnya kebalik, ganti ke setRotation(3).
  fwTft.fillScreen(FW_BG);
}

// gambar "+" kecil (kerlip) di (x,y) dengan warna c dan ukuran r (lengan salib)
inline void fwSpark(int x, int y, int r, uint16_t c) {
  fwTft.drawFastHLine(x - r, y, 2 * r + 1, c);
  fwTft.drawFastVLine(x, y - r, 2 * r + 1, c);
}

// senyum: kurva lengkung tipis di bawah mata (2 px tebal)
inline void fwSmile(int cx, int cy, int w, uint16_t c) {
  for (int x = -w; x <= w; x++) {
    float t = (float)x / w;                 // -1..1
    int y = (int)(sqrtf(fmaxf(0, 1 - t * t)) * 6.5f);  // lengkung U terbalik -> senyum
    fwTft.drawPixel(cx + x, cy + y, c);
    fwTft.drawPixel(cx + x, cy + y + 1, c);
  }
}

// satu mata: rounded-rect hijau (sudut membulat, bukan kotak tajam ataupun bulat penuh) + highlight
// putih besar; eyeOpen 0..1 (1=terbuka penuh, 0=kedip tertutup)
inline void fwEye(int x0, int y0, int w, int h, float eyeOpen, bool lookDx, int dx) {
  int hh = (int)(h * eyeOpen);
  if (hh < 3) hh = 3;
  int yOff = (h - hh);  // mata "menutup" dari atas ke bawah (kelopak turun)
  fwTft.fillRoundRect(x0, y0 + yOff, w, hh, w / 4, hh < h / 2 ? FW_EYE_DIM : FW_EYE);
  if (eyeOpen > 0.55f) {
    int hx = x0 + w / 2 - 7 + (lookDx ? dx : 0);
    int hy = y0 + yOff + hh / 4 + 4;
    fwTft.fillCircle(hx, hy, 5, FW_HILITE);         // kerlip utama, besar
    fwTft.fillCircle(hx + 7, hy + 9, 2, FW_HILITE);  // kerlip kecil pendamping
  }
}

inline void faceUpdate() {
  uint32_t now = millis();
  if (now - fwLastDraw < 33) return;  // ~30 fps
  fwLastDraw = now;

  fwTft.fillScreen(FW_BG);
  fwTft.drawRoundRect(1, 1, FW_W - 2, FW_H - 2, 8, FW_EYE);  // garis bingkai hijau di tepi layar
  fwTft.drawRoundRect(2, 2, FW_W - 4, FW_H - 4, 7, FW_EYE);  // ditebalkan 2px biar terlihat jelas

  // --- geometri wajah: mata rounded-square, rapat ---
  const int cx = FW_W / 2;        // 80
  const int eyeW = 32, eyeH = 46, gap = 14;
  const int eyesY = 8;
  int ex1 = cx - gap / 2 - eyeW;  // mata kiri
  int ex2 = cx + gap / 2;         // mata kanan

  // kedip berkala (~tiap 3,2 dtk, durasi 140ms), kecuali saat TALK (fokus ke mulut)
  float blink = 1.0f;
  uint32_t cyc = now % 3200;
  if (cyc < 140) { float p = cyc / 140.0f; blink = p < 0.5f ? 1 - p * 2 : (p - 0.5f) * 2; }

  // pupil / highlight bergerak pelan kalau IDLE/THINK (kesan "hidup")
  int lookDx = (int)(sinf(now / 900.0f) * 3);
  bool lookEnabled = (fwState == FACE_IDLE || fwState == FACE_THINK);

  float eyeOpenL = blink, eyeOpenR = blink;
  if (fwState == FACE_THINK) {                     // mata menyipit saat berpikir
    eyeOpenL = eyeOpenR = fminf(blink, 0.65f);
  } else if (fwState == FACE_LISTEN) {              // mata membelalak saat mendengar
    eyeOpenL = eyeOpenR = fminf(1.0f, blink * (1.0f + fwLevel * 0.15f));
  }

  fwEye(ex1, eyesY, eyeW, eyeH, eyeOpenL, lookEnabled, lookDx);
  fwEye(ex2, eyesY, eyeW, eyeH, eyeOpenR, lookEnabled, lookDx);

  // pipi (blush) — bulat coklat, ditempel dekat sudut bawah-luar tiap mata
  fwTft.fillCircle(ex1 + 2, eyesY + eyeH + 2, 7, FW_BLUSH);
  fwTft.fillCircle(ex2 + eyeW - 2, eyesY + eyeH + 2, 7, FW_BLUSH);

  // mulut: senyum kecil & dekat ke mata, atau "bulat" membuka-nutup mengikuti level suara saat TALK
  int mouthY = eyesY + eyeH + 15;
  if (fwState == FACE_TALK) {
    int mh = 3 + (int)(fwLevel * 13);               // tinggi mulut 3..16 px
    fwTft.fillRoundRect(cx - 10, mouthY - mh / 2, 20, mh, 6, FW_MOUTH);
  } else {
    fwSmile(cx, mouthY - 3, 11, FW_MOUTH);
  }

  // kerlip "+" di sekeliling wajah, ukuran bervariasi biar makin terasa "ramai & imut"
  bool twinkle = (now / 600) % 2;
  fwSpark(20, 14, 4, FW_SPARK_W);
  fwSpark(140, 12, twinkle ? 4 : 3, FW_SPARK_O);
  fwSpark(14, 100, twinkle ? 2 : 3, FW_SPARK_W);
  fwSpark(146, 98, 4, FW_SPARK_W);
  fwSpark(104, 16, 2, FW_SPARK_W);

  // indikator status di bawah: titik "berpikir", gelombang suara saat dengar, atau jam saat idle
  int barY = 106;
  if (fwState == FACE_THINK) {
    for (int k = 0; k < 3; k++) {
      int jy = barY - (int)(fabsf(sinf(now / 180.0f + k * 0.9f)) * 6);
      fwTft.fillCircle(cx - 14 + k * 14, jy, 3, FW_EYE);
    }
  } else if (fwState == FACE_LISTEN) {
    for (int k = 0; k < 23; k++) {
      float q = 0.3f + 0.7f * fabsf(sinf(now / 80.0f + k * 0.7f));
      int h = 2 + (int)(fwLevel * 20 * q);
      fwTft.fillRect(8 + k * 6, barY - h / 2, 4, h, FW_EYE);
    }
  } else if (fwTimeOk) {
    char clk[6];
    snprintf(clk, sizeof(clk), "%02d:%02d", fwHH, fwMM);
    fwTft.setFont(NULL);
    fwTft.setTextSize(2);
    fwTft.setTextColor(FW_TEXT);
    fwTft.setCursor(cx - 12 * 5 / 2, barY - 8);
    fwTft.print(clk);
  }
}
