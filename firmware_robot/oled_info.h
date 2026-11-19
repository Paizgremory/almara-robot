// oled_info.h — "OLED info" Almara, sekarang kanvas 128x32 yang tampil di bagian BAWAH layar IPS TFT 1,3" (ST7789).
// Semua gambar tetap digambar ke kanvas 1-bit 128x32 (MonoPanel), lalu diperbesar 1,875x dan dikirim berwarna
// oleh layar_tft.h (kuning). Tidak ada lagi I2C / Wire1: layar sama dengan tampilan utama (SPI).
//
// Isi layar (otomatis, urut prioritas):
//   DENGAR  : EQUALIZER 32 batang segmen LED, tumbuh dari BAWAH ke ATAS mengikuti suara mic (+ penanda puncak)
//   BERPIKIR: cincin titik berputar berekor, teks, dan pita gelombang mengalir
//   MUSIK   : piringan hitam berputar + lengan jarum, judul bergulir, progres, equalizer 6 batang berdenyut
//   TEKS    : jawaban Almara diketik huruf demi huruf dengan kursor berkedip, ikon gelembung bicara yang
//             berdenyut di kiri, garis waktu halaman di atas, gelombang suara di bawah
//   BERANDA : (3 halaman bergantian: JAM, MATA hidup, SUASANA suhu/kelembapan)
//             jam besar dengan digit yang meluncur saat berganti, kilau menyapu, komet di tepi bawah,
//             panel kanan berbingkai dengan cahaya berputar; tiap 5 detik berganti halaman:
//             suhu / kelembapan / baterai  <->  hari / tanggal / bulan (ikon matahari, kalender, bulan)
//   NOTIF   : notifikasi HP (WhatsApp dll.) layar penuh: ikon + nama aplikasi + pengirim + isi bergulir
//             (menimpa semua layar kecuali saat mendengar / berpikir)
//   Semua perpindahan layar memakai transisi "tirai" (strip vertikal terbuka berurutan).
#pragma once
#include "layar_tft.h"   // layar TFT ST7789 (pengganti OLED 128x32): kanvas 128x32 diperbesar & dikirim berwarna
#include <math.h>


static MonoPanel infoOled(128, 32);   // kanvas 1-bit 128x32 (dikirim ke bagian bawah layar TFT)
static inline void infoShow() { tftShowInfo(infoOled); }
static bool infoOk = false;
static SemaphoreHandle_t infoMtx = nullptr;

enum InfoMode : uint8_t { IM_IDLE, IM_LISTEN, IM_THINK, IM_TEXT, IM_MUSIC, IM_NOTIF };

struct InfoMusic {
  bool on = false;        // ada sesi musik (sedang main atau baru dijeda)
  bool playing = false;
  bool radio = false;
  char judul[64] = "";
  uint32_t durasi = 0;    // detik; 0 = tidak diketahui (radio / file tanpa info)
  uint32_t seek = 0;      // lagu dimulai dari detik ke-berapa
  uint32_t introSec = 0;  // lama ucapan pembuka sebelum musik terdengar
  uint32_t startMs = 0;   // saat aliran dimulai
  uint32_t titleAt = 0;   // saat judul terakhir berganti (untuk teks bergulir)
  uint32_t pausedAt = 0;
  uint32_t frozen = 0;    // detik yang sudah terdengar saat dijeda
  int id = 0;
};
static InfoMusic infoMusic;

// ---------- alat kecil ----------
static inline float infoClamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float infoEaseOut(float t) { t = infoClamp01(t); return 1 - (1 - t) * (1 - t) * (1 - t); }

// Transisi "tirai": setiap kali layar berganti, 16 strip vertikal terbuka berurutan dari kiri ke kanan.
// Dipanggil sebelum display() di setiap layar; mode dipakai untuk mendeteksi pergantian.
static void infoCurtain(uint32_t now, uint8_t mode) {
  static uint8_t last = 255;
  static uint32_t at = 0;
  if (mode != last) { last = mode; at = now; }
  uint32_t dt = now - at;
  if (dt >= 360) return;
  for (int k = 0; k < 16; k++) {
    int32_t t = (int32_t)dt - k * 10;
    if (t < 0) t = 0;
    int w = 8 - (int)(t * 8 / 170);
    if (w > 0) infoOled.fillRect(k * 8, 0, w, 32, SSD1306_BLACK);
  }
}

// Transisi tirai; dipanggil sebelum display() di setiap layar.
static void infoWipe(uint32_t now, uint8_t mode) {
  infoCurtain(now, mode);
}

static bool infoBegin() {
  infoMtx = xSemaphoreCreateMutex();
  if (!tftBegin() || !infoOled.begin()) return false;   // layar TFT dipakai bersama kanvas utama (tftBegin aman dipanggil dua kali)
  infoOled.setTextColor(SSD1306_WHITE);
  infoOled.setTextWrap(false);

  // Animasi pembuka (~0,9 detik): gelombang batang naik dari bawah, lalu nama muncul disapu.
  const char *name = "ALMARA";
  for (int f = 0; f < 30; f++) {
    infoOled.clearDisplay();
    float grow = infoClamp01(f / 12.0f);                // batang tumbuh
    float fade = f < 14 ? 1.0f : infoClamp01(1.0f - (f - 14) / 10.0f);  // lalu surut
    for (int i = 0; i < 32; i++) {
      float w = 0.5f + 0.5f * sinf(i * 0.55f - f * 0.7f);
      int h = (int)(w * 26.0f * grow * fade);
      if (h > 0) infoOled.fillRect(i * 4, 31 - h, 3, h + 1, SSD1306_WHITE);
    }
    if (f >= 12) {  // nama diketik dengan sapuan cahaya
      infoOled.setTextSize(2);
      int tx = 64 - 6 * 6;
      infoOled.fillRect(tx - 3, 7, 6 * 12 + 6, 18, SSD1306_BLACK);
      infoOled.setCursor(tx, 9);
      infoOled.print(name);
      int sweep = (f - 12) * 7;
      for (int y = 8; y < 25; y++)
        for (int dx = 0; dx < 5; dx++) {
          int x = tx - 8 + sweep + dx - (y - 8) / 3;
          if (x >= tx - 3 && x < tx + 75) infoOled.drawPixel(x, y, !infoOled.getPixel(x, y));
        }
      infoOled.setTextSize(1);
    }
    infoOled.display();
    delay(28);
  }
  infoOled.clearDisplay();
  infoOled.display();
  tftSentInfo.valid = false;   // kirim penuh pada frame pertama
  infoOk = true;
  return true;
}

// ---------- status musik (dipanggil dari alur utama / tugas /now) ----------
static uint32_t infoElapsed(uint32_t now, const InfoMusic &m) {
  if (!m.playing) return m.frozen;
  uint32_t ms = now - m.startMs;
  uint32_t intro = m.introSec * 1000UL;
  ms = ms > intro ? ms - intro : 0;
  return m.seek + ms / 1000;
}

static void infoMusicStart(bool radio, const char *judul, uint32_t durasi, uint32_t seek, uint32_t introSec, int id) {
  if (!infoMtx) return;
  xSemaphoreTake(infoMtx, portMAX_DELAY);
  infoMusic.on = true;
  infoMusic.playing = true;
  infoMusic.radio = radio;
  strlcpy(infoMusic.judul, judul, sizeof(infoMusic.judul));
  infoMusic.durasi = durasi;
  infoMusic.seek = seek;
  infoMusic.introSec = introSec;
  infoMusic.startMs = millis();
  infoMusic.titleAt = millis();
  infoMusic.frozen = 0;
  infoMusic.id = id;
  xSemaphoreGive(infoMtx);
}

// Playlist: lagu berganti di tengah aliran yang sama; server memberi tahu lewat /now.
static void infoMusicTitle(int id, const char *judul, uint32_t durasi) {
  if (!infoMtx) return;
  xSemaphoreTake(infoMtx, portMAX_DELAY);
  if (infoMusic.on && infoMusic.playing && infoMusic.id == id && strcmp(infoMusic.judul, judul) != 0) {
    strlcpy(infoMusic.judul, judul, sizeof(infoMusic.judul));
    infoMusic.durasi = durasi;
    infoMusic.seek = 0;
    infoMusic.introSec = 0;
    infoMusic.startMs = millis();
    infoMusic.titleAt = millis();
  }
  xSemaphoreGive(infoMtx);
}

static void infoMusicEnd() {
  if (!infoMtx) return;
  xSemaphoreTake(infoMtx, portMAX_DELAY);
  if (infoMusic.on && infoMusic.playing) {
    infoMusic.frozen = infoElapsed(millis(), infoMusic);
    infoMusic.playing = false;
    infoMusic.pausedAt = millis();
  }
  xSemaphoreGive(infoMtx);
}

// ---------- alat gambar ----------
static void infoAscii(const char *in, char *out, size_t n) {  // font bawaan hanya ASCII
  size_t j = 0;
  for (size_t i = 0; in[i] && j + 1 < n; i++) {
    uint8_t c = (uint8_t)in[i];
    if (c == '\n' || c == '\t') c = ' ';
    if (c >= 32 && c < 127) out[j++] = (char)c;
  }
  out[j] = 0;
}

// ---------- NOTIFIKASI HP (WhatsApp dll.) ----------
// Diisi dari firmware lewat infoNotifPush() (data dari server /api/notif). Antrian 4 notifikasi; tiap
// notifikasi tampil layar penuh 5-12 detik (teks panjang bergulir), masuk dari kanan, keluar ke kiri.
struct InfoNotif {
  char app[16] = "";
  char dari[26] = "";
  char teks[128] = "";
  uint32_t at = 0;
  uint32_t dur = 0;
};
#define INFO_NQ 4
static InfoNotif infoNq[INFO_NQ];
static int infoNqN = 0;
static InfoNotif infoNcur;
static bool infoNactive = false;

static void infoNotifPush(const char *app, const char *dari, const char *teks) {
  if (!infoMtx) return;
  InfoNotif n;
  infoAscii(app, n.app, sizeof(n.app));
  infoAscii(dari, n.dari, sizeof(n.dari));
  infoAscii(teks, n.teks, sizeof(n.teks));  // emoji & huruf non-ASCII dibuang (font bawaan)
  if (!n.teks[0]) strlcpy(n.teks, "(media / emoji)", sizeof(n.teks));
  xSemaphoreTake(infoMtx, portMAX_DELAY);
  if (infoNqN >= INFO_NQ) { memmove(&infoNq[0], &infoNq[1], sizeof(InfoNotif) * (INFO_NQ - 1)); infoNqN = INFO_NQ - 1; }
  infoNq[infoNqN++] = n;
  xSemaphoreGive(infoMtx);
}

// Dipanggil tiap frame: true = ada notifikasi yang sedang tampil (disalin ke *out).
static bool infoNotifCurrent(uint32_t now, InfoNotif *out) {
  if (!infoMtx) return false;
  bool r = false;
  xSemaphoreTake(infoMtx, portMAX_DELAY);
  if (infoNactive && now - infoNcur.at >= infoNcur.dur) infoNactive = false;
  if (!infoNactive && infoNqN > 0) {
    infoNcur = infoNq[0];
    memmove(&infoNq[0], &infoNq[1], sizeof(InfoNotif) * (INFO_NQ - 1));
    infoNqN--;
    infoNcur.at = now;
    uint32_t pw = (uint32_t)strlen(infoNcur.teks) * 6;
    uint32_t d = 5000;
    if (pw > 96) d = 3000 + (pw + 24) * 25;  // gulir ~40 px/dtk, cukup untuk sekali lewat
    if (d > 12000) d = 12000;
    if (infoNqN > 0 && d > 4500) d = 4500;   // masih ada antrean: jangan terlalu lama
    infoNcur.dur = d;
    infoNactive = true;
  }
  if (infoNactive) { *out = infoNcur; r = true; }
  xSemaphoreGive(infoMtx);
  return r;
}

static bool infoHas(const char *hay, const char *needle) {  // cari tanpa peduli huruf besar/kecil
  size_t nl = strlen(needle);
  for (size_t i = 0; hay[i]; i++) {
    size_t k = 0;
    while (k < nl && hay[i + k] && tolower((unsigned char)hay[i + k]) == needle[k]) k++;
    if (k == nl) return true;
  }
  return false;
}

static void infoDrawNotif(uint32_t now, const InfoNotif &n) {
  MonoPanel &o = infoOled;
  uint32_t age = now - n.at;
  o.clearDisplay();
  o.setTextWrap(false);
  o.setTextSize(1);
  o.setTextColor(SSD1306_WHITE);

  // geser masuk dari kanan (memantul sedikit), keluar ke kiri
  int ox = 0;
  if (age < 320) { float p = age / 320.0f - 1; float e = 1 + 2.70158f * p * p * p + 1.70158f * p * p; ox = (int)((1.0f - e) * 128); }
  else if (n.dur > 260 && age > n.dur - 260) ox = -(int)(infoEaseOut((age - (n.dur - 260)) / 260.0f) * 128);

  // baris 1: nama aplikasi pada bilah terbalik
  o.fillRect(ox + 30, 0, 98, 9, SSD1306_WHITE);
  o.setTextColor(SSD1306_BLACK);
  o.setCursor(ox + 33, 1);
  char app[16];
  strlcpy(app, n.app[0] ? n.app : "Pesan", sizeof(app));
  for (char *c = app; *c; c++) *c = toupper((unsigned char)*c);
  o.print(app);
  o.setTextColor(SSD1306_WHITE);

  // baris 2: pengirim (dipotong)
  char who[17];
  strlcpy(who, n.dari[0] ? n.dari : "-", sizeof(who));
  if (strlen(n.dari) > 16) { who[14] = '.'; who[15] = '.'; who[16] = 0; }
  o.setCursor(ox + 31, 11);
  o.print(who);

  // baris 3: isi pesan, bergulir kalau kepanjangan
  int pw = (int)strlen(n.teks) * 6, tx = 31;
  if (pw > 96) {
    uint32_t el = age > 900 ? age - 900 : 0;
    tx = 31 - (int)((el / 25) % (uint32_t)(pw + 24));
  }
  o.setCursor(ox + tx, 22);
  o.print(n.teks);
  if (pw > 96) { o.setCursor(ox + tx + pw + 24, 22); o.print(n.teks); }
  o.fillRect(ox, 0, 30, 32, SSD1306_BLACK);  // tutup teks yang bergulir ke bawah ikon

  // ikon: gelembung chat (WhatsApp / SMS), pesawat kertas (Telegram), lonceng (lainnya)
  int jig = age < 700 ? (int)roundf(sinf(age / 45.0f) * (1.0f - age / 700.0f) * 2.5f) : 0;
  int ix = ox + 4 + jig;
  bool chat = infoHas(n.app, "whatsapp") || infoHas(n.app, "pesan") || infoHas(n.app, "messag") || infoHas(n.app, "sms");
  if (chat) {
    o.fillRoundRect(ix, 5, 22, 16, 5, SSD1306_WHITE);
    o.fillTriangle(ix + 3, 20, ix + 3, 27, ix + 10, 20, SSD1306_WHITE);
    for (int i = 0; i < 3; i++) {  // tiga titik melompat bergantian
      int b = (int)(fmaxf(0, sinf(now / 150.0f - i * 0.9f)) * 3);
      o.fillRect(ix + 5 + i * 5, 12 - b, 2, 2, SSD1306_BLACK);
    }
  } else if (infoHas(n.app, "telegram")) {
    o.fillTriangle(ix + 22, 5, ix, 14, ix + 8, 17, SSD1306_WHITE);
    o.fillTriangle(ix + 22, 5, ix + 8, 17, ix + 11, 26, SSD1306_WHITE);
    o.drawLine(ix + 8, 17, ix + 17, 9, SSD1306_BLACK);
  } else {
    o.fillCircle(ix + 11, 12, 7, SSD1306_WHITE);
    o.fillRect(ix + 4, 12, 15, 8, SSD1306_WHITE);
    o.fillRect(ix + 1, 19, 21, 3, SSD1306_WHITE);
    o.fillCircle(ix + 11, 25, 2, SSD1306_WHITE);
  }
  // cincin getar memancar dari pojok ikon (hanya di awal)
  if (age < 1400) {
    int r = 3 + (int)((age / 60) % 8);
    o.drawCircleHelper(ix + 22, 5, r, 2, SSD1306_WHITE);
  }
  infoShow();
}

// Piringan hitam berputar (sampul lagu): alur, label tengah, kilau yang berputar, lengan jarum saat main.
static void infoVinyl(MonoPanel &o, uint32_t now, bool playing) {
  static float ang = 0;
  if (playing) ang = now / 380.0f;
  const int cx = 15, cy = 16;
  o.drawCircle(cx, cy, 14, SSD1306_WHITE);
  o.drawCircle(cx, cy, 10, SSD1306_WHITE);
  // alur putus-putus (terasa berputar)
  for (int i = 0; i < 18; i++) {
    float a = ang * 1.0f + i * 0.349f;
    if (i % 3 == 0) continue;
    o.drawPixel(cx + (int)roundf(cosf(a) * 12), cy + (int)roundf(sinf(a) * 12), SSD1306_WHITE);
  }
  // kilau pantulan (dua busur pendek berlawanan)
  for (int s = 0; s < 2; s++) {
    float a = ang * 1.6f + s * 3.14159f;
    o.drawLine(cx + (int)(cosf(a) * 7), cy + (int)(sinf(a) * 7), cx + (int)(cosf(a) * 9), cy + (int)(sinf(a) * 9), SSD1306_WHITE);
  }
  // label tengah dengan penanda putar
  o.fillCircle(cx, cy, 5, SSD1306_WHITE);
  o.drawPixel(cx, cy, SSD1306_BLACK);
  o.drawPixel(cx + (int)roundf(cosf(ang * 1.6f) * 3), cy + (int)roundf(sinf(ang * 1.6f) * 3), SSD1306_BLACK);
  if (playing) {  // lengan jarum
    o.drawLine(31, 1, 24, 10, SSD1306_WHITE);
    o.fillCircle(31, 1, 1, SSD1306_WHITE);
    o.fillRect(22, 10, 3, 3, SSD1306_WHITE);
  }
}

static void infoIconRadio(MonoPanel &o, uint32_t now, bool playing) {  // radio + antena + gelombang
  o.drawRoundRect(3, 14, 20, 13, 2, SSD1306_WHITE);
  o.drawCircle(9, 21, 3, SSD1306_WHITE);
  o.drawFastHLine(15, 18, 5, SSD1306_WHITE);
  o.drawFastHLine(15, 21, 5, SSD1306_WHITE);
  o.drawFastHLine(15, 24, 5, SSD1306_WHITE);
  o.drawLine(17, 14, 22, 5, SSD1306_WHITE);
  if (playing) {  // gelombang memancar dari ujung antena
    for (int k = 0; k < 3; k++) {
      int r = 2 + (int)((now / 110 + k * 3) % 9);
      o.drawCircleHelper(22, 5, r, 2, SSD1306_WHITE);
    }
  }
}

// ---------- layar MUSIK ----------
static void infoDrawMusic(uint32_t now, const InfoMusic &m) {
  MonoPanel &o = infoOled;
  const int X0 = 34, W = 94;
  o.clearDisplay();
  o.setTextWrap(false);
  o.setTextSize(1);
  o.setTextColor(SSD1306_WHITE);

  // baris 1: judul, bergulir kalau kepanjangan
  char t[64];
  infoAscii(m.judul, t, sizeof(t));
  if (!t[0]) strlcpy(t, m.radio ? "Radio" : "Lagu", sizeof(t));
  int pxw = (int)strlen(t) * 6;
  int x = X0;
  bool scroll = pxw > W;
  if (scroll) {
    uint32_t el = now - m.titleAt;
    if (el > 1500) x = X0 - (int)(((el - 1500) / 33) % (uint32_t)(pxw + 24));
  }
  o.setCursor(x, 2);
  o.print(t);
  if (scroll) { o.setCursor(x + pxw + 24, 2); o.print(t); }
  o.fillRect(0, 0, 32, 32, SSD1306_BLACK);  // tutup sisa teks yang lewat di bawah sampul

  // sampul: piringan hitam (lagu) atau radio
  if (m.radio) infoIconRadio(o, now, m.playing); else infoVinyl(o, now, m.playing);

  // baris 2: keterangan + equalizer / ikon jeda
  o.setCursor(X0, 13);
  if (!m.playing) o.print(F("Dijeda"));
  else if (m.radio) { o.print(F("Radio ")); if ((now / 500) % 2) o.fillCircle(X0 + 40, 16, 2, SSD1306_WHITE); else o.drawCircle(X0 + 40, 16, 2, SSD1306_WHITE); o.setCursor(X0 + 46, 13); o.print(F("LIVE")); }
  else o.print(F("Musik Almara"));
  if (m.playing) {  // 6 batang berdenyut mengikuti "ketukan"
    float beat = 0.5f + 0.5f * sinf(now / 230.0f);
    beat = beat * beat * beat;
    static float pk[6];
    for (int i = 0; i < 6; i++) {
      float s = 0.5f + 0.5f * sinf(now / (95.0f + i * 29.0f) + i * 1.7f);
      int h = 2 + (int)(s * (5.0f + beat * 6.0f));
      if (h > 11) h = 11;
      o.fillRect(110 + i * 3, 22 - h, 2, h, SSD1306_WHITE);
      if (h > pk[i]) pk[i] = (float)h; else pk[i] -= 0.25f;
      if (pk[i] > h + 1) o.drawPixel(110 + i * 3, 22 - (int)pk[i], SSD1306_WHITE);
    }
  } else {
    o.fillRect(114, 12, 3, 9, SSD1306_WHITE);
    o.fillRect(120, 12, 3, 9, SSD1306_WHITE);
  }

  // baris 3: bilah progres + waktu
  uint32_t el = infoElapsed(now, m);
  char tm[8];
  uint32_t mnt = el / 60;
  if (mnt > 99) mnt = 99;
  snprintf(tm, sizeof(tm), "%u:%02u", (unsigned)mnt, (unsigned)(el % 60));
  o.setCursor(128 - (int)strlen(tm) * 6, 24);
  o.print(tm);
  const int BW = 58, BY = 27;
  int pos;
  if (m.durasi > 0) pos = (int)((uint64_t)BW * (el > m.durasi ? m.durasi : el) / m.durasi);
  else {  // durasi tidak diketahui (radio): titik menyapu bolak-balik
    uint32_t ph = (now / 25) % (2 * BW);
    pos = ph < (uint32_t)BW ? (int)ph : 2 * BW - (int)ph;
    if (!m.playing) pos = 0;
  }
  o.drawFastHLine(X0, BY, BW, SSD1306_WHITE);
  if (m.durasi > 0) o.fillRect(X0, BY - 1, pos, 3, SSD1306_WHITE);
  o.fillCircle(X0 + pos, BY, 2, SSD1306_WHITE);
  if (m.playing && (now / 300) % 2) o.drawCircle(X0 + pos, BY, 3, SSD1306_WHITE);  // denyut di kenop
  infoWipe(now, IM_MUSIC);
  infoShow();
}

// ---------- layar DENGAR: EQUALIZER dari bawah ke atas ----------
// 32 batang (lebar 3, celah 1) memenuhi layar. Tiap batang tumbuh dari dasar layar ke atas dalam segmen
// ala lampu LED; tinggi dasar = kekerasan suara mic (level), dibentuk seperti spektrum suara (tengah lebih
// tinggi, tiap batang bergoyang sendiri) + naik cepat / turun pelan + penanda puncak yang jatuh.
#define INFO_EQ_N 32
#define INFO_EQ_SEG 10  // jumlah segmen per batang (3 px per segmen: 2 menyala + 1 celah)
static void infoDrawListen(uint32_t now, float level) {
  MonoPanel &o = infoOled;
  static float bar[INFO_EQ_N], peak[INFO_EQ_N];
  static uint32_t last = 0;
  if (now - last > 500) { memset(bar, 0, sizeof(bar)); memset(peak, 0, sizeof(peak)); }
  float dtf = (now - last) / 33.0f;
  last = now;
  if (dtf < 0.3f) dtf = 0.3f;
  if (dtf > 3.0f) dtf = 3.0f;

  float lv = infoClamp01(level * 1.5f);
  lv = powf(lv, 0.75f);  // suara pelan tetap kelihatan

  float atk = fminf(1.0f, 0.60f * dtf), dec = fminf(1.0f, 0.16f * dtf);
  o.clearDisplay();
  for (int i = 0; i < INFO_EQ_N; i++) {
    float xr = i / (float)(INFO_EQ_N - 1);
    float env = (0.55f + 0.45f * sinf(xr * 3.14159f)) * (1.0f - 0.30f * xr);  // bentuk spektrum suara
    float v1 = 0.5f + 0.5f * sinf(now / (85.0f + i * 6.0f) + i * 1.9f);
    float v2 = 0.5f + 0.5f * sinf(now / (160.0f - i * 2.0f) + i * 0.7f);
    float shape = 0.20f + 0.80f * (0.6f * v1 + 0.4f * v2);
    float target = lv * env * shape * 1.35f;
    if (lv < 0.03f) target = 0.03f + 0.04f * v1;  // diam: satu-dua segmen bawah berkedip, tetap hidup
    target = infoClamp01(target);
    if (target > bar[i]) bar[i] += (target - bar[i]) * atk;
    else bar[i] += (target - bar[i]) * dec;
    if (bar[i] > peak[i]) peak[i] = bar[i];
    else peak[i] -= 0.012f * dtf;
    if (peak[i] < 0) peak[i] = 0;

    int x = i * 4;
    int n = (int)(bar[i] * INFO_EQ_SEG + 0.5f);
    if (n > INFO_EQ_SEG) n = INFO_EQ_SEG;
    for (int s = 0; s < n; s++) o.fillRect(x, 29 - s * 3, 3, 2, SSD1306_WHITE);  // dari dasar (y=30..31) naik
    int ps = (int)(peak[i] * INFO_EQ_SEG + 0.5f);
    if (ps > INFO_EQ_SEG - 1) ps = INFO_EQ_SEG - 1;
    if (ps > n) o.drawFastHLine(x, 30 - ps * 3, 3, SSD1306_WHITE);  // penanda puncak
  }
  infoWipe(now, IM_LISTEN);
  infoShow();
}

// ---------- layar BERPIKIR ----------
static void infoDrawThink(uint32_t now) {
  MonoPanel &o = infoOled;
  o.clearDisplay();
  o.setTextSize(1);
  o.setTextColor(SSD1306_WHITE);
  o.setTextWrap(false);
  // cincin titik berputar dengan ekor memudar
  const int cx = 18, cy = 16;
  for (int i = 0; i < 9; i++) {
    float a = now / 240.0f - i * 0.42f;
    int rad = i == 0 ? 3 : (i < 3 ? 2 : (i < 6 ? 1 : 0));
    int px = cx + (int)roundf(cosf(a) * 12), py = cy + (int)roundf(sinf(a) * 12);
    if (rad) o.fillCircle(px, py, rad, SSD1306_WHITE); else o.drawPixel(px, py, SSD1306_WHITE);
  }
  float pulse = 0.5f + 0.5f * sinf(now / 190.0f);
  o.drawCircle(cx, cy, 3 + (int)(pulse * 2), SSD1306_WHITE);
  o.fillCircle(cx, cy, 1, SSD1306_WHITE);
  // teks + titik-titik
  o.setCursor(38, 4);
  o.print(F("Berpikir"));
  int nd = (now / 280) % 4;
  for (int i = 0; i < nd; i++) o.fillRect(90 + i * 6, 10, 2, 2, SSD1306_WHITE);
  // pita gelombang yang mengalir di bawah teks
  for (int px = 38; px < 124; px++) {
    float env = sinf((px - 38) / 86.0f * 3.14159f);
    int y = 22 + (int)roundf(sinf(px / 5.0f - now / 110.0f) * 4.0f * env);
    o.drawPixel(px, y, SSD1306_WHITE);
    o.drawPixel(px, y + 1, SSD1306_WHITE);
  }
  infoWipe(now, IM_THINK);
  infoShow();
}

// ---------- layar TEKS: jawaban, 3 baris per halaman ----------
#define INFO_COLS 18   // 18 huruf per baris (kiri disisakan 12 px untuk ikon gelembung bicara)
#define INFO_MAXL 60
static char infoLines[INFO_MAXL][INFO_COLS + 1];
static int infoNL = 0;
static uint32_t infoHash = 0;

static uint32_t infoFnv(const char *s) {
  uint32_t h = 2166136261u;
  for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; }
  return h;
}

static void infoWrap(const char *src) {
  infoNL = 0;
  char word[48];
  int wl = 0, col = 0;
  char cur[INFO_COLS + 1];
  cur[0] = 0;
  auto flush = [&]() {
    if (infoNL < INFO_MAXL) { strlcpy(infoLines[infoNL], cur, INFO_COLS + 1); infoNL++; }
    cur[0] = 0;
    col = 0;
  };
  auto putWord = [&]() {
    if (!wl) return;
    word[wl] = 0;
    while (wl > INFO_COLS) {  // kata sangat panjang: potong
      if (col) flush();
      char part[INFO_COLS + 1];
      memcpy(part, word, INFO_COLS);
      part[INFO_COLS] = 0;
      strlcpy(cur, part, sizeof(cur));
      col = INFO_COLS;
      flush();
      memmove(word, word + INFO_COLS, wl - INFO_COLS + 1);
      wl -= INFO_COLS;
    }
    if (col && col + 1 + wl > INFO_COLS) flush();
    if (col) { cur[col++] = ' '; cur[col] = 0; }
    strlcat(cur, word, sizeof(cur));
    col += wl;
    wl = 0;
  };
  for (const char *p = src; *p; p++) {
    uint8_t c = (uint8_t)*p;
    if (c == '\n') { putWord(); if (col) flush(); continue; }
    if (c == ' ' || c == '\t') { putWord(); continue; }
    if (c < 32 || c >= 127) continue;
    if (wl < (int)sizeof(word) - 1) word[wl++] = (char)c;
  }
  putWord();
  if (col) flush();
}

static void infoDrawText(uint32_t now, const char *raw, uint32_t textSince, float level, bool talking) {
  MonoPanel &o = infoOled;
  const char *ans = raw;
  if (raw[0] == '>' && raw[1] == ' ') {  // "> yang kamu ucapkan\njawaban" -> tampilkan jawabannya saja
    const char *nl = strchr(raw, '\n');
    ans = nl ? nl + 1 : "";
  }
  uint32_t h = infoFnv(ans);
  if (h != infoHash) { infoHash = h; infoWrap(ans); }
  o.clearDisplay();
  o.setTextSize(1);
  o.setTextColor(SSD1306_WHITE);
  o.setTextWrap(false);
  const int TX = 12;                // teks mulai di x=12 (kiri: ikon + jalur titik)
  const uint32_t PAGE_MS = 2600;
  int pages = (infoNL + 2) / 3;
  if (pages < 1) pages = 1;
  uint32_t since = now - textSince;
  int page = pages > 1 ? (int)((since / PAGE_MS) % (uint32_t)pages) : 0;

  static int lastPage = -1;
  static uint32_t lastH = 0, pgAt = 0;
  if (page != lastPage || h != lastH) { lastPage = page; lastH = h; pgAt = now; }
  int32_t dt = (int32_t)(now - pgAt);

  // ikon gelembung bicara; tiga titik di dalamnya melompat bergantian saat Almara bicara
  o.drawRoundRect(0, 1, 9, 7, 2, SSD1306_WHITE);
  o.drawPixel(2, 8, SSD1306_WHITE);
  o.drawPixel(1, 9, SSD1306_WHITE);
  for (int i = 0; i < 3; i++) {
    int dy = talking ? (int)roundf(sinf(now / 110.0f - i * 1.1f) * 1.5f) : 0;
    o.drawPixel(2 + i * 2, 4 + dy, SSD1306_WHITE);
  }
  for (int y = 13; y < 31; y += 3) o.drawPixel(4, y, SSD1306_WHITE);   // jalur titik di bawah ikon

  // garis waktu halaman di atas: terisi sampai halaman berikutnya
  if (pages > 1) {
    int w = (int)((since % PAGE_MS) * 108 / PAGE_MS);
    if (w > 0) o.drawFastHLine(TX, 0, w, SSD1306_WHITE);
  }

  // teks diketik huruf demi huruf (~55 huruf/detik), kursor blok mengikuti
  int remain = dt < 0 ? 0 : dt / 18;
  int total = 0;
  for (int r = 0; r < 3; r++) { int li = page * 3 + r; if (li < infoNL) total += (int)strlen(infoLines[li]); }
  bool typing = (dt / 18) < total;
  int curX = TX, curY = 1;
  bool curSet = false;
  for (int r = 0; r < 3; r++) {
    int li = page * 3 + r;
    if (li >= infoNL) break;
    int n = (int)strlen(infoLines[li]);
    int k = remain < n ? remain : n;
    char buf[INFO_COLS + 1];
    memcpy(buf, infoLines[li], k);
    buf[k] = 0;
    int y = 1 + r * 9;
    o.setCursor(TX, y);
    o.print(buf);
    if (!curSet) { curX = TX + k * 6; curY = y; if (remain <= n) curSet = true; }
    remain -= n;
    if (remain < 0) remain = 0;
  }
  if (typing || ((now / 450) % 2 == 0)) o.fillRect(curX, curY, 5, 7, SSD1306_WHITE);

  if (pages > 1) {  // titik penanda halaman di tepi kanan (halaman aktif = titik besar)
    int gap = pages > 8 ? 3 : 5;
    int totalDots = (pages > 8 ? 8 : pages);
    int y0 = 16 - (totalDots - 1) * gap / 2;
    int cur = pages > 8 ? page * 8 / pages : page;
    for (int i = 0; i < totalDots; i++) {
      if (i == cur) o.fillRect(125, y0 + i * gap - 1, 3, 3, SSD1306_WHITE);
      else o.drawPixel(126, y0 + i * gap, SSD1306_WHITE);
    }
  }
  if (talking) {  // gelombang suara kecil di dasar layar (tumbuh dari bawah)
    static uint8_t wv[54];
    static uint8_t wp = 0;
    float lv = infoClamp01(level * 1.4f);
    static uint32_t wvAt = 0;   // geser gelombang tiap 40 ms (tidak bergantung fps)
    if (now - wvAt >= 40) { wvAt = now; wv[wp] = (uint8_t)(lv * 4); wp = (wp + 1) % 54; }
    for (int i = 0; i < 54; i++) {
      int hh = wv[(wp + i) % 54];
      if (hh) o.drawFastVLine(TX + i * 2, 31 - hh, hh + 1, SSD1306_WHITE);
    }
  }
  infoWipe(now, IM_TEXT);
  infoShow();
}

// ---------- layar BERANDA ----------
static void infoIconTemp(MonoPanel &o, int x, int y) {  // termometer
  o.drawFastVLine(x + 2, y, 5, SSD1306_WHITE);
  o.drawFastVLine(x + 4, y, 5, SSD1306_WHITE);
  o.drawPixel(x + 3, y, SSD1306_WHITE);
  o.fillCircle(x + 3, y + 6, 2, SSD1306_WHITE);
}
static void infoIconDrop(MonoPanel &o, int x, int y) {  // tetes air
  o.fillTriangle(x + 3, y, x + 0, y + 4, x + 6, y + 4, SSD1306_WHITE);
  o.fillCircle(x + 3, y + 5, 3, SSD1306_WHITE);
}
static void infoIconBat(MonoPanel &o, int x, int y, int pct) {  // baterai
  o.drawRect(x, y + 1, 9, 6, SSD1306_WHITE);
  o.fillRect(x + 9, y + 3, 1, 2, SSD1306_WHITE);
  int w = (pct * 7) / 100;
  if (w > 0) o.fillRect(x + 1, y + 2, w, 4, SSD1306_WHITE);
}
static void infoIconNet(MonoPanel &o, int x, int y, bool online) {  // sinyal WiFi
  o.fillCircle(x + 4, y + 7, 1, SSD1306_WHITE);
  if (online) {
    o.drawCircleHelper(x + 4, y + 7, 3, 1 | 2, SSD1306_WHITE);
    o.drawCircleHelper(x + 4, y + 7, 6, 1 | 2, SSD1306_WHITE);
  } else {
    o.drawLine(x, y + 1, x + 8, y + 8, SSD1306_WHITE);
  }
}

static void infoIconSun(MonoPanel &o, int x, int y) {  // matahari = hari
  o.fillCircle(x + 3, y + 3, 2, SSD1306_WHITE);
  o.drawPixel(x + 3, y, SSD1306_WHITE); o.drawPixel(x + 3, y + 6, SSD1306_WHITE);
  o.drawPixel(x, y + 3, SSD1306_WHITE); o.drawPixel(x + 6, y + 3, SSD1306_WHITE);
  o.drawPixel(x + 1, y + 1, SSD1306_WHITE); o.drawPixel(x + 5, y + 1, SSD1306_WHITE);
  o.drawPixel(x + 1, y + 5, SSD1306_WHITE); o.drawPixel(x + 5, y + 5, SSD1306_WHITE);
}
static void infoIconCal(MonoPanel &o, int x, int y) {  // kalender = tanggal
  o.drawRect(x, y + 1, 7, 7, SSD1306_WHITE);
  o.drawFastHLine(x, y + 3, 7, SSD1306_WHITE);
  o.drawPixel(x + 1, y, SSD1306_WHITE); o.drawPixel(x + 5, y, SSD1306_WHITE);
}
static void infoIconMoon(MonoPanel &o, int x, int y) {  // bulan sabit = bulan
  o.fillCircle(x + 3, y + 4, 3, SSD1306_WHITE);
  o.fillCircle(x + 5, y + 3, 2, SSD1306_BLACK);
}

// Satu halaman panel kanan (3 baris: ikon + 3 huruf). yo = geseran vertikal untuk animasi ganti halaman.
static void infoPanelPage(MonoPanel &o, int pg, int yo, float tC, float hum, int bat, bool online,
                          int wday, int mday, int mon) {
  static const char *HARI[7] = {"MIN", "SEN", "SEL", "RAB", "KAM", "JUM", "SAB"};
  static const char *BULAN[12] = {"JAN", "FEB", "MAR", "APR", "MEI", "JUN", "JUL", "AGU", "SEP", "OKT", "NOV", "DES"};
  char b2[16];
  const int ry[3] = {2, 11, 20};
  o.setTextSize(1);
  o.setTextColor(SSD1306_WHITE);
  if (pg == 0) {
    infoIconTemp(o, 96, ry[0] + yo);
    o.setCursor(108, ry[0] + yo);
    if (isnan(tC)) o.print(F("--C")); else { snprintf(b2, sizeof(b2), "%dC", (int)roundf(tC)); o.print(b2); }
    infoIconDrop(o, 96, ry[1] + yo);
    o.setCursor(108, ry[1] + yo);
    if (isnan(hum)) o.print(F("--%")); else { int hv = (int)roundf(hum); if (hv > 99) hv = 99; snprintf(b2, sizeof(b2), "%d%%", hv); o.print(b2); }
    if (bat >= 0) {
      infoIconBat(o, 96, ry[2] + yo, bat);
      o.setCursor(108, ry[2] + yo);
      snprintf(b2, sizeof(b2), "%d%%", bat > 99 ? 99 : bat);
      o.print(b2);
    } else {
      infoIconNet(o, 96, ry[2] - 1 + yo, online);
      o.setCursor(108, ry[2] + yo);
      o.print(online ? F("net") : F("off"));
    }
  } else {
    infoIconSun(o, 96, ry[0] + yo);
    o.setCursor(108, ry[0] + yo);
    o.print(HARI[(wday < 0 ? 0 : wday) % 7]);
    infoIconCal(o, 96, ry[1] + yo);
    o.setCursor(108, ry[1] + yo);
    snprintf(b2, sizeof(b2), "%d", mday);
    o.print(b2);
    infoIconMoon(o, 96, ry[2] + yo);
    o.setCursor(108, ry[2] + yo);
    o.print(BULAN[(mon < 0 ? 0 : mon) % 12]);
  }
}

static void infoPaintClock(uint32_t now, bool timeOk, int hh, int mm, float tC, float hum, int bat, bool online,
                         int wday = -1, int mday = 0, int mon = 0) {
  MonoPanel &o = infoOled;
  o.clearDisplay();
  o.setTextColor(SSD1306_WHITE);
  o.setTextWrap(false);
  o.setTextSize(3);

  if (timeOk) {
    // jam: setiap digit yang berganti meluncur naik (angka lama keluar atas, angka baru masuk dari bawah)
    char b[8];
    snprintf(b, sizeof(b), "%02d%02d", hh, mm);
    static char prev[5] = {'-', '-', '-', '-', 0};
    static char old[4] = {'-', '-', '-', '-'};
    static uint32_t chg[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++)
      if (b[i] != prev[i]) { old[i] = prev[i]; prev[i] = b[i]; chg[i] = now; }
    for (int i = 0; i < 4; i++) {
      int x = i < 2 ? i * 18 : 54 + (i - 2) * 18;
      uint32_t d = now - chg[i];
      if (d < 340) {
        float e = infoEaseOut(d / 340.0f);
        if (old[i] != '-') o.drawChar(x, 4 - (int)(e * 28), old[i], SSD1306_WHITE, SSD1306_WHITE, 3);
        o.drawChar(x, 4 + (int)((1.0f - e) * 28), b[i], SSD1306_WHITE, SSD1306_WHITE, 3);
      } else {
        o.drawChar(x, 4, b[i], SSD1306_WHITE, SSD1306_WHITE, 3);
      }
    }
    if ((now / 500) % 2 == 0) o.drawChar(36, 4, ':', SSD1306_WHITE, SSD1306_WHITE, 3);

    // kilau: pita cahaya miring menyapu digit sekali tiap 7 detik (membalik piksel)
    uint32_t sw = now % 7000;
    if (sw < 700) {
      int bx = -10 + (int)(sw * 106 / 700);
      for (int y = 4; y < 28; y++) {
        int x0 = bx + (27 - y) / 2;
        for (int dx = 0; dx < 5; dx++) {
          int x = x0 + dx;
          if (x >= 0 && x < 90) o.drawPixel(x, y, !o.getPixel(x, y));
        }
      }
    }
  } else {
    o.setCursor(0, 4);
    o.print(F("--:--"));
  }

  // panel kanan: ganti halaman tiap 5 detik (sensor <-> tanggal) dengan geseran vertikal
  bool dateOk = timeOk && wday >= 0;
  const uint32_t PER = 5000;
  int pg = dateOk ? (int)((now / PER) % 2) : 0;
  uint32_t t = now % PER;
  if (dateOk && t < 320) {
    float e = infoEaseOut(t / 320.0f);
    infoPanelPage(o, 1 - pg, -(int)(e * 30), tC, hum, bat, online, wday, mday, mon);       // lama keluar ke atas
    infoPanelPage(o, pg, (int)((1.0f - e) * 30), tC, hum, bat, online, wday, mday, mon);   // baru masuk dari bawah
  } else {
    infoPanelPage(o, pg, 0, tC, hum, bat, online, wday, mday, mon);
  }
  o.fillRect(93, 0, 35, 1, SSD1306_BLACK);    // potong isi yang meluncur keluar dari bingkai
  o.fillRect(93, 29, 35, 3, SSD1306_BLACK);

  // bingkai panel (garis putus-putus) dengan komet cahaya yang berputar mengelilinginya
  const int PERIM = 128;
  int head = (int)((now / 55) % PERIM);
  for (int i = 0; i < PERIM; i++) {
    int dd = (head - i + PERIM) % PERIM;
    bool on = (i % 4 == 0) || dd < 6 || (dd < 14 && dd % 2 == 0);
    if (!on) continue;
    int x, y, k = i;
    if (k < 36) { x = 92 + k; y = 0; }
    else if ((k -= 36) < 29) { x = 127; y = 1 + k; }
    else if ((k -= 29) < 35) { x = 126 - k; y = 29; }
    else { k -= 35; x = 92; y = 28 - k; }
    o.drawPixel(x, y, SSD1306_WHITE);
  }

  // komet menyusuri tepi bawah + garis titik-titik di bawahnya
  for (int x = 2; x < 128; x += 8) o.drawPixel(x, 31, SSD1306_WHITE);
  int headB = (int)((now / 11) % (128 + 44)) - 4;
  for (int k = 0; k < 22; k++) {
    int x = headB - k;
    if (x < 0 || x > 127) continue;
    if (k < 6 || (k % (1 + k / 6)) == 0) o.drawPixel(x, 31, SSD1306_WHITE);
    if (k < 3) o.drawPixel(x, 30, SSD1306_WHITE);
  }
}

// Beranda lama dipertahankan sebagai satu halaman (jam); pembungkus ini menjaga kompatibilitas pemanggilan lama.
static void infoDrawIdle(uint32_t now, bool timeOk, int hh, int mm, float tC, float hum, int bat, bool online,
                         int wday = -1, int mday = 0, int mon = 0) {
  infoPaintClock(now, timeOk, hh, mm, tC, hum, bat, online, wday, mday, mon);
  infoWipe(now, IM_IDLE);
  infoShow();
}

// =====================================================================================================
//  BERANDA BARU: beberapa halaman yang berganti sendiri, supaya layar kecil ini terasa "hidup"
//    JAM       : jam besar + panel suhu / tanggal (halaman lama)
//    MATA      : sepasang mata besar yang berkedip, melirik, tersenyum, jatuh cinta, kaget, mengantuk
//    SUASANA   : termometer + gelas kelembapan bergelombang
// =====================================================================================================
enum InfoPage : uint8_t { IP_CLOCK = 0, IP_EYES, IP_CLIMATE = 3 };   // IP_CLIMATE tetap 3 (nomor tirai transisi)

static inline int infoMod(int a, int m) { int r = a % m; return r < 0 ? r + m : r; }

static void infoHeart(MonoPanel &o, int cx, int cy, int s, uint16_t c) {
  int r = s / 2 + 1;
  o.fillCircle(cx - s / 2, cy - s / 4, r, c);
  o.fillCircle(cx + s / 2, cy - s / 4, r, c);
  o.fillTriangle(cx - s - 1, cy - s / 8, cx + s + 1, cy - s / 8, cx, cy + s, c);
}

// ---------- halaman MATA ----------
struct InfoEyes {
  float lookX = 0, lookY = 0, tx = 0, ty = 0;
  uint32_t nextLook = 0, nextBlink = 1500, blinkAt = 0;
  uint8_t mood = 0;  // 0 normal, 1 senang, 2 kedip sebelah, 3 cinta, 4 curiga, 5 kaget, 6 ngantuk
  uint32_t moodAt = 0, moodUntil = 0, nextMood = 3500, last = 0;
};
static InfoEyes infoEy;

static void infoPaintEyes(uint32_t now, uint32_t idleMs) {
  MonoPanel &o = infoOled;
  InfoEyes &e = infoEy;
  float dtf = e.last ? (now - e.last) / 33.0f : 1.0f;
  e.last = now;
  if (dtf < 0.3f) dtf = 0.3f;
  if (dtf > 3.0f) dtf = 3.0f;
  bool sleepy = idleMs > 50000;

  // arah lirik: berpindah acak, dikejar halus
  if (now >= e.nextLook) {
    uint32_t q = esp_random();
    if (q % 4 == 0) { e.tx = 0; e.ty = 0; }
    else { e.tx = (float)((int)((q >> 4) % 19) - 9); e.ty = (float)((int)((q >> 12) % 9) - 4); }
    e.nextLook = now + 700 + (esp_random() % 2300);
  }
  float k = fminf(1.0f, 0.30f * dtf);
  e.lookX += (e.tx - e.lookX) * k;
  e.lookY += (e.ty - e.lookY) * k;

  // kedip
  if (now >= e.nextBlink) { e.blinkAt = now; e.nextBlink = now + 1800 + (esp_random() % 3600); }
  float open = 1.0f;
  { float p = (now - e.blinkAt) / 150.0f; if (p < 1.0f) open = fmaxf(0.10f, fabsf(1.0f - 2.0f * p)); }

  // suasana hati
  if (e.mood && !sleepy && now >= e.moodUntil) { e.mood = 0; e.nextMood = now + 2500 + (esp_random() % 4500); }
  if (sleepy) e.mood = 6;
  else if (!e.mood && now >= e.nextMood) {
    static const uint16_t MD[7] = {0, 2200, 1000, 2600, 1700, 1300, 0};
    e.mood = 1 + esp_random() % 5;
    e.moodAt = now;
    e.moodUntil = now + MD[e.mood];
  }
  uint8_t mood = e.mood;

  o.clearDisplay();
  o.setTextColor(SSD1306_WHITE);
  o.setTextWrap(false);
  int bob = (int)roundf(sinf(now / 650.0f) * 1.0f);
  int cy = 16 + bob;
  const int CX[2] = {34, 94};
  for (int s = 0; s < 2; s++) {
    int cx = CX[s];
    bool left = s == 0;
    int w = 34, h = 24;
    float op = open;
    if (mood == 2 && !left) op = 0.10f;   // kedip sebelah (mata kanan)
    if (mood == 5) { w = 36; h = 28; op = 1.0f; }
    if (mood == 6) op = fminf(op, 0.55f + 0.1f * sinf(now / 900.0f));
    int hh = (int)(h * op);
    if (hh < 2) hh = 2;

    if (mood == 3) {  // mata hati yang berdenyut
      infoHeart(o, cx, cy - 1, 9 + (int)roundf(2.0f * sinf(now / 150.0f)), SSD1306_WHITE);
      continue;
    }
    int x = cx - w / 2, y = cy - hh / 2;
    int rr = hh / 2 < 9 ? hh / 2 : 9;
    o.fillRoundRect(x, y, w, hh, rr, SSD1306_WHITE);
    if (hh > 10 && mood != 1) {  // biji mata + kilau
      int pr = mood == 5 ? 3 : 7;
      int px = cx + (int)roundf(e.lookX * (mood == 5 ? 0.4f : 1.0f));
      int py = cy + (int)roundf(e.lookY * hh / (float)h);
      o.fillCircle(px, py, pr, SSD1306_BLACK);
      o.fillRect(px - 3, py - 3, 2, 2, SSD1306_WHITE);
      if (pr > 4) o.drawPixel(px + 2, py + 2, SSD1306_WHITE);
    }
    if (mood == 1) o.fillCircle(cx, y + hh + 3, (w * 3) / 4, SSD1306_BLACK);  // lengkung senang ^ ^
    if (mood == 4 && hh > 10) {  // curiga: kelopak miring ke dalam
      int ox = left ? x - 1 : x + w, ix = left ? x + w : x - 1;
      o.fillTriangle(ox, y - 1, ix, y - 1, ix, y + hh / 2, SSD1306_BLACK);
      o.fillRect(x - 1, y + hh - 5, w + 2, 6, SSD1306_BLACK);
    }
    if (mood == 6) o.fillRect(x - 1, y - 1, w + 2, hh / 2 + 1, SSD1306_BLACK);  // kelopak atas turun
  }

  // hiasan sesuai suasana
  if (mood == 1) {  // kilau di pojok
    int t = (now / 120) % 6;
    int r = t < 3 ? t : 5 - t;
    o.drawFastHLine(6 - r, 6, 2 * r + 1, SSD1306_WHITE); o.drawFastVLine(6, 6 - r, 2 * r + 1, SSD1306_WHITE);
    o.drawFastHLine(121 - r, 26, 2 * r + 1, SSD1306_WHITE); o.drawFastVLine(121, 26 - r, 2 * r + 1, SSD1306_WHITE);
  } else if (mood == 3) {  // hati kecil naik
    for (int i = 0; i < 3; i++) {
      int ph = (int)((now / 40 + i * 14) % 44);
      int hx = 62 + (i - 1) * 6 + (int)roundf(sinf(ph / 5.0f + i) * 2);
      int hy = 31 - ph;
      if (hy > 2) infoHeart(o, hx, hy, 1, SSD1306_WHITE);
    }
  } else if (mood == 5) {  // garis kejut
    if ((now / 100) % 2) {
      o.drawLine(2, 3, 8, 8, SSD1306_WHITE); o.drawLine(125, 3, 119, 8, SSD1306_WHITE);
      o.drawLine(2, 29, 8, 24, SSD1306_WHITE); o.drawLine(125, 29, 119, 24, SSD1306_WHITE);
    }
  } else if (mood == 6) {  // z z z
    int ph = (int)((now / 600) % 3);
    o.setTextSize(1);
    for (int i = 0; i <= ph; i++) { o.setCursor(62 + i * 5, 24 - i * 7); o.print('z'); }
  } else if (mood == 0) {  // bintang kecil berkedip di sudut
    static const uint8_t SP[4][2] = {{4, 4}, {123, 5}, {4, 28}, {123, 27}};
    for (int i = 0; i < 4; i++) if (((now / 400) + i * 3) % 9 < 2) o.drawPixel(SP[i][0], SP[i][1], SSD1306_WHITE);
  }
}

// ---------- halaman SUASANA (suhu & kelembapan) ----------
static void infoPaintClimate(uint32_t now, float tC, float hum) {
  MonoPanel &o = infoOled;
  o.clearDisplay();
  o.setTextColor(SSD1306_WHITE);
  o.setTextWrap(false);
  // termometer (tabung + bola + raksa naik sesuai suhu 15..40 C)
  float tf = isnan(tC) ? 0.0f : infoClamp01((tC - 15.0f) / 25.0f);
  o.drawRoundRect(3, 1, 7, 24, 3, SSD1306_WHITE);
  o.fillCircle(6, 26, 4, SSD1306_WHITE);
  int mh = 2 + (int)(tf * 19.0f + 0.5f + sinf(now / 300.0f) * 0.6f);
  o.fillRect(5, 24 - mh, 3, mh + 1, SSD1306_WHITE);
  for (int i = 0; i < 5; i++) o.drawPixel(12, 4 + i * 5, SSD1306_WHITE);
  // angka suhu
  char b[10];
  o.setTextSize(2);
  o.setCursor(17, 2);
  if (isnan(tC)) o.print(F("--.-")); else { snprintf(b, sizeof(b), "%.1f", tC); o.print(b); }
  o.setTextSize(1);
  o.drawCircle(66, 3, 1, SSD1306_WHITE);
  o.setCursor(68, 2);
  o.print('C');
  const char *msg = "SUHU";
  if (!isnan(tC)) msg = tC >= 31.0f ? "GERAH" : (tC >= 28.5f ? "HANGAT" : (tC >= 23.0f ? "NYAMAN" : "DINGIN"));
  o.setCursor(17, 23);
  o.print(msg);
  // gelas kelembapan dengan permukaan air bergelombang + gelembung
  float hf = isnan(hum) ? 0.0f : infoClamp01(hum / 100.0f);
  const int GX = 80, GY = 2, GW = 16, GH = 27;
  o.drawRoundRect(GX, GY, GW, GH, 3, SSD1306_WHITE);
  int level = GH - 4 - (int)(hf * (GH - 7));
  for (int x = 1; x < GW - 1; x++) {
    int wy = GY + level + (int)roundf(sinf(x * 0.9f + now / 220.0f) * 1.5f);
    if (wy < GY + 1) wy = GY + 1;
    o.drawFastVLine(GX + x, wy, GY + GH - 2 - wy + 1, SSD1306_WHITE);
  }
  for (int i = 0; i < 2; i++) {
    int ph = (int)((now / 60 + i * 23) % 40);
    int by = GY + GH - 4 - ph / 2;
    if (by > GY + level + 2) o.drawPixel(GX + 4 + i * 6 + (int)roundf(sinf(ph / 4.0f)), by, SSD1306_BLACK);
  }
  // angka kelembapan
  o.setTextSize(2);
  o.setCursor(100, 2);
  if (isnan(hum)) o.print(F("--")); else { int hv = (int)roundf(hum); if (hv > 99) hv = 99; if (hv < 0) hv = 0; snprintf(b, sizeof(b), "%d", hv); o.print(b); }
  o.setTextSize(1);
  o.setCursor(100, 23);
  o.print(F("RH %"));
}

// ---------- penata halaman beranda (dipanggil infoTask saat layar diam) ----------
static void infoDrawHome(uint32_t now, bool timeOk, int hh, int mm, float tC, float hum, int bat, bool online,
                         int wday, int mday, int mon, uint32_t idleMs) {
  // urutan & lama tiap halaman (ms); total 37 dtk
  static const uint8_t SEQ[5] = {IP_CLOCK, IP_EYES, IP_CLOCK, IP_EYES, IP_CLIMATE};
  static const uint16_t DUR[5] = {8000, 8000, 8000, 8000, 5000};
  uint32_t total = 0;
  for (int i = 0; i < 5; i++) total += DUR[i];
  uint32_t ph = now % total;
  int pg = IP_CLOCK;
  for (int i = 0; i < 5; i++) { if (ph < DUR[i]) { pg = SEQ[i]; break; } ph -= DUR[i]; }

  switch (pg) {
    case IP_EYES: infoPaintEyes(now, idleMs); break;
    case IP_CLIMATE: infoPaintClimate(now, tC, hum); break;
    default: infoPaintClock(now, timeOk, hh, mm, tC, hum, bat, online, wday, mday, mon); break;
  }
  infoWipe(now, 16 + pg);  // tirai tiap ganti halaman
  infoShow();
}
