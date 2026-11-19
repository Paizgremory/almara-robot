// wajah.h - tampilan OLED robot ngobrol (desain orisinal).
// Mode wajah: mata animasi dengan banyak ekspresi, gerak pegas yang luwes, dan tingkah acak saat diam.
// Mode teks : teks percakapan bergulir. Hanya bergantung pada Adafruit_GFX.
#pragma once
#include <Adafruit_GFX.h>
#include <math.h>
#include <string.h>

#define C_ON 1
#define C_OFF 0

// 1 = tepi mata digambar dengan "abu-abu" semu (dither) supaya gerak < 1 piksel tetap terlihat halus.
// Ubah ke 0 kalau lebih suka tepi mata yang tegas.
#ifndef FACE_SUBPIXEL
#define FACE_SUBPIXEL 1
#endif

// Waktu untuk perhitungan sin/cos. Float hanya punya ~7 digit: setelah robot menyala berjam-jam,
// millis() besar membuat sin(now/150.0f) melompat-lompat. Dibungkus tiap 2^22 ms (~70 menit)
// supaya presisinya tetap sub-milidetik.
inline float tms(uint32_t now) { return (float)(now & 0x3FFFFFu); }
inline float smooth01(float t) { t = t < 0 ? 0 : (t > 1 ? 1 : t); return t * t * (3 - 2 * t); }

enum Act : uint8_t { ACT_IDLE, ACT_LISTEN, ACT_THINK, ACT_TALK, ACT_INFO };
enum Expr : uint8_t { EX_NORMAL, EX_SENANG, EX_SEDIH, EX_KAGET, EX_BINGUNG, EX_TERTAWA, EX_MALU,
                      EX_KESAL, EX_CINTA, EX_KAGUM, EX_NGANTUK, EX_MENANGIS };
// Tingkah singkat: dipilih acak saat diam, atau dipicu kejadian (dielus, alarm)
enum Behav : uint8_t { BH_NONE, BH_DBLINK, BH_WINK, BH_SMILE, BH_CURIOUS, BH_GLANCE, BH_LOOKDOWN,
                       BH_BOUNCE, BH_WIGGLE, BH_YAWN, BH_PET, BH_SHAKE,
                       BH_FLY, BH_SNEEZE, BH_HUM, BH_PEEK, BH_DIZZY,
                       // --- tingkah baru ---
                       BH_SPIN,      // mata berputar mengitari layar
                       BH_COOL,      // kacamata hitam meluncur turun + senyum miring
                       BH_SCAN,      // mode radar: garis pemindai + bidikan di sudut
                       BH_GLITCH,    // wajah error / bergetar acak
                       BH_STARRY,    // mata berbinar penuh bintang
                       BH_NOD,       // ngantuk mengangguk-angguk lalu kaget bangun
                       BH_SUS,       // curiga: mata menyipit melirik samping
                       BH_TONGUE,    // meletin lidah
                       BH_SQUISH,    // memipih & memanjang seperti bola karet
                       BH_ROLL,      // memutar bola mata (bosan)
                       BH_BUBBLE,    // meniup gelembung lalu pecah
                       BH_SHY,       // malu-malu, buang muka lalu melirik balik
                       BH_GRR,       // kesal menggerutu
                       BH_LAUGH,     // ketawa ngakak
                       BH_CAT,       // mode kucing: telinga, kumis, mulut ω
                       BH_LOVE,      // mata hati + hati meledak
                       BH_SCARE,     // takut: mata bulat gemetar + keringat
                       BH_SLOWBLINK, // kedip lambat manja
                       BH_DREAM,     // melamun, gelembung pikiran berisi ikon
                       BH_BROW,      // satu alis naik
                       BH_CROSS,     // mata juling lucu
                       // --- gelombang ke-2 ---
                       BH_HICCUP,    // cegukan "hik!"
                       BH_SHIVER,    // kedinginan gemetar + salju
                       BH_HOT,       // kepanasan, keringat, ngos-ngosan
                       BH_HUNGRY,    // lapar menatap es krim
                       BH_IDEA,      // dapat ide: lampu menyala
                       BH_UFO,       // UFO lewat, mata mengikuti, ada sinar
                       BH_HEARTBEAT, // jantung berdebar dag-dig-dug
                       BH_POKE,      // kena colek: mata > <
                       BH_PIXEL,     // wajah buyar jadi kotak-kotak lalu utuh
                       BH_LOADING,   // robot "loading" dengan bar
                       BH_JAM,       // pakai headphone, goyang musik
                       BH_HELLO,     // menyapa "HALO!"
                       BH_MORPH,     // mata elastis berubah bentuk
                       BH_FLUTTER,   // kedip genit berulang + bulu mata
                       BH_WINKHEART, // kedip sebelah + hati terbang
                       BH_CONFETTI,  // konfeti jatuh (perayaan)
                       BH_DISCO,     // bola disko + sorot lampu
                       BH_EQ,        // equalizer musik di bawah wajah
                       BH_ROCKET,    // roket meluncur melintas
                       BH_RAIN,      // awan mendung + hujan
                       BH_CHECK,     // centang konfirmasi
                       BH_WOW,       // kaget kagum, garis ledakan
                       BH_ZZZ,       // tidur, huruf Z melayang
                       BH_RIPPLE,    // riak lingkaran (respons klik)
                       BH_BOLT,      // petir: tanda diisi daya
                       BH_KISS,      // cium, hati terbang
                       BH_COUNT };

struct BhInfo { uint16_t dur; uint8_t w; };  // lama (ms) & bobot peluang terpilih saat diam
static const BhInfo BH_INFO[BH_COUNT] = {
  {0, 0},                                                   // NONE
  {600, 6}, {700, 6}, {1800, 6}, {1600, 5}, {900, 5},       // DBLINK WINK SMILE CURIOUS GLANCE
  {1500, 3}, {1200, 4}, {1300, 4}, {2400, 3},               // LOOKDOWN BOUNCE WIGGLE YAWN
  {2600, 0}, {1400, 0},                                     // PET SHAKE (hanya dari luar)
  {4200, 4}, {1800, 3}, {3200, 4}, {2600, 3}, {2400, 3},    // FLY SNEEZE HUM PEEK DIZZY
  {2200, 4}, {3200, 4}, {2800, 4}, {1000, 4}, {2200, 4},    // SPIN COOL SCAN GLITCH STARRY
  {3400, 3}, {2200, 4}, {2200, 4}, {1500, 4}, {2600, 3},    // NOD SUS TONGUE SQUISH ROLL
  {3200, 3}, {2600, 4}, {1800, 3}, {2400, 4}, {3200, 4},    // BUBBLE SHY GRR LAUGH CAT
  {2600, 4}, {1800, 3}, {1400, 3}, {3400, 4}, {1500, 4},    // LOVE SCARE SLOWBLINK DREAM BROW
  {1800, 3},                                                // CROSS
  {1800, 4}, {2400, 3}, {2800, 3}, {2800, 4}, {2200, 4},    // HICCUP SHIVER HOT HUNGRY IDEA
  {4200, 4}, {2800, 4}, {1500, 3}, {1800, 4}, {3400, 3},    // UFO HEARTBEAT POKE PIXEL LOADING
  {3600, 4}, {2000, 4}, {2800, 4}, {1600, 4}, {2000, 4},    // JAM HELLO MORPH FLUTTER WINKHEART
  {3000, 2}, {3600, 0}, {3600, 0}, {2800, 3}, {3200, 2},    // CONFETTI DISCO EQ ROCKET RAIN
  {1500, 0}, {1600, 0}, {3200, 0}, {900, 0}, {2600, 0},     // CHECK WOW ZZZ RIPPLE BOLT
  {2200, 3}};                                               // KISS

inline Expr exprFromName(const char *s) {
  static const char *names[] = {"normal", "senang", "sedih", "kaget", "bingung", "tertawa", "malu",
                                "kesal", "cinta", "kagum", "ngantuk", "menangis"};
  for (uint8_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    if (!strcmp(s, names[i])) return (Expr)i;
  return EX_NORMAL;
}

// Pegas: bergerak ke target dengan sedikit memantul, membuat gerakan terasa hidup.
// Dihitung berdasarkan waktu nyata (gSpringF = lama frame / 40 ms), jadi kecepatan & pantulannya
// sama di fps berapa pun; fps tinggi hanya membuat gerakannya lebih halus.
static float gSpringF = 1.0f;
// Arah lirikan tambahan dari roda: belok kanan = melirik kanan, maju = sedikit ke atas (diisi task gerak).
inline volatile float faceSteerX = 0, faceSteerY = 0;
inline volatile float faceNear = 0;  // 0..1: ada tangan / orang dekat sensor jarak -> mata membesar
struct Spring {
  float x = 0, v = 0;
  void step(float t, float k = 0.28f, float damp = 0.62f) {
    float f = gSpringF;
    v += (t - x) * k * f;
    v *= powf(damp, f);
    x += v * f;
  }
};

struct FaceAnim {
  Spring lw, lh, rw, rh, lookX, lookY, bnc;  // bnc: lompatan vertikal dihaluskan supaya tidak patah
  bool ready = false;
  uint32_t nextBlink = 1800, blinkStart = 0;
  float idleLookX = 0, idleLookY = 0;
  uint32_t nextLook = 900, nextBh = 5000, bhStart = 0, bhDur = 0;
  Behav bh = BH_NONE;
  uint32_t seenEvent = 0;
  uint32_t lastFrame = 0;
  Expr lastEx = EX_NORMAL;
  uint32_t exChangeAt = 0;
  uint32_t seed = 0x1234567;
  uint8_t lastBh = 0;                 // tingkah terakhir (supaya tidak diulang berturut-turut)
  uint8_t scene = 0;                  // latar suasana saat diam (0 = matahari/bintang)
  uint32_t sceneAt = 15000;           // kapan ganti suasana
  uint32_t microAt = 0;               // gerak mikro bola mata
  float microX = 0, microY = 0;
  uint32_t mouthAt = 7000, mouthEnd = 0;  // senyum kecil acak saat diam
  uint32_t rnd() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
  int range(int a, int b) { return a + (int)(rnd() % (uint32_t)(b - a + 1)); }
};

static const int EYE_LX = 38, EYE_RX = 90, EYE_Y = 30, EYE_W = 34, EYE_H = 36, EYE_R = 10;

// ---------- Bentuk-bentuk kecil ----------
inline void drawHeart(Adafruit_GFX &g, int cx, int cy, int s) {
  int r = s / 2 + 1;
  g.fillCircle(cx - s / 2, cy - s / 4, r, C_ON);
  g.fillCircle(cx + s / 2, cy - s / 4, r, C_ON);
  g.fillTriangle(cx - s - 1, cy - s / 8, cx + s + 1, cy - s / 8, cx, cy + s, C_ON);
}

inline void drawSparkle(Adafruit_GFX &g, int x, int y, int r) {
  if (r <= 0) return;
  g.drawFastHLine(x - r, y, 2 * r + 1, C_ON);
  g.drawFastVLine(x, y - r, 2 * r + 1, C_ON);
  if (r > 2) { g.drawPixel(x - 1, y - 1, C_ON); g.drawPixel(x + 1, y + 1, C_ON);
               g.drawPixel(x + 1, y - 1, C_ON); g.drawPixel(x - 1, y + 1, C_ON); }
}

inline void drawNote(Adafruit_GFX &g, int x, int y) {
  g.fillCircle(x, y, 2, C_ON);
  g.drawFastVLine(x + 2, y - 8, 8, C_ON);
  g.drawLine(x + 2, y - 8, x + 5, y - 5, C_ON);
}

inline void drawBlush(Adafruit_GFX &g, int cx, int y) {
  for (int i = 0; i < 3; i++) g.drawLine(cx - 8 + i * 5, y + 3, cx - 5 + i * 5, y, C_ON);
}

inline void drawTear(Adafruit_GFX &g, int x, int y) {
  g.fillCircle(x, y, 2, C_ON);
  g.fillTriangle(x - 2, y, x + 2, y, x, y - 5, C_ON);
}

// denyut jantung ganda (dag-dig): 0..1
inline float heartPulse(float bp) {
  float u = fmodf(bp * 5.0f, 1.0f);
  if (u < 0.15f) return sinf(u / 0.15f * 3.14159f);
  if (u > 0.25f && u < 0.37f) return 0.6f * sinf((u - 0.25f) / 0.12f * 3.14159f);
  return 0;
}

// ---------- Satu mata ----------
inline void drawEye(Adafruit_GFX &g, int cx, int cy, int w, int h, Expr ex, bool left, uint32_t now,
                    int shx = 0, int shy = 0, int lid = 0) {
  if (h < 3) h = 3;
  if (w < 6) w = 6;
  if (ex == EX_CINTA && h > 8) {  // mata jadi hati yang berdenyut
    int s = 10 + (int)(2 * sinf(tms(now) / 160.0f));
    drawHeart(g, cx, cy, s);
    return;
  }
  if (ex == EX_KAGET && h > EYE_H / 2) {
    g.fillCircle(cx, cy, h / 2 + 3, C_ON);
    return;
  }
  int r = EYE_R;
  if (r > h / 2) r = h / 2;
  if (r > w / 2) r = w / 2;
  int x = cx - w / 2, y = cy - h / 2;
  g.fillRoundRect(x, y, w, h, r, C_ON);
  if (h <= 8) return;  // sedang berkedip: bentuk dasar saja

  int outer = left ? x - 1 : x + w;
  int inner = left ? x + w : x - 1;
  switch (ex) {
    case EX_SENANG:
    case EX_TERTAWA:  // lengkung tersenyum
      g.fillCircle(cx, y + h + 2, (w * 3) / 4, C_OFF);
      break;
    case EX_SEDIH:
    case EX_MENANGIS:  // kelopak turun ke arah luar
      g.fillTriangle(outer, y - 1, inner, y - 1, outer, y + h / 2, C_OFF);
      break;
    case EX_KESAL:  // kelopak turun ke arah dalam
      g.fillTriangle(outer, y - 1, inner, y - 1, inner, y + h / 2, C_OFF);
      break;
    case EX_NGANTUK:  // kelopak atas setengah tertutup
      g.fillRect(x - 1, y - 1, w + 2, h / 2, C_OFF);
      break;
    case EX_KAGUM:  // kilau di mata
      g.fillCircle(cx - w / 5, y + h / 4 + 2, 3, C_OFF);
      g.fillCircle(cx + w / 6, y + h / 2 + 2, 1, C_OFF);
      break;
    default:
      break;
  }
  if ((ex == EX_NORMAL || ex == EX_BINGUNG || ex == EX_MALU) && h > 20 && lid == 0) {  // kilau mata yang ikut menatap
    int gx = cx - w / 4 - shx * 4, gy = y + h / 5 - shy * 2;
    if (gx < x + 3) gx = x + 3;
    if (gx > x + w - 8) gx = x + w - 8;
    if (gy < y + 3) gy = y + 3;
    g.fillRoundRect(gx, gy, 4, 4, 2, C_OFF);
    if (h > 26) g.fillRect(gx + w / 3, gy + h / 3, 2, 2, C_OFF);
  }
  if (lid > 0) g.fillRect(x - 1, y - 1, w + 2, lid + 1, C_OFF);  // kelopak datar (curiga / bosan)
}

// Pola dither 2x2: tingkat abu-abu 0..4 dari 4 piksel
inline bool ditherOn(int x, int y, float cov) {
  static const uint8_t M[4] = {0, 2, 3, 1};
  int n = (int)(cov * 4.0f + 0.5f);
  return M[(y & 1) * 2 + (x & 1)] < n;
}

// Mata dengan posisi/ukuran pecahan. Badan mata digambar utuh pada piksel bulat, lalu baris/kolom
// tepi yang "setengah tertutup" diisi pola dither sesuai pecahannya -> gerak 0,25 piksel terlihat.
inline void drawEyeSub(Adafruit_GFX &g, float cx, float cy, float w, float h, Expr ex, bool left,
                       uint32_t now, int shx, int shy, int lid) {
#if FACE_SUBPIXEL
  if (lid == 0 && h >= 10 && w >= 10 &&
      (ex == EX_NORMAL || ex == EX_BINGUNG || ex == EX_MALU || ex == EX_KAGUM)) {
    float xl = cx - w * 0.5f, xr = cx + w * 0.5f, yt = cy - h * 0.5f, yb = cy + h * 0.5f;
    int x0 = (int)ceilf(xl), x1 = (int)floorf(xr), y0 = (int)ceilf(yt), y1 = (int)floorf(yb);
    int wi = x1 - x0, hi = y1 - y0;
    if (wi >= 6 && hi >= 6) {
      drawEye(g, x0 + wi / 2, y0 + hi / 2, wi, hi, ex, left, now, shx, shy, lid);
      int r = EYE_R;
      if (r > hi / 2) r = hi / 2;
      if (r > wi / 2) r = wi / 2;
      float ct = y0 - yt, cb = yb - y1, cl = x0 - xl, cr = xr - x1;
      for (int x = x0 + r; x < x1 - r; x++) {
        if (ditherOn(x, y0 - 1, ct)) g.drawPixel(x, y0 - 1, C_ON);
        if (ditherOn(x, y1, cb)) g.drawPixel(x, y1, C_ON);
      }
      for (int y = y0 + r; y < y1 - r; y++) {
        if (ditherOn(x0 - 1, y, cl)) g.drawPixel(x0 - 1, y, C_ON);
        if (ditherOn(x1, y, cr)) g.drawPixel(x1, y, C_ON);
      }
      return;
    }
  }
#endif
  drawEye(g, (int)lroundf(cx), (int)lroundf(cy), (int)lroundf(w), (int)lroundf(h), ex, left, now, shx, shy, lid);
}

// Mata spiral (pusing)
inline void drawDizzyEye(Adafruit_GFX &g, int cx, int cy, uint32_t now, bool left) {
  float base = (left ? 1.0f : -1.0f) * (float)(now % 754000) / 120.0f;  // jaga presisi sudut
  for (int i = 0; i < 60; i++) {
    float a = base + i * 0.32f;
    float r = 2 + i * 0.25f;
    g.fillCircle(cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r), 1, C_ON);
  }
}

// ---------- Wajah lengkap ----------
// level: 0..1 kekerasan suara saat ini, idleMs: lama tidak ada aktivitas,
// event/eventAt: tingkah yang dipicu dari luar (BH_PET saat dielus, BH_SHAKE saat alarm).
// ---------------- Efek latar ----------------
// Semua efek digambar SEBELUM mata, dan sebagian besar di pinggir layar, supaya wajah tetap jadi fokus.
inline uint32_t fxHash(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
  return x;
}

// Titik bintang tetap di luar area mata (mata kira-kira x 21..107, y 12..48)
static const uint8_t STAR_XY[][2] = {{4, 4}, {15, 22}, {5, 38}, {12, 58}, {26, 4}, {102, 3},
                                     {118, 21}, {124, 38}, {114, 59}, {64, 2}, {46, 61}, {84, 62}};

inline void fxStars(Adafruit_GFX &g, uint32_t now, int count, int speed) {
  for (int i = 0; i < count; i++) {
    int x = STAR_XY[i][0], y = STAR_XY[i][1];
    int ph = (now / speed + i * 37) % 48;
    if (ph < 3) {  // berkelip terang: bentuk plus
      g.drawPixel(x, y, C_ON);
      g.drawPixel(x - 1, y, C_ON); g.drawPixel(x + 1, y, C_ON);
      g.drawPixel(x, y - 1, C_ON); g.drawPixel(x, y + 1, C_ON);
    } else if (ph < 30) {
      g.drawPixel(x, y, C_ON);
    }
  }
}

inline void fxSunCloud(Adafruit_GFX &g, uint32_t now) {
  int sx = 116, sy = 11;  // matahari kecil dengan sinar berputar
  g.fillCircle(sx, sy, 3, C_ON);
  float rot = tms(now) / 900.0f;
  for (int i = 0; i < 8; i++) {
    float a = rot + i * 0.785f;
    int r1 = 5, r2 = 7 + ((now / 300 + i) % 2);
    g.drawLine(sx + (int)(cosf(a) * r1), sy + (int)(sinf(a) * r1),
               sx + (int)(cosf(a) * r2), sy + (int)(sinf(a) * r2), C_ON);
  }
  int cx = 92 - (int)((now / 90) % 135);  // awan lewat pelan di atas mata, di kiri matahari
  if (cx > -20 && cx < 140) {
    g.fillCircle(cx, 6, 3, C_ON); g.fillCircle(cx + 5, 4, 4, C_ON);
    g.fillCircle(cx + 10, 6, 3, C_ON); g.fillRect(cx, 6, 11, 3, C_ON);
  }
}

inline void fxHeat(Adafruit_GFX &g, uint32_t now) {  // gelombang panas di kedua sisi bawah
  for (int k = 0; k < 2; k++) {
    for (int side = 0; side < 2; side++) {
      int bx = side ? 112 + k * 7 : 4 + k * 7;
      for (int y = 63; y > 46; y--) {
        int x = bx + (int)(sinf(y / 2.5f + tms(now) / 160.0f + k) * 1.6f);
        if ((y + now / 120) % 6 < 4) g.drawPixel(x, y, C_ON);
      }
    }
  }
}

// ---------------- Suasana latar acak saat diam ----------------
// 1 salju, 2 kunang-kunang, 3 hujan kode, 4 gelembung, 5 hujan meteor, 6 kelopak bunga, 7 ombak
inline void fxScene(Adafruit_GFX &g, uint8_t sc, uint32_t now) {
  switch (sc) {
    case 1:  // salju turun, melayang ke kiri-kanan
      for (int i = 0; i < 16; i++) {
        uint32_t h = fxHash(i * 613 + 11);
        int y = (int)((now / (26 + h % 22) + (h >> 10)) % 70) - 3;
        int x = (int)((h >> 3) % 128) + (int)(sinf(tms(now) / 650.0f + i) * 4);
        g.drawPixel(x, y, C_ON);
        if (i % 4 == 0) {
          g.drawPixel(x - 1, y, C_ON); g.drawPixel(x + 1, y, C_ON);
          g.drawPixel(x, y - 1, C_ON); g.drawPixel(x, y + 1, C_ON);
        }
      }
      break;
    case 2:  // kunang-kunang berkelip sambil melayang
      for (int i = 0; i < 8; i++) {
        float a = tms(now) / (1500.0f + i * 260) + i * 1.7f, b = tms(now) / (2100.0f + i * 190) + i * 2.3f;
        int x = 64 + (int)(sinf(a) * (50 + i * 2)), y = 32 + (int)(cosf(b) * 26);
        float glow = sinf(tms(now) / (300.0f + i * 40) + i);
        if (glow > -0.2f) g.drawPixel(x, y, C_ON);
        if (glow > 0.5f) {
          g.drawPixel(x - 1, y, C_ON); g.drawPixel(x + 1, y, C_ON);
          g.drawPixel(x, y - 1, C_ON); g.drawPixel(x, y + 1, C_ON);
        }
      }
      break;
    case 3: {  // hujan kode ala matrix di kedua sisi
      static const uint8_t MX[] = {3, 9, 15, 112, 118, 124};
      for (int i = 0; i < 6; i++) {
        int head = (int)((now / (14 + (i * 5) % 11) + i * 23) % 86) - 8;
        for (int k = 0; k < 7; k++) {
          int y = head - k * 3;
          if (y < 0 || y > 63) continue;
          if (k == 0 || fxHash(i * 97 + k + now / 160) % 3) g.drawPixel(MX[i], y, C_ON);
          if (k == 0) g.drawPixel(MX[i] + 1, y, C_ON);
        }
      }
      break;
    }
    case 4:  // gelembung sabun naik
      for (int i = 0; i < 6; i++) {
        uint32_t h = fxHash(i * 389 + 5);
        int y = 66 - (int)((now / (20 + h % 14) + (h >> 9)) % 80);
        int x = (int)((h >> 4) % 128) + (int)(sinf(tms(now) / 500.0f + i * 2) * 3);
        int r = 1 + h % 3;
        g.drawCircle(x, y, r, C_ON);
        if (r == 3) g.drawPixel(x - 1, y - 1, C_ON);
      }
      break;
    case 5:  // hujan meteor + bintang
      fxStars(g, now, 6, 150);
      for (int i = 0; i < 2; i++) {
        uint32_t cyc = (now + i * 2100) % 4200;
        if (cyc < 800) {
          float p = cyc / 800.0f;
          int hx = 118 - (int)(p * 96) - i * 30, hy = 2 + (int)(p * 52) + i * 4;
          g.drawLine(hx, hy, hx + 18, hy - 9, C_ON);
          g.fillCircle(hx, hy, 1, C_ON);
        }
      }
      break;
    case 6:  // kelopak sakura berguguran
      for (int i = 0; i < 8; i++) {
        uint32_t h = fxHash(i * 727 + 9);
        int y = (int)((now / (30 + h % 16) + (h >> 9)) % 76) - 6;
        int x = (int)((h >> 3) % 140) - y / 4 + (int)(sinf(tms(now) / 400.0f + i) * 5);
        int ph = (now / 180 + i) % 3;
        g.drawPixel(x, y, C_ON);
        if (ph == 0) g.drawPixel(x + 1, y, C_ON);
        else if (ph == 1) g.drawPixel(x, y + 1, C_ON);
        else { g.drawPixel(x + 1, y + 1, C_ON); g.drawPixel(x - 1, y, C_ON); }
      }
      break;
    case 7:  // ombak di dasar layar
      for (int x = 0; x < 128; x += 4) {
        int y1 = 61 + (int)(sinf(x / 7.0f + tms(now) / 260.0f) * 1.6f);
        int y2 = 61 + (int)(sinf((x + 4) / 7.0f + tms(now) / 260.0f) * 1.6f);
        g.drawLine(x, y1, x + 4, y2, C_ON);
        int z1 = 63 + (int)(sinf(x / 5.0f - tms(now) / 330.0f));
        g.drawPixel(x + 2, z1, C_ON);
      }
      break;
    case 8:  // galaksi spiral berputar pelan
      for (int i = 0; i < 26; i++) {
        float a = i * 0.55f + tms(now) / (1800.0f + (i % 5) * 450);
        g.drawPixel(64 + (int)(cosf(a) * (16 + i * 2.4f)), 32 + (int)(sinf(a) * (7 + i * 1.1f)), C_ON);
      }
      break;
    case 9:  // burung terbang melintas (siang)
      for (int i = 0; i < 3; i++) {
        int x = (int)((now / (38 + i * 14) + i * 55) % 170) - 20;
        int y = 7 + i * 6 + (int)(sinf(x / 9.0f) * 2);
        int up = ((now / 150 + i) % 2) ? 1 : -1;
        g.drawLine(x - 3, y + up, x, y, C_ON);
        g.drawLine(x, y, x + 3, y + up, C_ON);
      }
      break;
    case 10:  // kembang api (malam)
      for (int i = 0; i < 2; i++) {
        uint32_t t2 = now + i * 2300, cyc = t2 % 4600, n = t2 / 4600;
        uint32_t h = fxHash(n * 7 + i * 13 + 1);
        int bx = 14 + (int)(h % 100), by = 8 + (int)((h >> 8) % 16);
        if (cyc < 300) {
          g.drawPixel(bx, 62 - (int)(cyc * (62 - by) / 300), C_ON);
        } else if (cyc < 1400) {
          float q = (cyc - 300) / 1100.0f;
          for (int k = 0; k < 10; k++) {
            float a = k * 0.628f;
            int px = bx + (int)(cosf(a) * q * 18), py = by + (int)(sinf(a) * q * 18 + q * q * 6);
            g.drawPixel(px, py, C_ON);
            if (q < 0.7f) g.drawPixel(bx + (int)(cosf(a) * q * 14), by + (int)(sinf(a) * q * 14 + q * q * 4), C_ON);
          }
        }
      }
      break;
    default:
      break;
  }
}

inline void drawBackground(Adafruit_GFX &g, Act act, Expr ex, bool joget, bool sleeping, bool drowsy,
                           float level, uint32_t now, int hour = -1, bool hot = false, uint8_t scene = 0) {
  if (sleeping) {  // langit malam: bulan sabit, bintang, sesekali bintang jatuh
    g.fillCircle(14, 12, 7, C_ON);
    g.fillCircle(18, 9, 6, C_OFF);
    fxStars(g, now, 10, 110);  // 2 bintang terakhir dilewati: tempat jam besar
    uint32_t cyc = now % 9000;
    if (cyc < 700) {
      int p = cyc / 25;  // 0..28
      int x = 40 + p * 2, y = 2 + p;
      if (y < 30) g.drawLine(x, y, x - 6, y - 3, C_ON);
    }
    return;
  }
  if (act == ACT_LISTEN) {  // gelombang suara dari kedua sisi
    for (int k = 0; k < 3; k++) {
      int r = 6 + (int)((now / 35 + k * 9) % 26) + (int)(level * 6);
      g.drawCircle(-6, 30, r, C_ON);
      g.drawCircle(133, 30, r, C_ON);
    }
    return;
  }
  if (act == ACT_THINK) {  // titik mengorbit di pojok kiri atas
    for (int i = 0; i < 3; i++) {
      float a = tms(now) / 220.0f + i * 2.094f;
      g.fillCircle(14 + (int)(cosf(a) * 8), 14 + (int)(sinf(a) * 8), i == 0 ? 2 : 1, C_ON);
    }
    g.drawCircle(14, 14, 2, C_ON);
    return;
  }
  if ((act == ACT_TALK || act == ACT_IDLE) && joget) {  // equalizer di kedua sisi
    for (int i = 0; i < 3; i++) {
      for (int side = 0; side < 2; side++) {
        float k = 0.35f + 0.65f * fabsf(sinf(tms(now) / 110.0f + i * 1.3f + side * 2.1f));
        int h = 2 + (int)((0.3f + level) * 14 * k);
        if (h > 18) h = 18;
        int x = side ? 113 + i * 5 : 2 + i * 5;
        g.fillRect(x, 64 - h, 3, h, C_ON);
      }
    }
    return;
  }
  switch (ex) {
    case EX_CINTA:  // hati melayang naik
      for (int i = 0; i < 4; i++) {
        int ph = (now / 35 + i * 18) % 72;
        int x = (i % 2 ? 116 : 10) + (i > 1 ? (i % 2 ? -6 : 6) : 0) + (int)(sinf((ph + i * 10) / 7.0f) * 3);
        int y = 66 - ph;
        if (y > -4) drawHeart(g, x, y, i > 1 ? 2 : 3);
      }
      break;
    case EX_SENANG:  // kilauan di sudut
      for (int i = 0; i < 4; i++) {
        static const uint8_t P[4][2] = {{7, 8}, {121, 9}, {8, 55}, {120, 54}};
        int tw = (now / 160 + i * 3) % 8;
        if (tw < 5) drawSparkle(g, P[i][0], P[i][1], tw == 2 ? 3 : 1 + (tw % 2));
      }
      break;
    case EX_TERTAWA:  // konfeti berjatuhan
      for (int i = 0; i < 14; i++) {
        uint32_t h = fxHash(i * 7919 + 13);
        int speed = 14 + h % 10;
        int x = (h >> 8) % 128 + (int)(sinf(tms(now) / 300.0f + i) * 3);
        int y = (int)((now / speed + (h >> 16)) % 74) - 6;
        if (i % 3 == 0) g.fillRect(x, y, 2, 2, C_ON);
        else if (i % 3 == 1) g.drawLine(x, y, x + 1, y + 1, C_ON);
        else g.drawPixel(x, y, C_ON);
      }
      break;
    case EX_SEDIH:
    case EX_MENANGIS: {  // hujan miring
      int drops = ex == EX_MENANGIS ? 16 : 9;
      for (int i = 0; i < drops; i++) {
        uint32_t h = fxHash(i * 104729 + 7);
        int y = (int)((now / (9 + h % 5) + (h >> 12)) % 76) - 8;
        int x = (int)(((h >> 4) % 150) - y / 3) % 140;
        g.drawLine(x, y, x - 1, y + 3, C_ON);
      }
      break;
    }
    case EX_KAGET:  // garis kejut dari keempat sudut, berkedip
      if ((now / 120) % 2 == 0) {
        g.drawLine(2, 2, 9, 9, C_ON);   g.drawLine(10, 1, 13, 7, C_ON);  g.drawLine(1, 10, 7, 13, C_ON);
        g.drawLine(125, 2, 118, 9, C_ON); g.drawLine(117, 1, 114, 7, C_ON); g.drawLine(126, 10, 120, 13, C_ON);
        g.drawLine(2, 61, 9, 54, C_ON); g.drawLine(125, 61, 118, 54, C_ON);
      }
      break;
    case EX_KESAL:  // kepulan uap di pojok kanan atas
      for (int i = 0; i < 3; i++) {
        int ph = (now / 60 + i * 9) % 27;
        int r = 1 + ph / 9;
        g.drawCircle(116 + (int)(sinf(ph / 4.0f + i) * 3), 24 - ph, r, C_ON);
      }
      break;
    default:
      if (drowsy) {  // gelembung naik pelan
        for (int i = 0; i < 4; i++) {
          int ph = (now / 70 + i * 17) % 68;
          int x = (i % 2 ? 118 : 9) + (int)(sinf(ph / 9.0f + i) * 3);
          g.drawCircle(x, 66 - ph, 1 + (i % 2), C_ON);
        }
      } else if (act == ACT_IDLE) {
        if (scene > 0) fxScene(g, scene, now);           // suasana acak yang berganti sendiri
        else if (hour >= 6 && hour < 18) fxSunCloud(g, now);  // siang: matahari & awan
        else fxStars(g, now, 10, 140);                   // malam (atau jam belum tersinkron): bintang
        if (hot) fxHeat(g, now);
      }
      break;
  }
}

// Animasi pembuka saat robot menyala: mata terbuka dari garis, berkedip, lalu menyapa.
inline bool drawSplash(Adafruit_GFX &g, uint32_t ms, const char *name) {
  if (ms > 3200) return false;
  float p = ms < 900 ? ms / 900.0f : 1.0f;
  float ease = 1 - (1 - p) * (1 - p) * (1 - p);
  int h = 2 + (int)((EYE_H - 2) * ease);
  if (ms > 1300 && ms < 1450) h = 3;  // kedip sekali
  for (int i = 0; i < 2; i++) {
    int cx = i ? EYE_RX : EYE_LX;
    g.fillRoundRect(cx - EYE_W / 2, EYE_Y - h / 2 - 4, EYE_W, h, h > 2 * EYE_R ? EYE_R : h / 2, C_ON);
  }
  // cincin menyebar di belakang mata
  if (ms < 1100) {
    int r = ms / 12;
    g.drawCircle(64, 28, r, C_ON);
    if (r > 14) g.drawCircle(64, 28, r - 14, C_ON);
  }
  if (ms > 1500) {
    char msg[32];
    snprintf(msg, sizeof(msg), "Halo, aku %s!", name);
    int shown = (ms - 1500) / 60;
    int len = strlen(msg);
    if (shown < len) msg[shown] = 0;
    g.setTextSize(1);
    g.setTextColor(C_ON);
    g.setCursor(64 - len * 3, 55);
    g.print(msg);
  }
  return true;
}

// ---------- Hiasan khusus tiap tingkah (digambar setelah mata) ----------
inline void drawBehaviorOverlay(Adafruit_GFX &g, FaceAnim &f, float bp, uint32_t now,
                                int lx, int rx, int cy, float bounce) {
  g.setTextSize(1);
  g.setTextColor(C_ON);
  switch (f.bh) {
    case BH_COOL: {  // kacamata hitam meluncur dari atas, kilau menyapu, senyum miring
      float slide = bp < 0.2f ? 1 - bp / 0.2f : (bp > 0.82f ? (bp - 0.82f) / 0.18f : 0);
      int y0 = cy - 10 - (int)(slide * 40);
      for (int s = 0; s < 2; s++) {
        int cx = s ? rx : lx;
        g.fillRect(cx - 21, y0, 42, 9, C_ON);
        g.fillRoundRect(cx - 21, y0 + 3, 42, 19, 8, C_ON);
        g.drawLine(cx - 15, y0 + 5, cx - 12, y0 + 10, C_OFF);
        if (bp > 0.32f && bp < 0.62f) {
          int gx = cx - 26 + (int)((bp - 0.32f) / 0.30f * 56);
          if (gx > cx - 18 && gx < cx + 14) g.drawLine(gx + 5, y0 + 4, gx, y0 + 14, C_OFF);
        }
      }
      g.fillRect(lx + 21, y0 + 2, rx - lx - 42, 3, C_ON);
      g.drawLine(lx - 21, y0 + 3, lx - 30, y0 + 1, C_ON);
      g.drawLine(rx + 21, y0 + 3, rx + 30, y0 + 1, C_ON);
      if (slide < 0.2f) {
        g.drawLine(56, 55, 66, 55, C_ON); g.drawLine(66, 55, 72, 51, C_ON);
      }
      break;
    }
    case BH_SCAN: {  // garis pemindai bolak-balik, bidikan di empat sudut
      float ph = bp * 9.42f;
      int x = 64 + (int)(sinf(ph) * 58), dir = cosf(ph) > 0 ? 1 : -1;
      g.drawFastVLine(x, 0, 64, C_ON);
      g.drawFastVLine(x - dir, 0, 64, C_OFF);
      for (int k = 2; k <= 6; k++)
        for (int y = 0; y < 64; y += k + 1) g.drawPixel(x - dir * k * 2, y, C_ON);
      g.drawFastHLine(0, 0, 7, C_ON); g.drawFastVLine(0, 0, 7, C_ON);
      g.drawFastHLine(121, 0, 7, C_ON); g.drawFastVLine(127, 0, 7, C_ON);
      g.drawFastHLine(0, 63, 7, C_ON); g.drawFastVLine(0, 57, 7, C_ON);
      g.drawFastHLine(121, 63, 7, C_ON); g.drawFastVLine(127, 57, 7, C_ON);
      if ((now / 200) % 2) { g.setCursor(52, 55); g.print("SCAN"); }
      break;
    }
    case BH_GLITCH:  // garis noise acak + irisan hitam di mata
      for (int i = 0; i < 6; i++) g.drawFastHLine(f.range(0, 110), f.range(0, 63), f.range(4, 22), C_ON);
      for (int i = 0; i < 3; i++) g.fillRect(f.range(16, 90), f.range(14, 46), f.range(10, 40), f.range(1, 2), C_OFF);
      if (f.range(0, 2) == 0) g.fillRect(f.range(0, 100), f.range(0, 60), f.range(8, 26), f.range(1, 3), C_ON);
      break;
    case BH_STARRY:  // kilau tambahan di kedua sisi
      for (int i = 0; i < 5; i++) {
        uint32_t h = fxHash(f.bhStart * 7 + i * 131);
        int px = (i & 1) ? 110 + (int)(h % 14) : 4 + (int)(h % 14), py = 6 + (int)((h >> 8) % 50);
        int tw = (now / 120 + i * 3) % 6;
        if (tw < 4) drawSparkle(g, px, py, tw == 1 ? 4 : 2);
      }
      break;
    case BH_NOD:
      if (bp > 0.3f && bp < 0.72f) {  // z kecil naik
        g.setCursor(100, 22 - (int)((now / 220) % 3) * 4);
        g.print('z');
      } else if (bp >= 0.72f && bp < 0.92f) {  // kaget bangun
        g.setTextSize(2);
        g.setCursor(113, 6);
        g.print('!');
      }
      break;
    case BH_SUS: {  // titik-titik muncul satu per satu
      int n = (int)(bp * 5);
      if (n > 3) n = 3;
      for (int i = 0; i < n; i++) g.fillRect(56 + i * 8, 56, 2, 2, C_ON);
      break;
    }
    case BH_TONGUE: {
      int wag = (int)(sinf(tms(now) / 90.0f) * 1.5f);
      g.drawLine(53, 52, 57, 56, C_ON); g.drawLine(57, 56, 71, 56, C_ON); g.drawLine(71, 56, 75, 52, C_ON);
      g.fillRoundRect(62 + wag, 56, 9, 7, 3, C_ON);
      g.drawFastVLine(66 + wag, 57, 4, C_OFF);
      break;
    }
    case BH_SQUISH: {  // bayangan menyempit saat melompat
      int sw = 30 + (int)(bounce * 2.5f);
      if (sw > 4) { g.drawFastHLine(64 - sw / 2, 60, sw, C_ON); g.drawFastHLine(64 - sw / 2 + 3, 61, sw - 6, C_ON); }
      break;
    }
    case BH_ROLL:  // mulut datar bosan
      g.drawFastHLine(58, 55, 12, C_ON);
      break;
    case BH_BUBBLE: {
      if (bp < 0.1f) { g.drawCircle(64, 56, 2, C_ON); break; }
      if (bp < 0.72f) {
        float q = (bp - 0.1f) / 0.62f;
        int r = 2 + (int)(q * 8.0f), by = 55 - r;
        g.fillCircle(64, by, r, C_OFF);
        g.drawCircle(64, by, r, C_ON);
        g.drawPixel(64 - r / 2, by - r / 2, C_ON);
        g.drawCircle(64, 57, 1, C_ON);
      } else if (bp < 0.88f) {  // pecah: percikan ke segala arah
        float q = (bp - 0.72f) / 0.16f;
        int r0 = 10 + (int)(q * 4), r1 = r0 + 3 + (int)(q * 5);
        for (int i = 0; i < 8; i++) {
          float a = i * 0.785f;
          g.drawLine(64 + (int)(cosf(a) * r0), 45 + (int)(sinf(a) * r0), 64 + (int)(cosf(a) * r1), 45 + (int)(sinf(a) * r1), C_ON);
        }
      }
      break;
    }
    case BH_GRR:  // mulut bergerigi
      for (int i = 0; i < 6; i++)
        g.drawLine(52 + i * 5, (i & 1) ? 53 : 57, 57 + i * 5, (i & 1) ? 57 : 53, C_ON);
      break;
    case BH_LAUGH: {
      int mh = 7 + (int)(fabsf(sinf(tms(now) / 70.0f)) * 4);
      g.fillCircle(64, 50, mh, C_ON);
      g.fillRect(64 - mh - 1, 50 - mh - 1, 2 * mh + 2, mh + 1, C_OFF);
      break;
    }
    case BH_CAT: {  // telinga, kumis, mulut ω
      int tw = (int)(sinf(tms(now) / 260.0f) * 1.5f);
      g.fillTriangle(26, 11, 31 + tw, 1, 44, 9, C_ON);
      g.fillTriangle(102, 11, 97 - tw, 1, 84, 9, C_ON);
      g.fillTriangle(30, 9, 32 + tw, 4, 38, 8, C_OFF);
      g.fillTriangle(98, 9, 96 - tw, 4, 90, 8, C_OFF);
      g.drawLine(2, 34, 17, 38, C_ON); g.drawLine(2, 41, 17, 41, C_ON); g.drawLine(2, 48, 17, 44, C_ON);
      g.drawLine(126, 34, 111, 38, C_ON); g.drawLine(126, 41, 111, 41, C_ON); g.drawLine(126, 48, 111, 44, C_ON);
      g.drawLine(58, 54, 61, 57, C_ON); g.drawLine(61, 57, 64, 54, C_ON);
      g.drawLine(64, 54, 67, 57, C_ON); g.drawLine(67, 57, 70, 54, C_ON);
      break;
    }
    case BH_LOVE:  // hati meledak keluar dari tengah
      if (bp < 0.92f)
        for (int i = 0; i < 6; i++) {
          float a = i * 1.047f + bp * 2.0f, r = bp * 44.0f;
          drawHeart(g, 64 + (int)(cosf(a) * r * 1.5f), 30 + (int)(sinf(a) * r * 0.8f), 2);
        }
      g.drawLine(58, 55, 62, 58, C_ON); g.drawLine(62, 58, 66, 58, C_ON); g.drawLine(66, 58, 70, 55, C_ON);
      break;
    case BH_SCARE:  // keringat dingin + mulut bergetar
      drawTear(g, rx + 19, cy - 14 + (int)((now / 50) % 12));
      for (int i = 0; i < 5; i++)
        g.drawLine(54 + i * 5, (i & 1) ? 58 : 55, 59 + i * 5, (i & 1) ? 55 : 58, C_ON);
      break;
    case BH_DREAM: {  // gelembung pikiran berisi ikon
      if (bp < 0.1f) break;
      g.fillRoundRect(99, 0, 29, 21, 9, C_OFF);
      g.drawRoundRect(99, 0, 29, 21, 9, C_ON);
      int icon = (f.bhStart >> 3) % 3, ix = 113, iy = 10;
      if (icon == 0) drawHeart(g, ix, iy - 1, 3 + (int)(sinf(tms(now) / 150.0f) * 1.2f));
      else if (icon == 1) drawSparkle(g, ix, iy, 3 + (int)((now / 180) % 2));
      else drawNote(g, ix - 2, iy + 4);
      break;
    }
    case BH_BROW: {  // satu alis naik
      float raise = sinf(bp * 3.14159f) * 4.0f;
      int top = cy - EYE_H / 2 - 7;
      for (int t = 0; t < 2; t++) {
        g.drawLine(lx - 12, top + 4 + t, lx + 10, top + 2 + t, C_ON);
        g.drawLine(rx - 12, top + 3 + t - (int)(raise / 2), rx, top + t - (int)raise, C_ON);
        g.drawLine(rx, top + t - (int)raise, rx + 12, top + 3 + t - (int)(raise / 2), C_ON);
      }
      break;
    }
    case BH_HICCUP: {
      float sv = fabsf(sinf(bp * 9.42f));
      if (bp < 0.7f && sv > 0.75f) { g.setCursor(94, 54); g.print("hik!"); }
      if (sv > 0.6f) g.drawCircle(64, 56, 2, C_ON);
      break;
    }
    case BH_SHIVER:
      fxScene(g, 1, now);
      if ((now / 150) % 2) { g.setCursor(4, 56); g.print("brr"); }
      for (int i = 0; i < 4; i++) g.drawLine(56 + i * 4, (i & 1) ? 57 : 55, 60 + i * 4, (i & 1) ? 55 : 57, C_ON);
      break;
    case BH_HOT: {
      fxHeat(g, now);
      drawTear(g, lx - 17, cy - 12 + (int)((now / 45) % 18));
      drawTear(g, rx + 17, cy - 12 + (int)(((now / 45) + 9) % 18));
      int mh = 3 + (int)(fabsf(sinf(tms(now) / 140.0f)) * 3);
      g.fillRoundRect(59, 54, 10, mh, 2, C_ON);
      break;
    }
    case BH_HUNGRY: {
      int by = (int)(sinf(tms(now) / 200.0f) * 1.0f);
      g.fillCircle(64, 40 + by, 6, C_OFF);
      g.fillTriangle(58, 44 + by, 70, 44 + by, 64, 59 + by, C_OFF);
      g.fillTriangle(59, 45 + by, 69, 45 + by, 64, 58 + by, C_ON);
      g.fillCircle(64, 40 + by, 5, C_ON);
      g.drawLine(61, 47 + by, 65, 55 + by, C_OFF);
      g.drawLine(66, 47 + by, 62, 53 + by, C_OFF);
      g.drawPixel(62, 38 + by, C_OFF); g.drawPixel(63, 38 + by, C_OFF);
      break;
    }
    case BH_IDEA:
      if (bp > 0.1f) {
        g.fillCircle(64, 8, 6, C_OFF);
        g.fillCircle(64, 7, 4, C_ON);
        g.fillRect(62, 11, 5, 3, C_ON);
        if ((now / 110) % 2)
          for (int i = 0; i < 5; i++) {
            float a = -3.14159f + i * 0.785f;
            g.drawLine(64 + (int)(cosf(a) * 7), 7 + (int)(sinf(a) * 7), 64 + (int)(cosf(a) * 10), 7 + (int)(sinf(a) * 10), C_ON);
          }
      }
      break;
    case BH_UFO: {
      int ux = (int)(-12 + bp * 152), uy = 8 + (int)(sinf(bp * 12.0f) * 3);
      g.fillRect(ux - 12, uy - 7, 24, 16, C_OFF);
      g.fillCircle(ux, uy - 1, 4, C_ON);
      g.fillRoundRect(ux - 9, uy, 18, 5, 2, C_ON);
      g.drawPixel(ux - 4, uy + 2, C_OFF); g.drawPixel(ux, uy + 2, C_OFF); g.drawPixel(ux + 4, uy + 2, C_OFF);
      if (bp > 0.3f && bp < 0.6f)
        for (int y = uy + 6; y < 63; y += 2) {
          int sp = (y - uy - 5) / 3;
          g.drawPixel(ux - 2 - sp, y, C_ON); g.drawPixel(ux + 2 + sp, y, C_ON);
        }
      break;
    }
    case BH_HEARTBEAT:
      drawHeart(g, 64, 32, 3 + (int)(heartPulse(bp) * 3));
      break;
    case BH_POKE:
      if (bp > 0.15f && bp < 0.8f) {
        g.setTextSize(2); g.setCursor(113, 6); g.print('!');
        g.drawCircle(64, 56, 2, C_ON);
      }
      break;
    case BH_PIXEL: {
      float p = sinf(bp * 3.14159f);
      for (int by = 8; by < 52; by += 4)
        for (int bx = 18; bx < 112; bx += 4) {
          uint32_t h = fxHash(by * 131 + bx * 7 + (now / 80) * 7919);
          if ((int)(h % 100) < (int)(p * 85)) g.fillRect(bx, by, 4, 4, C_OFF);
          else if (p > 0.3f && h % 97 == 0) g.fillRect(bx, by, 4, 4, C_ON);
        }
      break;
    }
    case BH_LOADING:
      g.drawRect(34, 54, 60, 7, C_ON);
      g.fillRect(36, 56, (int)(bp * 56), 3, C_ON);
      if (bp > 0.93f) { g.drawLine(100, 56, 103, 59, C_ON); g.drawLine(103, 59, 109, 52, C_ON); }
      break;
    case BH_JAM: {
      int by = (int)bounce;
      for (int k = 0; k < 24; k++) {
        float a1 = 3.14159f + k * (3.14159f / 24), a2 = a1 + 3.14159f / 24;
        for (int t = 0; t < 2; t++)
          g.drawLine(64 + (int)(cosf(a1) * 58), 26 + (int)(sinf(a1) * 26) + by + t,
                     64 + (int)(cosf(a2) * 58), 26 + (int)(sinf(a2) * 26) + by + t, C_ON);
      }
      g.fillRoundRect(1, 22 + by, 10, 22, 3, C_ON);
      g.fillRoundRect(117, 22 + by, 10, 22, 3, C_ON);
      for (int i = 0; i < 2; i++) {
        int ph = (int)((now / 32 + i * 30) % 60), ny = 60 - ph;
        if (ny > 14) drawNote(g, (i ? 112 : 14) + (int)(sinf(ph / 8.0f) * 3), ny);
      }
      break;
    }
    case BH_HELLO:
      g.setTextSize(2);
      g.setCursor(34, 49 + (int)(sinf(tms(now) / 160.0f) * 1.5f));
      g.print("HALO!");
      break;
    case BH_FLUTTER:
      for (int k = 0; k < 3; k++) {
        int a = 14 + k * 3, b2 = 20 + k * 3 + (k ? 1 : 0);
        g.drawLine(lx - 17 + k, cy - 14 + k * 4, lx - 24 - k, cy - 20 + k * 5 - (k == 2 ? 2 : 0), C_ON);
        g.drawLine(rx + 17 - k, cy - 14 + k * 4, rx + 24 + k, cy - 20 + k * 5 - (k == 2 ? 2 : 0), C_ON);
        (void)a; (void)b2;
      }
      g.drawLine(58, 54, 61, 57, C_ON); g.drawLine(61, 57, 67, 57, C_ON); g.drawLine(67, 57, 70, 54, C_ON);
      break;
    case BH_WINKHEART:
      if (bp > 0.3f && bp < 0.92f) {
        float q = (bp - 0.3f) / 0.62f;
        drawHeart(g, rx + 20 + (int)(sinf(q * 9) * 3), cy - 4 - (int)(q * 36), 3);
      }
      g.drawLine(58, 54, 61, 57, C_ON); g.drawLine(61, 57, 67, 57, C_ON); g.drawLine(67, 57, 70, 54, C_ON);
      break;
    case BH_CONFETTI:
      for (int k = 0; k < 26; k++) {
        uint32_t h = fxHash(k * 977 + 5);
        int x = (int)(h % 126) + 1;
        int sp = 18 + (int)((h >> 8) % 22);
        int y = (int)(((now / sp) + (h >> 12)) % 72) - 6;
        int sway = (int)(sinf(tms(now) / 230.0f + k) * 2);
        if ((h >> 4) & 1) g.fillRect(x + sway, y, 3, 2, C_ON);
        else g.drawLine(x + sway, y, x + sway + 2, y + 2, C_ON);
      }
      break;
    case BH_DISCO: {
      for (int k = 0; k < 5; k++) {
        int x2 = 64 + (int)(sinf(tms(now) / 330.0f + k * 1.3f) * 66);
        g.drawLine(64, 6, x2, 63, C_ON);
        g.drawLine(65, 6, x2 + 1, 63, C_ON);
      }
      g.fillCircle(64, 5, 6, C_OFF);
      g.drawCircle(64, 5, 6, C_ON);
      g.drawLine(58, 5, 70, 5, C_ON);
      int ph = (int)((now / 120) % 4);
      g.drawLine(64, 0, 64, -1, C_ON);
      g.drawPixel(60 + ph * 3, 3, C_ON);
      g.drawPixel(66 - ph * 2, 8, C_ON);
      if ((now / 220) & 1) { drawNote(g, 8, 44); drawNote(g, 116, 36); } else { drawNote(g, 14, 38); drawNote(g, 110, 44); }
      break;
    }
    case BH_EQ:
      for (int i = 0; i < 14; i++) {
        int hh = 3 + (int)((sinf(tms(now) / 85.0f + i * 1.7f) + sinf(tms(now) / 140.0f + i * 0.6f) + 2) * 3.2f);
        g.fillRect(6 + i * 8, 63 - hh, 6, hh, C_ON);
        g.drawFastHLine(6 + i * 8, 63 - hh - 3 - (int)(sinf(tms(now) / 300.0f + i) * 1.5f), 6, C_ON);
      }
      break;
    case BH_ROCKET: {
      int x = -8 + (int)(bp * 144), y = 58 - (int)(bp * 54);
      for (int k = 1; k < 5; k++) g.drawCircle(x - k * 7, y + k * 5 + (int)(sinf(k + tms(now) / 90.0f) * 1.5f), 1 + k / 2, C_ON);
      g.fillTriangle(x, y - 9, x - 3, y - 2, x + 3, y - 2, C_ON);
      g.fillRect(x - 3, y - 2, 7, 8, C_ON);
      g.fillTriangle(x - 3, y + 6, x - 7, y + 9, x - 3, y + 2, C_ON);
      g.fillTriangle(x + 4, y + 6, x + 8, y + 9, x + 4, y + 2, C_ON);
      g.fillCircle(x, y + 1, 1, C_OFF);
      int fl = 4 + (int)((now / 50) % 4);
      g.fillTriangle(x - 2, y + 6, x + 3, y + 6, x, y + 6 + fl + 3, C_ON);
      break;
    }
    case BH_RAIN: {
      g.fillCircle(38, 7, 7, C_ON); g.fillCircle(50, 4, 8, C_ON); g.fillCircle(63, 6, 8, C_ON);
      g.fillCircle(76, 7, 7, C_ON); g.fillCircle(88, 8, 5, C_ON); g.fillRect(36, 8, 54, 6, C_ON);
      for (int k = 0; k < 16; k++) {
        uint32_t h = fxHash(k * 311 + 9);
        int x = 32 + (int)(h % 62);
        int y = 16 + (int)(((now / 11) + (h >> 9)) % 46);
        g.drawLine(x, y, x - 1, y + 3, C_ON);
      }
      break;
    }
    case BH_CHECK: {
      float q = smooth01(bp / 0.5f);
      g.drawCircle(64, 53, 3 + (int)(q * 7), C_ON);
      if (bp > 0.25f) {
        float c = smooth01((bp - 0.25f) / 0.35f);
        int x1 = 58, y1 = 53, x2 = 62, y2 = 57, x3 = 71, y3 = 48;
        if (c < 0.4f) { float u = c / 0.4f; g.drawLine(x1, y1, x1 + (int)((x2 - x1) * u), y1 + (int)((y2 - y1) * u), C_ON); }
        else { g.drawLine(x1, y1, x2, y2, C_ON); float u = (c - 0.4f) / 0.6f; g.drawLine(x2, y2, x2 + (int)((x3 - x2) * u), y2 + (int)((y3 - y2) * u), C_ON); }
      }
      break;
    }
    case BH_WOW: {
      float pulse = 3 + sinf(tms(now) / 70.0f) * 2;
      for (int k = 0; k < 14; k++) {
        float a = k * 0.4488f;
        int r1 = 33 + (int)pulse, r2 = r1 + 7;
        g.drawLine(64 + (int)(cosf(a) * r1 * 1.4f), 30 + (int)(sinf(a) * r1 * 0.9f),
                   64 + (int)(cosf(a) * r2 * 1.5f), 30 + (int)(sinf(a) * r2 * 0.95f), C_ON);
      }
      g.setTextSize(1); g.setCursor(52, 55); g.print("WOW!");
      break;
    }
    case BH_ZZZ:
      for (int k = 0; k < 3; k++) {
        float q = fmodf(bp * 2.2f + k * 0.33f, 1.0f);
        g.setTextSize(k == 0 ? 1 : 2);
        g.setCursor(96 + (int)(q * 22) + (int)(sinf(q * 8) * 2), 46 - (int)(q * 42));
        g.print('Z');
      }
      break;
    case BH_RIPPLE:
      for (int k = 0; k < 3; k++) {
        float q = bp * 1.5f - k * 0.18f;
        if (q > 0 && q < 1) g.drawCircle(64, 32, (int)(q * 72), C_ON);
      }
      break;
    case BH_BOLT: {
      bool on = ((now / 140) & 1) || bp > 0.7f;
      if (on) {
        g.fillTriangle(68, 44, 59, 56, 65, 56, C_ON);
        g.fillTriangle(62, 63, 70, 51, 63, 51, C_ON);
      }
      for (int k = 0; k < 4; k++) {
        uint32_t h = fxHash(k * 53 + now / 110);
        g.drawPixel(52 + (int)(h % 24), 46 + (int)((h >> 8) % 18), C_ON);
      }
      break;
    }
    case BH_KISS:
      if (bp < 0.8f) {
        g.fillCircle(62, 56, 2, C_ON); g.fillCircle(66, 56, 2, C_ON);
        g.drawPixel(64, 56, C_OFF);
      }
      for (int k = 0; k < 3; k++) {
        float q = bp * 1.6f - k * 0.22f;
        if (q > 0 && q < 1) drawHeart(g, 64 + (int)(sinf(q * 7 + k) * 10) + (k - 1) * 8, 52 - (int)(q * 48), 2 + (k == 1));
      }
      break;
    default:
      break;
  }
}

inline void drawFace(Adafruit_GFX &g, FaceAnim &f, Act act, Expr ex, bool joget, float level,
                     uint32_t now, uint32_t idleMs, uint8_t event = BH_NONE, uint32_t eventAt = 0,
                     bool fx = true, int hour = -1, bool hot = false) {
  // lama frame ini -> faktor pegas (dibatasi supaya tetap stabil kalau ada jeda panjang)
  float dtMs = f.lastFrame ? (float)(now - f.lastFrame) : 40.0f;
  f.lastFrame = now;
  gSpringF = fminf(fmaxf(dtMs / 40.0f, 0.05f), 2.0f);

  if (!f.ready) {  // pertama kali: mata mulai tertutup lalu terbuka memantul
    f.lw.x = f.rw.x = EYE_W;
    f.lh.x = f.rh.x = 2;
    f.ready = true;
  }

  // ---- Kejadian dari luar ----
  if (eventAt && eventAt != f.seenEvent) {
    f.seenEvent = eventAt;
    f.bh = (Behav)event;
    f.bhStart = now;
    f.bhDur = BH_INFO[event].dur ? BH_INFO[event].dur : 1400;
  }
  if (f.bh != BH_NONE && now - f.bhStart > f.bhDur) f.bh = BH_NONE;
  bool external = f.bh == BH_PET || f.bh == BH_SHAKE;
  if (act != ACT_IDLE && !external) f.bh = BH_NONE;  // tingkah iseng berhenti saat ada kegiatan

  bool sleeping = act == ACT_IDLE && idleMs > 120000 && !external;
  bool drowsy = act == ACT_IDLE && idleMs > 60000 && !sleeping;

  // ---- Pilih tingkah acak saat diam (berbobot, tidak mengulang yang barusan) ----
  if (act == ACT_IDLE && !sleeping && !joget && f.bh == BH_NONE && now >= f.nextBh) {
    Behav pick = BH_DBLINK;
    if (drowsy) {
      static const Behav D[] = {BH_YAWN, BH_YAWN, BH_LOOKDOWN, BH_NOD, BH_SLOWBLINK};
      pick = D[f.range(0, 4)];
    } else {
      int total = 0;
      for (int i = 1; i < BH_COUNT; i++) if (i != f.lastBh) total += BH_INFO[i].w;
      int r = f.range(0, total - 1);
      for (int i = 1; i < BH_COUNT; i++) {
        if (i == f.lastBh) continue;
        if (r < BH_INFO[i].w) { pick = (Behav)i; break; }
        r -= BH_INFO[i].w;
      }
    }
    f.bh = pick;
    f.lastBh = pick;
    f.bhStart = now;
    f.bhDur = BH_INFO[pick].dur;
    bool chain = !drowsy && f.range(0, 3) == 0;  // kadang disambung langsung dengan tingkah berikutnya
    f.nextBh = now + f.bhDur + (chain ? f.range(200, 700) : f.range(drowsy ? 5000 : 1200, drowsy ? 10000 : 4500));
  }
  if (act == ACT_IDLE && f.bh == BH_NONE && !sleeping && now >= f.mouthAt) {  // senyum kecil sesekali
    f.mouthEnd = now + f.range(1000, 1800);
    f.mouthAt = now + f.range(7000, 15000);
  }
  // suasana latar ikut berganti sendiri
  if (act == ACT_IDLE && !sleeping && !drowsy && now >= f.sceneAt) {
    static const uint8_t DAY[] = {0, 0, 1, 4, 6, 7, 9};    // matahari, salju, gelembung, kelopak, ombak, burung
    static const uint8_t NIGHT[] = {0, 0, 2, 3, 5, 1, 8, 10};  // + galaksi, kembang api
    f.scene = (hour >= 6 && hour < 18) ? DAY[f.range(0, 6)] : NIGHT[f.range(0, 7)];
    f.sceneAt = now + f.range(15000, 35000);
  }
  float bp = f.bh != BH_NONE ? (float)(now - f.bhStart) / f.bhDur : 0;  // 0..1 progres tingkah

  // ---- Ekspresi akhir ----
  if (act == ACT_IDLE) {
    ex = EX_NORMAL;
    if (drowsy) ex = EX_NGANTUK;
    if (f.bh == BH_SMILE || joget) ex = EX_SENANG;  // berjoget = senang
  }
  if (act == ACT_LISTEN || act == ACT_THINK) ex = EX_NORMAL;
  if (f.bh == BH_PET) ex = bp < 0.5f ? EX_CINTA : EX_MALU;
  if (f.bh == BH_SHAKE) ex = EX_KAGET;
  if (f.bh == BH_HUM) ex = EX_SENANG;
  if (act == ACT_IDLE && !joget) {  // tingkah baru yang mengubah ekspresi
    switch (f.bh) {
      case BH_STARRY: case BH_HUNGRY: ex = EX_KAGUM; break;
      case BH_HEARTBEAT: ex = EX_MALU; break;
      case BH_JAM: case BH_HELLO: ex = EX_SENANG; break;
      case BH_CONFETTI: case BH_DISCO: case BH_EQ: case BH_ROCKET: case BH_CHECK: ex = EX_SENANG; break;
      case BH_RAIN: ex = EX_SEDIH; break;
      case BH_WOW: case BH_BOLT: ex = EX_KAGUM; break;
      case BH_ZZZ: ex = EX_NGANTUK; break;
      case BH_KISS: ex = EX_CINTA; break;
      case BH_TONGUE: case BH_CAT: ex = EX_SENANG; break;
      case BH_LAUGH: ex = EX_TERTAWA; break;
      case BH_SHY: ex = EX_MALU; break;
      case BH_GRR: ex = EX_KESAL; break;
      case BH_LOVE: ex = EX_CINTA; break;
      case BH_SQUISH: if (bp > 0.5f) ex = EX_SENANG; break;
      default: break;
    }
  }
  if (ex != f.lastEx) {  // catat pergantian ekspresi untuk efek "pop" (hanya dari jawaban, bukan tingkah iseng)
    f.lastEx = ex;
    if (act != ACT_IDLE && f.bh == BH_NONE) f.exChangeAt = now;
  }
  // posisi lalat (tingkah mengejar lalat)
  float flyX = 64, flyY = 30;
  if (f.bh == BH_FLY) {
    flyX = 64 + sinf(bp * 9.42f) * 54 + sinf(bp * 23.0f) * 6;
    flyY = 28 + sinf(bp * 14.4f) * 20;
  }

  // ---- Arah lirikan ----
  float tx = 0, ty = 0;
  if (act == ACT_IDLE) {
    if (now >= f.nextLook) {
      bool center = f.range(0, 3) == 0;
      float nlx = center ? 0 : f.range(-10, 10);
      if (fabsf(nlx - f.idleLookX) > 8 && f.range(0, 2) == 0) f.nextBlink = now + 90;  // lirikan jauh sering disusul kedip
      f.idleLookX = nlx;
      f.idleLookY = center ? 0 : f.range(-5, 5);
      f.nextLook = now + f.range(1200, 3500);
    }
    tx = f.idleLookX;
    ty = f.idleLookY;
    if (f.bh == BH_GLANCE) { tx = (f.bhStart / 7) % 2 ? 16 : -16; ty = -2; }
    if (f.bh == BH_LOOKDOWN || drowsy) { tx = tx * 0.3f; ty = 7; }
    if (f.bh == BH_WIGGLE) tx = sinf(tms(now) / 110.0f) * 9;
    if (f.bh == BH_PET) { tx = 0; ty = 2; }
    if (f.bh == BH_FLY) { tx = (flyX - 64) / 3.5f; ty = (flyY - 30) / 3.0f; }
    if (f.bh == BH_HUM) { tx = sinf(tms(now) / 380.0f) * 6; ty = -1; }
    if (f.bh == BH_PEEK) tx = (bp > 0.08f && bp < 0.55f) ? 78 : 0;
    if (f.bh == BH_SNEEZE) { tx = 0; ty = bp < 0.55f ? -5 * bp / 0.55f : (bp < 0.7f ? 7 : 0); }
    if (f.bh == BH_DIZZY) { tx = sinf(tms(now) / 260.0f) * 5; ty = cosf(tms(now) / 330.0f) * 3; }
    switch (f.bh) {
      case BH_SPIN: { float a = bp * 12.566f; tx = cosf(a) * 11; ty = sinf(a) * 6; break; }
      case BH_ROLL: { float a = bp * 6.283f - 2.2f; tx = cosf(a) * 10; ty = sinf(a) * 6; break; }
      case BH_SCAN: tx = sinf(bp * 9.42f) * 15; ty = 0; break;
      case BH_SUS: tx = bp < 0.45f ? 13 : (bp < 0.55f ? 0 : -13); ty = 1; break;
      case BH_DREAM: tx = 9; ty = -8; break;
      case BH_SHY: tx = (bp < 0.55f || bp > 0.75f) ? -14 : 3; ty = 4; break;
      case BH_SCARE: tx = sinf(tms(now) / 25.0f) * 2; ty = 0; break;
      case BH_BUBBLE: tx = 0; ty = 3; break;
      case BH_CAT: tx = sinf(tms(now) / 500.0f) * 4; ty = 0; break;
      case BH_BROW: tx = 0; ty = -1; break;
      case BH_STARRY: ty = -2; break;
      case BH_COOL: case BH_CROSS: case BH_TONGUE: case BH_LOVE: case BH_GRR: case BH_LAUGH: tx = ty = 0; break;
      case BH_HUNGRY: tx = 0; ty = 5; break;
      case BH_IDEA: tx = 0; ty = -4; break;
      case BH_UFO: { float ux = -12 + bp * 152; tx = fminf(fmaxf((ux - 64) / 4.5f, -14.0f), 14.0f); ty = -4; break; }
      case BH_HOT: tx = 0; ty = 2; break;
      case BH_LOADING: tx = sinf(tms(now) / 200.0f) * 4; ty = 0; break;
      case BH_JAM: tx = sinf(tms(now) / 300.0f) * 5; ty = 0; break;
      case BH_WINKHEART: tx = -4; ty = 0; break;
      case BH_HICCUP: case BH_SHIVER: case BH_HEARTBEAT: case BH_POKE: case BH_HELLO: case BH_FLUTTER: tx = ty = 0; break;
      case BH_CONFETTI: case BH_DISCO: case BH_EQ: case BH_CHECK: case BH_WOW: case BH_RIPPLE: case BH_BOLT: case BH_KISS: case BH_ZZZ: tx = ty = 0; break;
      case BH_RAIN: tx = 0; ty = 3; break;
      case BH_ROCKET: { float rx2 = -10 + bp * 150; tx = fminf(fmaxf((rx2 - 64) / 4.5f, -14.0f), 14.0f); ty = -3; break; }
      default: break;
    }
    if (f.bh == BH_NONE) {  // gerak mikro bola mata, supaya tatapan tidak pernah kaku
      if (now >= f.microAt) {
        f.microX = f.range(-2, 2) * 0.5f;
        f.microY = f.range(-1, 1) * 0.5f;
        f.microAt = now + f.range(250, 700);
      }
      tx += f.microX; ty += f.microY;
    }
  } else if (act == ACT_LISTEN) {
    tx = 0; ty = -3;
  } else if (act == ACT_THINK) {
    tx = 9 + sinf(tms(now) / 600.0f) * 2; ty = -7;
  } else if (act == ACT_TALK) {
    tx = sinf(tms(now) / 700.0f) * 3;
    ty = ex == EX_MALU ? 4 : 0;
  }
  if (ex == EX_MALU && act != ACT_TALK) ty = 4;

  // ---- Ukuran mata target ----
  float lW = EYE_W, rW = EYE_W, lH = EYE_H, rH = EYE_H;
  if (act == ACT_LISTEN) { lH = rH = EYE_H * 1.1f; }
  if (act == ACT_THINK) { lH = rH = EYE_H * 0.75f; }
  switch (ex) {
    case EX_KAGET: lH = rH = EYE_H * 1.15f; break;
    case EX_TERTAWA: lH = rH = EYE_H * 0.75f; break;
    case EX_KESAL: lH = rH = EYE_H * 0.8f; break;
    case EX_MALU: lH = rH = EYE_H * 0.8f; break;
    case EX_BINGUNG: rH = EYE_H * 0.55f; break;
    case EX_KAGUM: lW = rW = EYE_W * 1.08f; lH = rH = EYE_H * 1.1f; break;
    default: break;
  }
  if (f.bh == BH_CURIOUS) { lH = EYE_H * 1.18f; lW = EYE_W * 1.1f; rH = EYE_H * 0.7f; }
  if (f.bh == BH_YAWN && bp > 0.15f && bp < 0.8f) { lH = rH = 5; lW = rW = EYE_W + 4; }
  if (sleeping) { lH = rH = 4; }
  if (f.bh == BH_SNEEZE && bp < 0.55f) { float q = 1 - 0.6f * bp / 0.55f; lH *= q; rH *= q; }
  if (f.bh == BH_SNEEZE && bp >= 0.55f && bp < 0.8f) { lH = rH = 4; lW = rW = EYE_W + 4; }  // terpejam rapat
  float nodSink = 0;  // seberapa jauh kepala terkulai (BH_NOD)
  float pulse = f.bh == BH_HEARTBEAT ? heartPulse(bp) : 0;
  switch (f.bh) {
    case BH_SCAN: lH = rH = EYE_H * 0.42f; lW = rW = EYE_W * 1.1f; break;
    case BH_SUS: lH = rH = EYE_H * 0.62f; break;
    case BH_ROLL: lH = rH = EYE_H * 0.7f; break;
    case BH_BUBBLE: lH = rH = EYE_H * 0.85f; break;
    case BH_IDEA: lH = rH = EYE_H * 1.1f; break;
    case BH_UFO: lH = rH = EYE_H * 1.1f; lW = rW = EYE_W * 1.05f; break;
    case BH_HOT: lH = rH = EYE_H * 0.75f; break;
    case BH_SHIVER: lH = rH = EYE_H * 0.85f; break;
    case BH_LOADING: lH = rH = EYE_H * 0.5f; break;
    case BH_POKE: if (bp < 0.15f) { lH = rH = EYE_H * 1.15f; } break;
    case BH_HEARTBEAT: lW = rW = EYE_W * (0.8f + 0.12f * pulse); lH = rH = EYE_H * (0.8f + 0.14f * pulse); break;
    case BH_MORPH: {
      float q = bp * 12.566f;
      lW = EYE_W * (0.7f + 0.35f * sinf(q));        lH = EYE_H * (0.8f + 0.35f * cosf(q));
      rW = EYE_W * (0.7f + 0.35f * sinf(q + 1.5f)); rH = EYE_H * (0.8f + 0.35f * cosf(q + 1.5f));
      break;
    }
    case BH_SQUISH: {
      float q = sinf(bp * 12.566f);  // + = memipih saat mendarat, - = memanjang di udara
      lW = rW = EYE_W * (1 + 0.16f * q); lH = rH = EYE_H * (1 - 0.24f * q);
      break;
    }
    case BH_NOD:
      if (bp < 0.72f) {
        float q = bp / 0.72f;
        nodSink = q + 0.1f * sinf(bp * 28.0f) * q;
        nodSink = fminf(fmaxf(nodSink, 0.0f), 1.0f);
        lH = rH = EYE_H * (1 - 0.88f * nodSink);
      } else if (bp < 0.9f) { lH = rH = EYE_H * 1.2f; lW = rW = EYE_W * 1.08f; }
      break;
    default: break;
  }
  if (act == ACT_IDLE && !sleeping) {  // perspektif: mata di sisi yang dituju sedikit menyempit
    float ax = fabsf(tx);
    if (tx > 0) { rW *= 1 - 0.012f * ax; lW *= 1 + 0.004f * ax; }
    else        { lW *= 1 - 0.012f * ax; rW *= 1 + 0.004f * ax; }
  }
  // napas pelan saat diam
  if (act == ACT_IDLE && !sleeping) { float b = sinf(tms(now) / 1400.0f) * 1.2f; lH += b; rH += b; }

  if (!sleeping && faceNear > 0.02f) {  // penasaran: mata membesar & sedikit menatap ke atas
    float k = faceNear;
    lW *= 1 + 0.10f * k; rW *= 1 + 0.10f * k; lH *= 1 + 0.16f * k; rH *= 1 + 0.16f * k;
  }
  f.lw.step(lW); f.rw.step(rW); f.lh.step(lH); f.rh.step(rH);
  if (!sleeping) { tx += faceSteerX; ty += faceSteerY; }  // mata mengikuti arah roda
  f.lookX.step(tx, 0.25f, 0.6f); f.lookY.step(ty, 0.25f, 0.6f);

  // ---- Kedip (langsung, tanpa pegas, supaya cepat) ----
  if (now >= f.nextBlink) {
    f.blinkStart = now;
    f.nextBlink = now + f.range(2200, 6000);
  }
  float openL = 1, openR = 1;
  auto blinkAt = [&](uint32_t start) {
    float p = (now - start) / 170.0f;  // menutup cepat (~65 ms), membuka sedikit lebih pelan, keduanya halus
    if (p >= 1.0f) return 1.0f;
    float o = p < 0.38f ? 1.0f - smooth01(p / 0.38f) : smooth01((p - 0.38f) / 0.62f);
    return fmaxf(0.08f, o);
  };
  bool canBlink = !sleeping && ex != EX_CINTA && f.bh != BH_YAWN && f.bh != BH_DIZZY && act != ACT_THINK &&
                  f.bh != BH_SCARE && f.bh != BH_GLITCH && f.bh != BH_NOD && f.bh != BH_SLOWBLINK &&
                  f.bh != BH_SCAN && f.bh != BH_COOL && f.bh != BH_POKE && f.bh != BH_FLUTTER;
  if (canBlink) {
    openL = openR = blinkAt(f.blinkStart);
    if (f.bh == BH_DBLINK) openL = openR = fminf(blinkAt(f.bhStart), blinkAt(f.bhStart + 260));
    if ((f.bh == BH_WINK || f.bh == BH_WINKHEART) && bp > 0.2f && bp < 0.75f) openR = 0.08f;  // kedip sebelah
  }
  if (f.bh == BH_SLOWBLINK && !sleeping) {  // kedip lambat: menutup, bertahan, lalu membuka pelan
    openL = openR = fmaxf(0.08f, powf(fabsf(cosf(bp * 3.14159f)), 2.5f));
  }
  if (f.bh == BH_FLUTTER && !sleeping) {  // kedip beruntun lima kali
    float o = 1;
    for (int k = 0; k < 5; k++) o = fminf(o, blinkAt(f.bhStart + k * 190));
    openL = openR = o;
  }

  // ---- Posisi ----
  float bounce = 0, shakeX = 0;
  if ((act == ACT_TALK || act == ACT_IDLE) && joget) bounce = sinf(tms(now) / 140.0f) * 2.0f - level * 5.0f;
  if (ex == EX_TERTAWA) bounce = -fabsf(sinf(tms(now) / 90.0f)) * 4.0f;
  if (f.bh == BH_BOUNCE) bounce = -fabsf(sinf(bp * 3.14159f * 3)) * 6.0f;
  if (f.bh == BH_PET) bounce = -fabsf(sinf(tms(now) / 120.0f)) * 3.0f;
  if (f.bh == BH_SHAKE) shakeX = sinf(tms(now) / 22.0f) * 3.0f;
  if (f.bh == BH_HUM) bounce = sinf(tms(now) / 190.0f) * 2.0f;
  if (f.bh == BH_SNEEZE && bp > 0.55f && bp < 0.72f) shakeX = sinf(tms(now) / 18.0f) * 3.0f;
  if (f.bh == BH_DIZZY) shakeX = sinf(tms(now) / 90.0f) * 2.0f;
  if (f.bh == BH_HICCUP) bounce = bp < 0.7f ? -fabsf(sinf(bp * 9.42f)) * 5.0f : 0.0f;
  if (f.bh == BH_SHIVER) shakeX = sinf(tms(now) / 15.0f) * 1.8f;
  if (f.bh == BH_POKE) shakeX = sinf(tms(now) / 20.0f) * 2.0f;
  if (f.bh == BH_JAM) bounce = sinf(tms(now) / 150.0f) * 2.2f;
  if (f.bh == BH_HELLO) bounce = sinf(tms(now) / 160.0f) * 2.0f;
  if (f.bh == BH_HEARTBEAT) bounce = -pulse * 1.5f;
  if (f.bh == BH_SQUISH) bounce = fminf(0.0f, sinf(bp * 12.566f)) * 6.0f;
  if (f.bh == BH_NOD) bounce = nodSink * 5.0f - ((bp > 0.72f && bp < 0.85f) ? 4.0f : 0.0f);
  if (f.bh == BH_TONGUE) bounce = sinf(tms(now) / 120.0f) * 1.6f;
  if (f.bh == BH_LOVE) bounce = -fabsf(sinf(tms(now) / 150.0f)) * 2.5f;
  if (f.bh == BH_COOL) bounce = sinf(tms(now) / 350.0f) * 1.2f;
  if (f.bh == BH_GRR) shakeX = sinf(tms(now) / 28.0f) * 1.6f;
  if (f.bh == BH_SCARE) shakeX = sinf(tms(now) / 19.0f) * 2.6f;
  if (f.bh == BH_CAT) shakeX = sinf(tms(now) / 320.0f) * 2.0f;
  if (f.bh == BH_LAUGH) shakeX = sinf(tms(now) / 45.0f) * 1.2f;
  if (act == ACT_IDLE && f.bh == BH_NONE && !joget && !sleeping) {  // mengambang pelan, tidak pernah benar-benar diam
    shakeX += sinf(tms(now) / 1700.0f) * 1.2f;
    bounce += sinf(tms(now) / 2300.0f) * 0.8f;
  }
  if (act == ACT_TALK && !joget && ex != EX_TERTAWA) bounce = -level * 2.0f;  // mengangguk kecil saat bicara

  f.bnc.step(bounce, 0.55f, 0.45f);  // lompatan dihaluskan (tidak ada patahan mendadak)
  float lxF = EYE_LX + f.lookX.x + shakeX, rxF = EYE_RX + f.lookX.x + shakeX;
  float cyF = EYE_Y + f.lookY.x + f.bnc.x;
  float hlF = f.lh.x * openL, hrF = f.rh.x * openR;
  float wlF = f.lw.x + (openL < 0.5f ? 3 : 0), wrF = f.rw.x + (openR < 0.5f ? 3 : 0);
  // dibulatkan ke terdekat (bukan dipotong ke nol, yang membuat lompatan ganda saat melewati 0)
  int lx = (int)lroundf(lxF), rx = (int)lroundf(rxF), cy = (int)lroundf(cyF);
  int hl = (int)lroundf(hlF), hr = (int)lroundf(hrF);
  int wl = (int)lroundf(wlF), wr = (int)lroundf(wrF);
  int ryOff = ex == EX_BINGUNG ? -5 : 0;

  if (fx) {
    drawBackground(g, act, ex, joget, sleeping, drowsy, level, now, hour, hot, f.scene);
    uint32_t dt = now - f.exChangeAt;
    if (dt < 260 && f.exChangeAt > 1500 && !sleeping) {  // cincin "pop" saat ekspresi berganti
      int r = 22 + dt / 4;
      g.drawCircle(64, 30, r, C_ON);
      g.drawCircle(64, 30, r + 5, C_ON);
    }
  }

  if (sleeping) {
    float breathe = sinf(tms(now) / 900.0f) * 1.5f;
    for (int i = 0; i < 2; i++) {
      int cx = i ? EYE_RX : EYE_LX;
      g.fillRoundRect(cx - EYE_W / 2, EYE_Y + 6 + (int)breathe, EYE_W, 4, 2, C_ON);
    }
    int phase = (now / 700) % 3;
    g.setTextSize(1);
    g.setTextColor(C_ON);
    for (int i = 0; i <= phase; i++) {
      g.setCursor(94 + i * 6, 20 - i * 6);
      g.print('z');
    }
    return;
  }

  int shx = (int)lroundf(-f.lookX.x * 0.15f), shy = (int)lroundf(-f.lookY.x * 0.2f);
  if (f.bh == BH_DIZZY) {
    drawDizzyEye(g, lx, cy, now, true);
    drawDizzyEye(g, rx, cy, now, false);
    for (int i = 0; i < 3; i++) {  // bintang berputar di atas kepala
      float a = tms(now) / 260.0f + i * 2.094f;
      drawSparkle(g, 64 + (int)(cosf(a) * 22), 7 + (int)(sinf(a) * 4), 2);
    }
  } else if (f.bh == BH_POKE && bp > 0.15f && bp < 0.8f) {  // mata > <
    for (int t = -1; t <= 1; t++) {
      g.drawLine(lx - 10, cy - 11 + t, lx + 8, cy + t, C_ON); g.drawLine(lx + 8, cy + t, lx - 10, cy + 11 + t, C_ON);
      g.drawLine(rx + 10, cy - 11 + t, rx - 8, cy + t, C_ON); g.drawLine(rx - 8, cy + t, rx + 10, cy + 11 + t, C_ON);
    }
  } else if (f.bh == BH_SCARE) {  // mata bulat berongga, bola mata kecil gemetar
    for (int i = 0; i < 2; i++) {
      int cx = i ? rx : lx;
      g.drawCircle(cx, cy, 13, C_ON);
      g.drawCircle(cx, cy, 12, C_ON);
      g.fillCircle(cx + (int)(sinf(tms(now) / 30.0f + i) * 2), cy + (int)(cosf(tms(now) / 27.0f) * 2), 2, C_ON);
    }
  } else if (f.bh == BH_COOL && !(bp < 0.2f ? (1 - bp / 0.2f) > 0.35f : (bp > 0.82f && (bp - 0.82f) / 0.18f > 0.35f))) {
    // mata tertutup kacamata (digambar di hiasan)
  } else {
    int lxx = lx, rxx = rx, cyy = cy, wl2 = wl, wr2 = wr, hl2 = hl, hr2 = hr, lid = 0;
    if (f.bh == BH_CROSS) {  // juling: kedua mata saling mendekat
      int c = (int)(7.0f * fminf(1.0f, fminf(bp, 1.0f - bp) / 0.15f));
      lxx += c; rxx -= c;
    }
    if (f.bh == BH_GLITCH) {  // ukuran & posisi mata loncat acak
      lxx += f.range(-5, 5); rxx += f.range(-5, 5); cyy += f.range(-3, 3);
      wl2 += f.range(-8, 8); wr2 += f.range(-8, 8); hl2 += f.range(-7, 7); hr2 += f.range(-7, 7);
    }
    if (f.bh == BH_HUNGRY) { lxx += 2; rxx -= 2; }
    if (f.bh == BH_SUS) lid = hl / 3;
    if (f.bh == BH_ROLL) lid = hl / 4;
    drawEyeSub(g, lxF + (lxx - lx), cyF + (cyy - cy), wlF + (wl2 - wl), hlF + (hl2 - hl), ex, true, now, shx, shy, lid);
    drawEyeSub(g, rxF + (rxx - rx), cyF + (cyy - cy) + ryOff, wrF + (wr2 - wr), hrF + (hr2 - hr), ex, false, now, shx, shy, lid);
  }

  // ---- Hiasan ekspresi ----
  int under = cy + (int)(f.lh.x / 2) + 3;
  if (ex == EX_MALU || ex == EX_CINTA) { drawBlush(g, lx - 4, under); drawBlush(g, rx + 4, under); }
  if (ex == EX_MENANGIS) {
    int t = (now / 25) % 22;
    drawTear(g, lx - 10, under + t / 2);
    drawTear(g, rx + 10, under + ((t + 11) % 22) / 2);
  }
  if (ex == EX_KAGUM) {
    int tw = (now / 200) % 4;
    drawSparkle(g, 8, 12, tw == 0 ? 4 : 2);
    drawSparkle(g, 120, 46, tw == 2 ? 4 : 2);
    drawSparkle(g, 116, 14, tw == 1 ? 3 : 1);
  }
  if (ex == EX_BINGUNG) {
    g.setTextSize(2);
    g.setTextColor(C_ON);
    g.setCursor(114, 12);
    g.print('?');
  }
  if (f.bh == BH_FLY && bp < 0.94f) {  // lalat kecil dengan sayap mengepak
    int fxp = (int)flyX, fyp = (int)flyY;
    g.fillCircle(fxp, fyp, 1, C_ON);
    if ((now / 40) % 2) { g.drawLine(fxp - 1, fyp - 1, fxp - 3, fyp - 3, C_ON); g.drawLine(fxp + 1, fyp - 1, fxp + 3, fyp - 3, C_ON); }
    else { g.drawPixel(fxp - 2, fyp - 1, C_ON); g.drawPixel(fxp + 2, fyp - 1, C_ON); }
  }
  if (f.bh == BH_HUM) {  // not keluar dari mulut, mulut kecil membulat
    g.drawCircle(64, 55, 2 + (int)(fabsf(sinf(tms(now) / 200.0f)) * 1.5f), C_ON);
    for (int i = 0; i < 2; i++) {
      int ph = (int)((now / 28 + i * 25) % 50);
      int nx = 74 + i * 14 + (int)(sinf(ph / 6.0f) * 3), ny = 56 - ph;
      if (ny > 6) drawNote(g, nx, ny);
    }
  }
  if (f.bh == BH_SNEEZE && bp > 0.55f && bp < 0.9f) {  // HACHII!
    g.setTextSize(1);
    g.setTextColor(C_ON);
    g.setCursor(43, 55);
    g.print("HACHII!");
    for (int i = 0; i < 6; i++) {  // percikan kecil
      uint32_t h = fxHash(i * 31 + 5);
      float t = (bp - 0.55f) / 0.35f;
      g.drawPixel(64 + (int)(((int)(h % 40) - 20) * t * 1.4f), 50 + (int)((h >> 8) % 6 * t * 2), C_ON);
    }
  }
  if (f.bh == BH_PET) {  // hati kecil bermunculan saat dielus
    for (int i = 0; i < 5; i++) {
      uint32_t h = fxHash(i * 977 + 3);
      float t = fmodf(bp * 1.6f + i * 0.2f, 1.0f);
      int hx = (int)(h % 100) + 14, hy = 50 - (int)(t * 46);
      if (t < 0.85f) drawHeart(g, hx + (int)(sinf(t * 9 + i) * 3), hy, 2);
    }
  }
  if (ex == EX_KESAL) {  // tanda kesal kecil di pojok
    g.drawLine(6, 8, 12, 14, C_ON); g.drawLine(12, 8, 6, 14, C_ON);
  }

  if (f.bh >= BH_SPIN && act == ACT_IDLE) drawBehaviorOverlay(g, f, bp, now, lx, rx, cy, bounce);
  if (act == ACT_IDLE && f.bh == BH_NONE && !drowsy && now < f.mouthEnd) {  // senyum kecil
    g.drawLine(58, 55, 61, 58, C_ON); g.drawLine(61, 58, 67, 58, C_ON); g.drawLine(67, 58, 70, 55, C_ON);
  }

  // ---- Mulut & elemen per aktivitas ----
  if (f.bh == BH_YAWN && bp > 0.15f && bp < 0.8f) {
    int mh = 4 + (int)(10 * sinf((bp - 0.15f) / 0.65f * 3.14159f));
    g.fillRoundRect(56, 54 - mh / 2, 16, mh, mh / 2, C_ON);
  } else if (act == ACT_LISTEN) {
    for (int i = 0; i < 7; i++) {  // gelombang suara di bawah mata
      float k = 0.35f + 0.65f * fabsf(sinf(tms(now) / 90.0f + i * 0.9f));
      int bh = 1 + (int)(level * 9 * k);
      g.fillRect(43 + i * 6, 62 - bh, 4, bh, C_ON);
    }
  } else if (act == ACT_THINK) {
    int n = (now / 300) % 4;
    for (int i = 0; i < n; i++) g.fillCircle(100 + i * 8, 56, 2, C_ON);
  } else if (act == ACT_TALK) {
    if (ex == EX_TERTAWA) {  // mulut tertawa lebar
      int mh = 6 + (int)(level * 6);
      g.fillCircle(64, 52, mh, C_ON);
      g.fillRect(64 - mh - 1, 52 - mh - 1, 2 * mh + 2, mh + 1, C_OFF);
    } else {
      int mh = level > 0.04f ? 2 + (int)(level * 9) : 2;
      int mw = ex == EX_KAGET ? 10 : 18;
      if (level > 0.35f && (now / 170) % 4 == 0) {  // sesekali mulut membulat seperti mengucap "o"
        g.fillCircle(64, 57, 2 + (int)(level * 4), C_ON);
        g.fillCircle(64, 57, (int)(level * 2), C_OFF);
      } else {
        g.fillRoundRect(64 - mw / 2, 54, mw, mh, mh > 4 ? 3 : 1, C_ON);
      }
    }
    if (joget) {  // not musik melayang naik
      for (int i = 0; i < 3; i++) {
        int ph = (int)((now / 30 + i * 20) % 60);
        int nx = (i == 1 ? 116 : 8 + i * 4) + (int)(sinf(ph / 8.0f) * 3);
        int ny = 62 - ph;
        if (ny > 12) drawNote(g, nx, ny);
      }
    }
  } else if (f.bh == BH_PET) {
    g.fillRoundRect(58, 54, 12, 3, 1, C_ON);  // senyum kecil
  }
}

// ---- Mode teks ----
static const int TXT_COLS = 21, TXT_MAX_LINES = 120;

// Bungkus teks per kata ke baris 21 karakter. Karakter non-ASCII dilewati.
inline int wrapText(const char *src, char lines[][TXT_COLS + 1], int maxLines) {
  int n = 0, len = 0;
  lines[0][0] = 0;
  const char *p = src;
  while (*p && n < maxLines) {
    if (*p == '\n') {
      lines[n][len] = 0; n++; len = 0;
      if (n < maxLines) lines[n][0] = 0;
      p++;
      continue;
    }
    if (*p == ' ' && len == 0) { p++; continue; }
    // panjang kata berikutnya
    const char *e = p;
    while (*e && *e != ' ' && *e != '\n') e++;
    int wl = 0;
    for (const char *q = p; q < e; q++) if ((unsigned char)*q < 128) wl++;
    if (*p != ' ' && len > 0 && len + wl > TXT_COLS && wl <= TXT_COLS) {
      lines[n][len] = 0; n++; len = 0;
      if (n >= maxLines) break;
      lines[n][0] = 0;
      continue;
    }
    if ((unsigned char)*p < 128) {
      if (len >= TXT_COLS) {
        lines[n][len] = 0; n++; len = 0;
        if (n >= maxLines) break;
        lines[n][0] = 0;
        if (*p == ' ') { p++; continue; }  // spasi di batas baris tidak ikut ke baris baru
      }
      lines[n][len++] = *p;
    }
    p++;
  }
  if (n < maxLines) { lines[n][len] = 0; if (len > 0) n++; }
  return n;
}

inline void drawTextScreen(Adafruit_GFX &g, const char *title, const char *text, Act act,
                           float level, uint32_t now, uint32_t textSince, int titleChars = 21) {
  static char lines[TXT_MAX_LINES][TXT_COLS + 1];
  g.setTextSize(1);
  g.setTextColor(C_ON);
  g.setCursor(0, 0);
  if (act == ACT_THINK) {
    g.print("Berpikir");
    for (uint32_t i = 0; i < (now / 300) % 4; i++) g.print('.');
  } else {
    char t[22];
    if (titleChars > 21) titleChars = 21;
    strncpy(t, title, titleChars);
    t[titleChars] = 0;
    g.print(t);
  }
  g.drawFastHLine(0, 9, 128, C_ON);

  int visible = (act == ACT_LISTEN) ? 5 : 6;
  int n = wrapText(text, lines, TXT_MAX_LINES);
  int first = 0;
  if (n > visible) {
    // Gulir 1 baris tiap 1,5 detik, berhenti sebentar di akhir, lalu ulang dari atas
    int steps = n - visible;
    uint32_t period = (uint32_t)(steps + 4) * 1500;
    int pos = (int)(((now - textSince) % period) / 1500) - 2;
    first = pos < 0 ? 0 : (pos > steps ? steps : pos);
  }
  for (int i = 0; i < visible && first + i < n; i++) {
    g.setCursor(0, 12 + i * 9);
    g.print(lines[first + i]);
  }
  if (act == ACT_LISTEN) {
    g.drawRect(0, 57, 128, 7, C_ON);
    g.fillRect(1, 58, (int)(126 * level), 5, C_ON);
  }
}

// ---- Baris status kecil di pojok kanan atas: timer, server putus, baterai ----
inline void drawBattery(Adafruit_GFX &g, int x, int y, int pct) {
  g.drawRect(x, y, 13, 7, C_ON);
  g.fillRect(x + 13, y + 2, 2, 3, C_ON);
  int w = (pct * 11) / 100;
  if (w > 0) g.fillRect(x + 1, y + 1, w, 5, C_ON);
}

// Mengembalikan lebar (piksel) yang dipakai, dihitung dari kanan layar.
inline int drawStatusBar(Adafruit_GFX &g, int bat, bool online, const char *timerTxt, bool clearBg) {
  int w = 0;
  if (bat >= 0) w += 17;
  if (!online) w += 10;
  int tlen = (int)strlen(timerTxt);
  if (tlen) w += tlen * 6 + 9;
  if (!w) return 0;
  int x = 128 - w;
  if (clearBg) g.fillRect(x - 1, 0, w + 1, 9, C_OFF);
  g.setTextSize(1);
  g.setTextColor(C_ON);
  if (tlen) {  // ikon jam kecil + sisa waktu
    g.drawCircle(x + 3, 4, 3, C_ON);
    g.drawFastVLine(x + 3, 2, 3, C_ON);
    g.drawFastHLine(x + 3, 4, 2, C_ON);
    g.setCursor(x + 8, 1);
    g.print(timerTxt);
    x += tlen * 6 + 9;
  }
  if (!online) {  // lingkaran dicoret = server tidak tersambung
    g.drawCircle(x + 4, 4, 3, C_ON);
    g.drawLine(x + 1, 7, x + 7, 1, C_ON);
    x += 10;
  }
  if (bat >= 0) drawBattery(g, x + 1, 1, bat);
  return w;
}
