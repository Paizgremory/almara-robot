// tampilan.h - layar pembuka Almara, mode teks bergaya chat, beranda jam, dan layar belajar (Pomodoro).
#pragma once
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

static const char *UI_HARI[7] = {"Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"};
static const char *UI_BULAN[12] = {"Jan", "Feb", "Mar", "Apr", "Mei", "Jun", "Jul", "Agu", "Sep", "Okt", "Nov", "Des"};

inline volatile uint32_t uiClickAt = 0;   // waktu klik terakhir (diisi saat bunyi klik UI)
inline float uiEaseOut(float t) { t = t < 0 ? 0 : (t > 1 ? 1 : t); return 1 - (1 - t) * (1 - t) * (1 - t); }
inline float uiEaseBack(float t) {  // melewati sedikit lalu kembali (memantul)
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  const float c1 = 1.70158f, c3 = c1 + 1;
  return 1 + c3 * powf(t - 1, 3) + c1 * powf(t - 1, 2);
}

inline int uiTextW(Adafruit_GFX &g, const char *s) {
  int16_t x1, y1;
  uint16_t w, h;
  g.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}

inline int uiGlyphAdvance(const GFXfont *f, char c) {
  if (c < f->first || c > f->last) return 0;
  return f->glyph[c - f->first].xAdvance;
}

// Teks diketik huruf demi huruf (font bawaan 6 px per huruf), rata tengah
inline void uiTypeC(Adafruit_GFX &g, const char *s, int y, uint32_t ms, uint32_t start, uint32_t perChar = 45) {
  if (ms < start) return;
  int n = (int)strlen(s), shown = (int)((ms - start) / perChar);
  if (shown > n) shown = n;
  char buf[40];
  strncpy(buf, s, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  if (shown < (int)strlen(buf)) buf[shown] = 0;
  g.setFont(NULL);
  g.setTextSize(1);
  g.setTextColor(C_ON);
  g.setCursor(64 - n * 3, y);
  g.print(buf);
  if (shown < n && (ms / 120) % 2) g.fillRect(64 - n * 3 + shown * 6, y, 5, 7, C_ON);  // kursor
}

// ================= Layar pembuka: logo muncul, lalu langsung ke wajah =================
static const uint32_t WELCOME_MS = 2100;

inline bool drawWelcome(Adafruit_GFX &g, uint32_t ms, const char *name) {
  if (ms >= WELCOME_MS) return false;
  const GFXfont *F = &FreeSansBold12pt7b;
  g.setFont(F);
  int textW = uiTextW(g, name);
  const int ICON = 20, GAP = 7;
  int x0 = 64 - (ICON + GAP + textW) / 2;  // logo + nama rata tengah
  int cy = 31;

  // 1. Ikon (wajah kecil dalam kotak membulat) membesar dengan sedikit pantulan
  float p = uiEaseBack(ms / 450.0f);
  int sz = (int)(ICON * p);
  if (sz > 1) {
    int ix = x0 + ICON / 2 - sz / 2, iy = cy - sz / 2;
    g.fillRoundRect(ix, iy, sz, sz, sz / 4, C_ON);
    if (sz > 10) {  // dua mata; berkedip sekali
      bool blink = ms > 1350 && ms < 1480;
      int eh = blink ? 1 : sz / 3;
      g.fillRoundRect(ix + sz / 4 - 1, cy - eh / 2, 3, eh, 1, C_OFF);
      g.fillRoundRect(ix + sz * 3 / 4 - 2, cy - eh / 2, 3, eh, 1, C_OFF);
    }
  }

  // 2. Nama muncul dari kiri ke kanan (disapu)
  if (ms > 380) {
    int tx = x0 + ICON + GAP;
    g.setTextColor(C_ON);
    g.setTextSize(1);
    g.setCursor(tx, cy + 8);
    g.print(name);
    int reveal = (int)(uiEaseOut((ms - 380) / 500.0f) * (textW + 2));
    g.fillRect(tx + reveal, cy - 12, textW + 4 - reveal, 26, C_OFF);
  }
  g.setFont(NULL);
  return true;
}

// ================= Ikon kecil untuk header (8x8) =================
inline void uiActIcon(Adafruit_GFX &g, int x, int y, Act act, float level, uint32_t now, uint16_t c) {
  switch (act) {
    case ACT_LISTEN:  // mikrofon dengan gelombang
      g.fillRoundRect(x + 2, y, 3, 5, 1, c);
      g.drawFastHLine(x + 1, y + 6, 5, c);
      g.drawPixel(x + 3, y + 7, c);
      if ((now / 250) % 2) g.drawFastVLine(x + 7, y + 1, 4, c);
      break;
    case ACT_THINK: {  // titik berputar
      for (int i = 0; i < 3; i++) {
        float a = now / 160.0f + i * 2.09f;
        g.drawPixel(x + 3 + (int)(cosf(a) * 3), y + 3 + (int)(sinf(a) * 3), c);
      }
      break;
    }
    case ACT_TALK:  // speaker dengan gelombang
      g.fillRect(x, y + 2, 2, 4, c);
      g.fillTriangle(x + 2, y + 3, x + 4, y, x + 4, y + 7, c);
      if (level > 0.1f) g.drawFastVLine(x + 6, y + 2, 4, c);
      if (level > 0.3f) g.drawFastVLine(x + 8, y + 1, 6, c);
      break;
    case ACT_INFO:  // tanda seru
      g.drawRoundRect(x, y, 8, 8, 2, c);
      g.drawFastVLine(x + 3, y + 2, 3, c);
      g.drawPixel(x + 3, y + 6, c);
      break;
    default: {  // wajah kecil Almara (berkedip)
      bool blink = (now % 3500) < 120;
      g.fillRect(x + 1, y + (blink ? 3 : 1), 2, blink ? 1 : 4, c);
      g.fillRect(x + 5, y + (blink ? 3 : 1), 2, blink ? 1 : 4, c);
      g.drawFastHLine(x + 2, y + 7, 4, c);
      break;
    }
  }
}

// Header bar terbalik (gelap di atas terang) dengan ikon & judul. statusW = lebar ikon status di kanan.
inline void uiHeader(Adafruit_GFX &g, const char *title, Act act, float level, uint32_t now, int statusW) {
  int w = 128 - statusW - (statusW ? 3 : 0);
  g.fillRoundRect(0, 0, w, 12, 3, C_ON);
  uiActIcon(g, 3, 2, act, level, now, C_OFF);
  g.setFont(NULL);
  g.setTextSize(1);
  g.setTextColor(C_OFF);
  int maxChars = (w - 16) / 6;
  char t[24];
  strncpy(t, title, sizeof(t) - 1);
  t[sizeof(t) - 1] = 0;
  if ((int)strlen(t) > maxChars) t[maxChars > 0 ? maxChars : 0] = 0;
  g.setCursor(14, 2);
  g.print(t);
  g.setTextColor(C_ON);
}

// ================= Mode teks bergaya chat =================
// text: "> pesan pengguna\njawaban robot" (baris berawalan "> " = pesan pengguna, gelembung kanan).
struct UiRow { char s[22]; uint8_t kind; };  // kind 0 = robot, 1 = pengguna, 2 = celah antar gelembung

inline int uiWrapInto(const char *src, int cols, UiRow *rows, int max, int n, uint8_t kind) {
  char line[22];
  int len = 0;
  const char *p = src;
  while (*p && n < max) {
    const char *w = p;  // ambil satu kata
    while (*p && *p != ' ' && *p != '\n') p++;
    int wl = (int)(p - w);
    while (wl > cols) {  // kata terlalu panjang: potong paksa
      if (len) { line[len] = 0; strcpy(rows[n].s, line); rows[n++].kind = kind; len = 0; if (n >= max) return n; }
      memcpy(line, w, cols); line[cols] = 0;
      strcpy(rows[n].s, line); rows[n++].kind = kind;
      if (n >= max) return n;
      w += cols; wl -= cols;
    }
    if (len && len + 1 + wl > cols) { line[len] = 0; strcpy(rows[n].s, line); rows[n++].kind = kind; len = 0; if (n >= max) return n; }
    if (len) line[len++] = ' ';
    memcpy(line + len, w, wl);
    len += wl;
    if (*p == '\n') { line[len] = 0; strcpy(rows[n].s, line); rows[n++].kind = kind; len = 0; }
    if (*p) p++;
  }
  if (len && n < max) { line[len] = 0; strcpy(rows[n].s, line); rows[n++].kind = kind; }
  return n;
}

inline void drawTextUI(Adafruit_GFX &g, const char *title, const char *text, Act act, float level, uint32_t now,
                       uint32_t textSince, int statusW) {
  static UiRow rows[140];
  int n = 0;
  // pisahkan pesan pengguna ("> ...") dan jawaban
  const char *robot = text;
  if (text[0] == '>' && text[1] == ' ') {
    const char *nl = strchr(text, '\n');
    char user[300];
    int ul = nl ? (int)(nl - text - 2) : (int)strlen(text) - 2;
    if (ul > (int)sizeof(user) - 1) ul = sizeof(user) - 1;
    memcpy(user, text + 2, ul);
    user[ul] = 0;
    n = uiWrapInto(user, 17, rows, 140, n, 1);
    if (n < 140) { rows[n].s[0] = 0; rows[n++].kind = 2; }
    robot = nl ? nl + 1 : "";
  }
  n = uiWrapInto(robot, 19, rows, 140, n, 0);

  const int top = 14;
  int viewH = (act == ACT_LISTEN) ? 36 : 50;
  int contentH = 0;
  for (int i = 0; i < n; i++) contentH += rows[i].kind == 2 ? 4 : 9;
  float off = 0;
  if (contentH > viewH) {  // gulir halus per piksel: tunggu 1,5 dtk, gulir 14 px/dtk, berhenti 2,5 dtk, ulang
    float maxOff = contentH - viewH;
    float scrollMs = maxOff / 0.014f;
    float period = 1500 + scrollMs + 2500;
    float t = fmodf((float)(now - textSince), period) - 1500;
    off = t <= 0 ? 0 : (t >= scrollMs ? maxOff : t * 0.014f);
  }

  // Isi percakapan
  int y = top - (int)off;
  int i = 0;
  while (i < n) {
    if (rows[i].kind == 2) { y += 4; i++; continue; }
    if (rows[i].kind == 1) {  // gelembung pengguna (rata kanan, berbingkai)
      int j = i, maxw = 0;
      while (j < n && rows[j].kind == 1) { int w = (int)strlen(rows[j].s) * 6; if (w > maxw) maxw = w; j++; }
      int h = (j - i) * 9 + 1;
      int bx = 124 - maxw - 6;
      if (y + h > top - 2 && y < 64) g.drawRoundRect(bx, y - 1, maxw + 6, h + 1, 3, C_ON);
      for (int k = i; k < j; k++) {
        int ry = y + (k - i) * 9 + 1;
        if (ry > top - 9 && ry < 64) { g.setCursor(bx + 3, ry); g.print(rows[k].s); }
      }
      y += h + 1;
      i = j;
      continue;
    }
    // jawaban robot: garis aksen di kiri
    if (y > top - 9 && y < 64) {
      g.drawFastVLine(0, y, 8, C_ON);
      g.setCursor(4, y);
      g.print(rows[i].s);
    }
    y += 9;
    i++;
  }

  if (contentH > viewH) {  // scrollbar tipis
    int barH = viewH * viewH / contentH;
    if (barH < 4) barH = 4;
    int barY = top + (int)((viewH - barH) * (off / (contentH - viewH)));
    g.drawFastVLine(127, barY, barH, C_ON);
  }

  if (act == ACT_THINK && !text[0]) {  // tiga titik melompat
    for (int k = 0; k < 3; k++) {
      int jy = (int)(fabsf(sinf(now / 180.0f + k * 0.9f)) * 6);
      g.fillCircle(52 + k * 12, 40 - jy, 3, C_ON);
    }
  }
  if (act == ACT_LISTEN) {  // gelombang suara di bawah
    g.fillRect(0, 51, 128, 13, C_OFF);
    for (int k = 0; k < 21; k++) {
      float q = 0.3f + 0.7f * fabsf(sinf(now / 80.0f + k * 0.7f));
      int h = 1 + (int)(level * 11 * q);
      g.fillRect(2 + k * 6, 57 - h / 2, 4, h, C_ON);
    }
  }

  g.fillRect(0, 0, 128, 13, C_OFF);  // potong teks yang tergulir ke area header
  uiHeader(g, title, act, level, now, statusW);
}

// ================= Beranda (mode teks saat diam) =================
inline void drawHome(Adafruit_GFX &g, const char *name, bool timeOk, int hh, int mm, int wday, int mday, int mon,
                     float roomT, uint32_t now, int statusW) {
  g.setFont(NULL);
  g.setTextSize(1);
  g.setTextColor(C_ON);
  uiActIcon(g, 0, 1, ACT_IDLE, 0, now, C_ON);
  g.setCursor(11, 1);
  g.print(name);
  if (timeOk) {
    char clk[6], hhs[3], mms[3];
    snprintf(clk, sizeof(clk), "%02d:%02d", hh, mm);
    snprintf(hhs, sizeof(hhs), "%02d", hh);
    snprintf(mms, sizeof(mms), "%02d", mm);
    const GFXfont *F = &FreeSansBold18pt7b;
    g.setFont(F);
    int w = uiTextW(g, clk);
    g.setCursor(64 - w / 2, 39);
    g.print(hhs);
    if ((now / 500) % 2) g.print(':');                            // titik dua berkedip
    else g.setCursor(g.getCursorX() + uiGlyphAdvance(F, ':'), 39);  // lewati tanpa menggambar
    g.print(mms);
    g.setFont(NULL);
    char date[24];
    snprintf(date, sizeof(date), "%s, %d %s", UI_HARI[wday % 7], mday, UI_BULAN[mon % 12]);
    g.setCursor(64 - (int)strlen(date) * 3, 44);
    g.print(date);
  } else {
    g.setFont(&FreeSansBold12pt7b);
    int w = uiTextW(g, name);
    g.setCursor(64 - w / 2, 36);
    g.print(name);
    g.setFont(NULL);
    g.setCursor(64 - 11 * 3, 44);
    g.print("Menyiapkan");
  }
  g.drawFastHLine(8, 54, 112, C_ON);
  g.setCursor(2, 57);
  if (!isnan(roomT)) {
    char t[10];
    snprintf(t, sizeof(t), "%.1f", roomT);
    g.print(t);
    g.drawCircle(g.getCursorX() + 2, 58, 1, C_ON);  // tanda derajat
    g.setCursor(g.getCursorX() + 5, 57);
    g.print("C");
  }
  const char *hint = "Tekan bicara";
  g.setCursor(126 - (int)strlen(hint) * 6, 57);
  g.print(hint);
}

// ================= Layar belajar (Pomodoro) =================
inline void drawFocus(Adafruit_GFX &g, bool rest, uint32_t leftMs, uint32_t totalMs, int sessions, uint32_t now) {
  g.setFont(NULL);
  g.setTextSize(1);
  g.setTextColor(C_ON);
  const char *title = rest ? "Istirahat dulu" : "Fokus belajar";
  g.fillRoundRect(0, 0, 128, 12, 3, C_ON);
  g.setTextColor(C_OFF);
  g.setCursor(64 - (int)strlen(title) * 3, 2);
  g.print(title);
  g.setTextColor(C_ON);

  uint32_t sec = (leftMs + 999) / 1000;
  char t[8];
  snprintf(t, sizeof(t), "%02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
  g.setFont(&FreeSansBold18pt7b);
  int w = uiTextW(g, t);
  g.setCursor(64 - w / 2, 46);
  g.print(t);
  g.setFont(NULL);

  // mata kecil Almara di pojok kanan bawah: menyipit fokus, atau melengkung senang saat istirahat
  for (int i = 0; i < 2; i++) {
    int cx = i ? 121 : 111;
    if (rest) { g.fillCircle(cx, 61, 3, C_ON); g.fillCircle(cx, 63, 3, C_OFF); }
    else { int bob = (int)(sinf(now / 600.0f) * 1); g.fillRoundRect(cx - 3, 59 + bob, 7, 3, 1, C_ON); }
  }

  float p = totalMs ? 1.0f - (float)leftMs / totalMs : 0;  // bilah progres
  g.drawRoundRect(4, 52, 120, 5, 2, C_ON);
  g.fillRect(5, 53, (int)(118 * p), 3, C_ON);

  for (int i = 0; i < sessions && i < 8; i++) g.fillCircle(8 + i * 7, 61, 2, C_ON);  // titik sesi selesai
  if (sessions > 8) { g.setCursor(66, 57); g.print("+"); }
}

// ================= Mode wajah + teks =================
// Wajah digambar penuh di kanvas tersembunyi (mesin animasi yang sama), lalu diperkecil 2x ke
// separuh atas layar (64x32). Piksel hasil = menyala kalau minimal 2 dari 4 piksel asal menyala.
inline void drawMixedFace(Adafruit_GFX &g, GFXcanvas1 &face, int x0 = 32, int y0 = 0) {
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 64; x++) {
      int n = face.getPixel(2 * x, 2 * y) + face.getPixel(2 * x + 1, 2 * y) +
              face.getPixel(2 * x, 2 * y + 1) + face.getPixel(2 * x + 1, 2 * y + 1);
      if (n >= 2) g.drawPixel(x0 + x, y0 + y, C_ON);
    }
}

// Teks di bawah wajah (3 baris terlihat), gulir halus kalau lebih panjang.
inline void drawMixedText(Adafruit_GFX &g, const char *text, Act act, float level, uint32_t now, uint32_t textSince,
                          bool timeOk, int hh, int mm) {
  const int top = 35, viewH = 29;
  g.drawFastHLine(0, 33, 128, C_ON);
  g.setFont(NULL);
  g.setTextSize(1);
  g.setTextColor(C_ON);
  if (act == ACT_LISTEN) {  // gelombang suara
    g.setCursor(2, top);
    g.print("Mendengarkan...");
    for (int k = 0; k < 21; k++) {
      float q = 0.3f + 0.7f * fabsf(sinf(now / 80.0f + k * 0.7f));
      int h = 1 + (int)(level * 12 * q);
      g.fillRect(2 + k * 6, 57 - h / 2, 4, h, C_ON);
    }
    return;
  }
  if (act == ACT_THINK && !text[0]) {
    g.setCursor(2, top);
    g.print("Berpikir");
    for (int k = 0; k < 3; k++) g.fillCircle(56 + k * 7, top + 4 - (int)(fabsf(sinf(now / 180.0f + k)) * 3), 2, C_ON);
    return;
  }
  if (act == ACT_IDLE && now - textSince > 20000) {  // diam lama: jam & petunjuk
    char b[24];
    if (timeOk) snprintf(b, sizeof(b), "%02d:%02d", hh, mm);
    else strcpy(b, "--:--");
    g.setTextSize(2);
    g.setCursor(64 - 30, top + 1);
    g.print(b);
    g.setTextSize(1);
    g.setCursor(64 - 6 * 6, top + 19);
    g.print("Tekan bicara");
    return;
  }
  static UiRow rows[140];
  int n = 0;
  const char *robot = text;
  if (text[0] == '>' && text[1] == ' ') {  // baris pertama = ucapanmu
    const char *nl = strchr(text, '\n');
    char user[200];
    int ul = nl ? (int)(nl - text - 2) : (int)strlen(text) - 2;
    if (ul > (int)sizeof(user) - 1) ul = sizeof(user) - 1;
    memcpy(user, text + 2, ul);
    user[ul] = 0;
    n = uiWrapInto(user, 19, rows, 140, n, 1);
    robot = nl ? nl + 1 : "";
  }
  n = uiWrapInto(robot, 20, rows, 140, n, 0);
  int contentH = n * 9;
  float off = 0;
  if (contentH > viewH) {  // tunggu 1,5 dtk, gulir 12 px/dtk, berhenti 2,5 dtk, ulang
    float maxOff = contentH - viewH, scrollMs = maxOff / 0.012f, period = 1500 + scrollMs + 2500;
    float t = fmodf((float)(now - textSince), period) - 1500;
    off = t <= 0 ? 0 : (t >= scrollMs ? maxOff : t * 0.012f);
    int barH = max(3, viewH * viewH / contentH);
    g.drawFastVLine(127, top + (int)((viewH - barH) * (off / maxOff)), barH, C_ON);
  }
  bool firstUser = true;
  for (int i = 0; i < n; i++) {
    int y = top + i * 9 - (int)off;
    if (y < top - 8 || y > 63) { if (rows[i].kind == 1) firstUser = false; continue; }
    if (rows[i].kind == 1) {  // ucapanmu: tanda panah kecil di baris pertama
      if (firstUser) g.fillTriangle(0, y + 1, 0, y + 5, 3, y + 3, C_ON);
      firstUser = false;
      g.setCursor(6, y);
    } else {
      g.setCursor(1, y);
    }
    g.print(rows[i].s);
  }
  g.fillRect(0, 0, 128, 34, C_OFF);  // potong teks yang tergulir ke area wajah (wajah digambar sesudahnya)
  g.drawFastHLine(0, 33, 128, C_ON);
}

// ================= Bilah volume di tepi kanan =================
// Pop-up "atur volume": muncul membesar dari tengah, angka persen besar, bilah yang mengalir halus,
// panah berdenyut saat ditekan. openAt/closeAt = waktu buka/tutup (closeAt 0 = belum ditutup).
#define VOLPOP_OPEN_MS 260
#define VOLPOP_CLOSE_MS 220
inline float volEaseBack(float p) { float c = 1.7f; p -= 1; return 1 + (c + 1) * p * p * p + c * p * p; }
inline void drawVolumeOverlay(Adafruit_GFX &g, int level, uint32_t now, uint32_t openAt, uint32_t closeAt,
                              uint32_t moveAt, int moveDir) {
  static float shown = -1;
  float target = level * 10.0f;
  if (shown < 0 || (int32_t)(now - openAt) < 20) shown = target;
  shown += (target - shown) * 0.25f;
  if (fabsf(target - shown) < 0.3f) shown = target;
  float sc;
  if (closeAt) { float p = (float)(int32_t)(now - closeAt) / VOLPOP_CLOSE_MS; if (p >= 1) return; sc = 1 - p * p; }
  else { float p = (float)(int32_t)(now - openAt) / VOLPOP_OPEN_MS; sc = p >= 1 ? 1 : volEaseBack(p < 0 ? 0 : p); }
  const int W = 112, H = 44;
  int w = (int)(W * sc), h = (int)(H * sc);
  if (w < 6 || h < 6) return;
  int x = 64 - w / 2, y = 32 - h / 2;
  g.fillRoundRect(x - 2, y - 2, w + 4, h + 4, 7, C_OFF);
  g.drawRoundRect(x, y, w, h, 6, C_ON);
  if (sc < 0.92f || sc > 1.08f) return;  // isi hanya saat hampir penuh (biar tidak berantakan)
  x = 64 - W / 2; y = 32 - H / 2;
  // ikon speaker + gelombang sesuai level
  int ix = x + 6, iy = y + 8;
  g.fillRect(ix, iy + 3, 3, 5, C_ON);
  g.fillTriangle(ix + 3, iy + 3, ix + 7, iy - 1, ix + 7, iy + 11, C_ON);
  g.fillTriangle(ix + 3, iy + 7, ix + 7, iy + 11, ix + 7, iy - 1, C_ON);
  int waves = level == 0 ? 0 : (level <= 3 ? 1 : (level <= 7 ? 2 : 3));
  for (int i = 0; i < waves; i++) {
    int r = 3 + i * 3, cx = ix + 7;
    for (int a = -r * 7 / 10; a <= r * 7 / 10; a++) {
      int dx = (int)sqrtf((float)(r * r - a * a));
      g.drawPixel(cx + dx, iy + 5 + a, C_ON);
    }
  }
  // angka persen besar
  char b[6];
  snprintf(b, sizeof(b), "%d%%", (int)(shown + 0.5f));
  g.setFont(NULL);
  g.setTextSize(2);
  g.setTextColor(C_ON);
  int tw = (int)strlen(b) * 12;
  g.setCursor(x + 66 - tw / 2, y + 6);
  g.print(b);
  g.setTextSize(1);
  // panah kiri/kanan, berdenyut (geser 2 px) saat ditekan
  bool pulse = (int32_t)(now - moveAt) < 160;
  int lx = x + 24 - (pulse && moveDir < 0 ? 2 : 0), rx = x + W - 8 + (pulse && moveDir > 0 ? 2 : 0);
  g.fillTriangle(lx, y + 13, lx + 4, y + 9, lx + 4, y + 17, C_ON);
  g.fillTriangle(rx, y + 13, rx - 4, y + 9, rx - 4, y + 17, C_ON);
  // bilah 10 segmen, segmen terakhir mengisi halus
  int bx = x + 8, by = y + 27, bw = W - 16, bh = 8;
  g.drawRoundRect(bx, by, bw, bh, 3, C_ON);
  int fill = (int)((bw - 4) * shown / 100.0f);
  if (fill > 0) g.fillRect(bx + 2, by + 2, fill, bh - 4, C_ON);
  for (int i = 1; i < 10; i++) g.drawPixel(bx + 2 + (bw - 4) * i / 10, by + bh + 1, C_ON);
}

// ---------------- Notifikasi geser dari atas (gamepad, speaker, mode bicara, gerak) ----------------
enum ToastIcon : uint8_t { TI_NONE, TI_PAD, TI_SPK, TI_MIC, TI_WHEEL, TI_OK, TI_X };

inline void uiIconPad(Adafruit_GFX &g, int x, int y, uint32_t now, bool pulse) {  // stik 22x13
  g.fillRoundRect(x, y + 2, 22, 9, 4, C_ON);
  g.fillCircle(x + 4, y + 10, 3, C_ON);
  g.fillCircle(x + 17, y + 10, 3, C_ON);
  g.drawFastHLine(x + 3, y + 6, 5, C_OFF);   // d-pad
  g.drawFastVLine(x + 5, y + 4, 5, C_OFF);
  bool b = !pulse || (now / 180) % 2;
  g.drawPixel(x + 16, y + 4, C_OFF);         // tombol
  g.drawPixel(x + 18, y + 6, C_OFF);
  if (b) { g.drawPixel(x + 14, y + 6, C_OFF); g.drawPixel(x + 16, y + 8, C_OFF); }
}

inline void uiIconSpk(Adafruit_GFX &g, int x, int y, uint32_t now, bool pulse) {
  g.fillRect(x, y + 4, 4, 6, C_ON);
  g.fillTriangle(x + 3, y + 7, x + 9, y, x + 9, y + 14, C_ON);
  int waves = pulse ? 1 + (now / 220) % 3 : 3;
  for (int i = 0; i < waves; i++) {
    int r = 4 + i * 3;
    for (int a = -r * 7 / 10; a <= r * 7 / 10; a++) g.drawPixel(x + 9 + (int)sqrtf((float)(r * r - a * a)), y + 7 + a, C_ON);
  }
}

inline void uiIconMic(Adafruit_GFX &g, int x, int y) {
  g.fillRoundRect(x + 6, y, 7, 10, 3, C_ON);
  g.drawCircleHelper(x + 9, y + 7, 6, 4 | 8, C_ON);
  g.drawFastVLine(x + 9, y + 13, 2, C_ON);
  g.drawFastHLine(x + 6, y + 14, 7, C_ON);
}

inline void uiIconWheel(Adafruit_GFX &g, int x, int y, uint32_t now) {
  int cx = x + 9, cy = y + 7;
  g.drawCircle(cx, cy, 7, C_ON);
  g.drawCircle(cx, cy, 6, C_ON);
  float a0 = now / 160.0f;
  for (int i = 0; i < 3; i++) {
    float a = a0 + i * 2.094f;
    g.drawLine(cx, cy, cx + (int)(cosf(a) * 5), cy + (int)(sinf(a) * 5), C_ON);
  }
  g.fillCircle(cx, cy, 1, C_ON);
}

// age = ms sejak muncul, dur = lama tampil. spin = titik-titik "menunggu" di baris kedua.
inline void drawToast(Adafruit_GFX &g, uint8_t icon, const char *t1, const char *t2, uint32_t age, uint32_t dur,
                      bool spin, uint32_t now) {
  if (age >= dur) return;
  const int H = 26, IN = 230, OUT = 200;
  float p = age < (uint32_t)IN ? uiEaseBack((float)age / IN) : (age > dur - OUT ? 1 - uiEaseOut((float)(age - (dur - OUT)) / OUT) : 1);
  int y = (int)(-H - 2 + (H + 4) * p);
  if (y < -H) return;
  g.fillRoundRect(3, y - 1, 122, H + 2, 6, C_OFF);
  g.drawRoundRect(4, y, 120, H, 5, C_ON);
  int ix = 9, iy = y + 6;
  bool pulse = spin;
  switch (icon) {
    case TI_PAD: uiIconPad(g, ix - 2, iy, now, pulse); break;
    case TI_SPK: uiIconSpk(g, ix, iy - 1, now, pulse); break;
    case TI_MIC: uiIconMic(g, ix - 1, iy - 1); break;
    case TI_WHEEL: uiIconWheel(g, ix - 1, iy - 1, now); break;
    case TI_OK: {  // centang yang tergambar bertahap
      float q = fminf(1, age / 260.0f);
      int x1 = ix + 1, y1 = iy + 7, x2 = ix + 6, y2 = iy + 12, x3 = ix + 17, y3 = iy + 1;
      if (q < 0.4f) { float k = q / 0.4f; g.drawLine(x1, y1, x1 + (int)((x2 - x1) * k), y1 + (int)((y2 - y1) * k), C_ON); }
      else {
        float k = (q - 0.4f) / 0.6f;
        g.drawLine(x1, y1, x2, y2, C_ON); g.drawLine(x1, y1 + 1, x2, y2 + 1, C_ON);
        g.drawLine(x2, y2, x2 + (int)((x3 - x2) * k), y2 + (int)((y3 - y2) * k), C_ON);
        g.drawLine(x2, y2 + 1, x2 + (int)((x3 - x2) * k), y2 + 1 + (int)((y3 - y2) * k), C_ON);
      }
      break;
    }
    case TI_X:
      g.drawLine(ix + 3, iy + 1, ix + 14, iy + 12, C_ON); g.drawLine(ix + 4, iy + 1, ix + 15, iy + 12, C_ON);
      g.drawLine(ix + 14, iy + 1, ix + 3, iy + 12, C_ON); g.drawLine(ix + 15, iy + 1, ix + 4, iy + 12, C_ON);
      break;
    default: break;
  }
  g.setFont(NULL);
  g.setTextSize(1);
  g.setTextColor(C_ON);
  g.setTextWrap(false);
  g.setCursor(34, y + 4);
  g.print(t1);
  g.setCursor(34, y + 14);
  g.print(t2);
  g.setTextWrap(true);
  if (spin) {
    int x = 34 + (int)strlen(t2) * 6 + 2;
    for (int i = 0; i < 3; i++) {
      int b = (int)(fmaxf(0, sinf(now / 140.0f - i * 0.9f)) * 3);
      if (x + i * 4 < 122) g.fillRect(x + i * 4, y + 20 - b, 2, 2, C_ON);
    }
  }
}

// Mode bicara klik: titik "REC" berdenyut + cincin yang menyusut saat robot menunggu kamu diam.
inline void drawTapListen(Adafruit_GFX &g, uint32_t now, float silenceFrac) {
  int r = 2 + (int)(fabsf(sinf(now / 200.0f)) * 2);
  g.fillCircle(120, 6, r, C_ON);
  if (silenceFrac > 0.05f) {
    int rr = 7 + (int)((1 - silenceFrac) * 4);
    g.drawCircle(120, 6, rr, C_ON);
  }
}

// ---------------- Menu Pengaturan ----------------
struct SetRow { const char *label; char val[12]; };

inline void uiGear(Adafruit_GFX &g, int cx, int cy, uint32_t now) {  // roda gigi kecil berputar
  float a0 = now / 900.0f;
  for (int i = 0; i < 6; i++) {
    float a = a0 + i * 1.047f;
    g.fillRect(cx + (int)roundf(cosf(a) * 4) - 1, cy + (int)roundf(sinf(a) * 4) - 1, 2, 2, C_ON);
  }
  g.drawCircle(cx, cy, 3, C_ON);
}

// hl = posisi sorotan (meluncur halus), scroll = baris teratas (pecahan = sedang menggulir).
inline void drawSettings(Adafruit_GFX &g, const SetRow *rows, int n, int sel, float scroll, float hl, bool edit,
                         uint32_t now, uint32_t savedAt, uint32_t openMs, const char *title = "PENGATURAN") {
  const int TOP = 13, RH = 13;
  g.setFont(NULL);
  g.setTextSize(1);
  g.setTextWrap(false);
  if (!edit) {  // sorotan meluncur halus ke baris terpilih
    int y = (int)roundf(TOP + (hl - scroll) * RH);
    if (y < TOP - 1) y = TOP - 1;
    g.fillRoundRect(0, y, 123, RH - 1, 3, C_ON);
    uint32_t ca = now - uiClickAt;   // denyut garis luar saat diklik
    if (uiClickAt && ca < 320) {
      int k = (int)(ca / 45) + 1;
      g.drawRoundRect(-k, y - k, 123 + 2 * k, RH - 1 + 2 * k, 3 + k, C_ON);
      if (ca < 140) g.fillRect(2, y + 2, 3, RH - 5, C_OFF);
    }
  }
  // baris (muncul berurutan saat menu dibuka)
  for (int i = 0; i < n; i++) {
    float yf = TOP + (i - scroll) * RH;
    if (yf < TOP - RH + 2 || yf > 64) continue;
    int slide = openMs < 400 ? (int)(fmaxf(0, 1 - (openMs - (i - (int)scroll) * 50.0f) / 200.0f) * 60) : 0;
    int y = (int)roundf(yf), x = 3 + slide;
    bool on = fabsf(i - hl) < 0.5f;
    g.setTextColor(on && !edit ? C_OFF : C_ON);
    g.setCursor(x, y + 2);
    g.print(rows[i].label);
    int vw = (int)strlen(rows[i].val) * 6;
    int vx = 120 - vw + slide;
    if (on && edit) {  // nilai yang sedang diubah: kotak terbalik + panah atas/bawah berkedip di kanan
      vx = 114 - vw;
      g.fillRoundRect(vx - 2, y, 125 - vx, RH - 1, 3, C_ON);
      g.setTextColor(C_OFF);
      g.setCursor(vx, y + 2);
      g.print(rows[i].val);
      if ((now / 250) % 2) {
        g.fillTriangle(116, y + 4, 118, y + 2, 120, y + 4, C_OFF);
        g.fillTriangle(116, y + 7, 118, y + 9, 120, y + 7, C_OFF);
      }
    } else {
      g.setCursor(vx, y + 2);
      g.print(rows[i].val);
    }
  }
  {  // bingkai sorotan saat mengubah nilai
    int y = (int)roundf(TOP + (hl - scroll) * RH);
    if (edit) g.drawRoundRect(0, y, 123, RH - 1, 3, C_ON);
  }
  // judul (digambar setelah baris supaya baris yang menggulir tidak menimpanya)
  g.fillRect(0, 0, 128, 11, C_OFF);
  uiGear(g, 5, 5, now);
  g.setTextColor(C_ON);
  g.setCursor(13, 1);
  g.print(title);
  char pos[8];
  snprintf(pos, sizeof(pos), "%d/%d", sel + 1, n);
  g.setCursor(126 - (int)strlen(pos) * 6, 1);
  g.print(pos);
  g.drawFastHLine(0, 10, 128, C_ON);
  // batang gulir
  int track = 64 - TOP, bh = max(6, track * 4 / n);
  int by = TOP + (int)((track - bh) * scroll / fmaxf(1, n - 4));
  g.drawFastVLine(126, TOP, track, C_ON);
  g.fillRect(125, by, 3, bh, C_ON);
  // "Tersimpan" sebentar setelah menyimpan
  uint32_t sa = now - savedAt;
  if (savedAt && sa < 900) {
    int w = 64, x = 32, yy = 26 + (sa < 150 ? (int)((150 - sa) / 15) : 0);
    g.fillRoundRect(x - 2, yy - 2, w + 4, 16, 4, C_OFF);
    g.drawRoundRect(x, yy, w, 12, 3, C_ON);
    g.setTextColor(C_ON);
    g.setCursor(x + 5, yy + 2);
    g.print("Tersimpan");
  }
  g.setTextColor(C_ON);
  g.setTextWrap(true);
}

// Saat LCD terpasang: pemberitahuan cukup ikon kecil di pojok kanan atas OLED (teksnya di LCD).
inline void drawToastBadge(Adafruit_GFX &g, uint8_t icon, uint32_t age, uint32_t dur, bool spin, uint32_t now) {
  if (age >= dur) return;
  float p = age < 200 ? uiEaseBack(age / 200.0f) : (age > dur - 200 ? 1 - uiEaseOut((age - (dur - 200)) / 200.0f) : 1);
  int y = (int)(-18 + 20 * p);
  if (y < -16) return;
  g.fillRoundRect(98, y - 1, 30, 18, 5, C_OFF);
  g.drawRoundRect(99, y, 28, 16, 4, C_ON);
  switch (icon) {
    case TI_PAD: uiIconPad(g, 102, y + 2, now, spin); break;
    case TI_SPK: uiIconSpk(g, 104, y + 1, now, spin); break;
    case TI_MIC: uiIconMic(g, 103, y + 1); break;
    case TI_WHEEL: uiIconWheel(g, 104, y + 1, now); break;
    case TI_OK: g.drawLine(106, y + 8, 110, y + 12, C_ON); g.drawLine(110, y + 12, 119, y + 3, C_ON);
                g.drawLine(106, y + 9, 110, y + 13, C_ON); g.drawLine(110, y + 13, 119, y + 4, C_ON); break;
    case TI_X: g.drawLine(107, y + 3, 118, y + 12, C_ON); g.drawLine(118, y + 3, 107, y + 12, C_ON); break;
    default: break;
  }
}

