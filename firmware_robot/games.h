// games.h - menu game & 22 game untuk OLED 128x64 dengan 3 tombol (kiri, OK, kanan).
// Semua game berjalan di robot (tanpa internet). Waktu memakai dt (milidetik) supaya
// kecepatan sama di fps berapa pun. Bunyi dikirim lewat gameSfx() (diputar oleh inti lain).
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>

// ---- Disediakan oleh firmware ----
void gameSfx(int freq, int ms);  // antrekan nada (tidak memblokir)
extern volatile float gameMicLevel;  // 0..1 kekerasan suara dari mic (untuk game Teriak Terbang)
extern volatile bool gameWantMic;    // true = game sedang butuh mic
extern int gameHigh[];            // rekor tiap game
extern volatile bool gameHighDirty;

struct GBtn {
  bool l = false, ok = false, r = false;     // sedang ditahan
  bool lP = false, okP = false, rP = false;  // baru ditekan (frame ini)
  // Gamepad X3: atas/bawah, B = kembali, arah mutlak (0 kanan, 1 bawah, 2 kiri, 3 atas; 255 = tidak ada)
  bool u = false, d = false, uP = false, dP = false, backP = false;
  uint8_t padDir = 255, padHold = 255;
};

static const int NGAMES = 22;
static const char *GAME_NAMES[NGAMES] = {"Burung", "Lari Dino", "Ular", "Pecah Bata", "Pong",
                                         "Tangkap Bintang", "Ingat Urutan", "Refleks", "Balok",
                                         "Tembak Alien", "Teriak Terbang", "Irama", "Balapan", "Lompat Awan",
                                         "Helikopter", "Tumpuk Balok", "Pukul Tikus", "Besar Kecil",
                                         "Hitung Kilat", "Tiga Sebaris", "Seberang Jalan", "Labirin"};
static const char *GAME_HINTS[NGAMES] = {"OK: terbang", "OK: lompat", "Kiri/kanan: belok", "Kiri/kanan: gerak",
                                         "Kiri naik/kanan turun", "Kiri/kanan: gerak", "Tiru urutannya",
                                         "Ikuti arah lirikan", "OK putar, K+K jatuh", "OK: tembak",
                                         "Bersuara = naik!", "Tekan saat not tiba", "Kiri/kanan: pindah",
                                         "Kiri/kanan: gerak", "Tahan OK: naik", "OK: jatuhkan balok",
                                         "Pukul tikus, awas bom", "Kecil < atau > Besar",
                                         "Salah < atau > Benar", "Geser lalu OK", "OK maju, kiri/kanan",
                                         "Belok kiri/kanan, OK"};
static const int GAME_VOICE = 10;  // indeks game yang memakai mic

enum GState : uint8_t { GS_MENU, GS_PLAY, GS_OVER };

struct GameCtx {
  GState st = GS_MENU;
  int sel = 0;           // pilihan di menu (NGAMES = "Keluar")
  float carousel = 0;    // posisi korsel (animasi halus menuju sel)
  int cur = 0;           // game yang dimainkan
  int score = 0;
  bool newRecord = false, won = false;
  uint32_t overAt = 0, stateAt = 0;
  uint32_t seed = 0x9e3779b9;
  uint32_t rnd() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
  int range(int a, int b) { return a + (int)(rnd() % (uint32_t)(b - a + 1)); }
  float frand() { return (rnd() & 0xFFFF) / 65535.0f; }
};

inline void gPrintC(Adafruit_GFX &g, const char *s, int y, int size = 1) {  // teks rata tengah
  g.setTextSize(size);
  g.setTextColor(C_ON);
  g.setCursor(64 - (int)strlen(s) * 3 * size, y);
  g.print(s);
}

// Hati kecil 5x4 piksel untuk nyawa
inline void gHeart(Adafruit_GFX &g, int x, int y) {
  static const uint8_t rows[4] = {0b01010, 0b11111, 0b01110, 0b00100};
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 5; c++)
      if (rows[r] >> (4 - c) & 1) g.drawPixel(x + c, y + r, C_ON);
}

inline void gHud(Adafruit_GFX &g, int score, int lives = -1) {
  char b[12];
  snprintf(b, sizeof(b), "%d", score);
  g.setTextSize(1);
  g.setTextColor(C_ON);
  g.setCursor(126 - (int)strlen(b) * 6, 0);
  g.print(b);
  for (int i = 0; i < lives && i < 5; i++) gHeart(g, 1 + i * 7, 1);
}

// =================== 1. Burung ===================
struct GFlappy {
  float y, vy, px[3];
  int gap[3];
  bool passed[3];
  float speed;
};

// =================== 2. Lari Dino ===================
struct GDino {
  float y, vy, dist, speed;
  struct Ob { float x; int w, h, kind; } ob[3];  // kind 0 = kaktus, 1 = burung
  float nextGap;
};

// =================== 3. Ular ===================
struct GSnake {
  static const int W = 32, H = 14, MAXLEN = 200;
  uint8_t x[MAXLEN], y[MAXLEN];
  int len, dir, nextDir, fx, fy;
  float acc, interval;
};

// =================== 4. Pecah Bata ===================
struct GBreak {
  float px, bx, by, vx, vy;
  bool stuck;
  uint8_t bricks[4][8];
  int lives, level, left;
};

// =================== 5. Pong ===================
struct GPong {
  float py, ay, bx, by, vx, vy;
  float aiErr;  // salah perkiraan robot (piksel), diacak tiap kali kamu memukul
  int sp, sa;
  uint32_t serveAt;
};

// =================== 6. Tangkap Bintang ===================
struct GCatch {
  float bx;
  struct It { float x, y, v; int kind; bool on; } it[6];  // kind 0 bintang, 1 bom, 2 hati
  float spawn, interval;
  int lives;
  uint32_t hitAt;
};

// =================== 7. Ingat Urutan ===================
struct GSimon {
  uint8_t seq[64];
  int len, pos, phase;  // phase 0 = robot memperagakan, 1 = giliranmu, 2 = jeda benar
  int lit;
  uint32_t t;
};

// =================== 8. Refleks ===================
struct GReflex {
  int dir;          // 0 kiri, 1 atas (OK), 2 kanan, -1 menunggu
  uint32_t t, window;
  float lookX, lookY;
  uint32_t happyAt;
};

// =================== 9. Balok ===================
struct GTetris {
  static const int W = 10, H = 18;
  uint8_t board[H][W];
  int px, py, rot, type, next, lines;
  float fall;
  uint32_t lHeld, rHeld;
};

// =================== 10. Tembak Alien ===================
struct GInvade {
  float sx, ax, ay, adir;
  bool alive[3][6];
  struct Shot { float x, y; bool on; } shot[3], bomb[4];
  int lives, wave, left;
  float bombAcc;
  uint32_t hitAt;
};

// =================== 11. Teriak Terbang ===================
struct GVoice {
  float y, vy, px[3], speed;
  int gap[3];
  bool passed[3];
};

// =================== 12. Irama ===================
struct GRhythm {
  struct Note { float y; int lane; bool on; } n[12];
  float acc, interval;
  int lives, combo, lastLane, flashLane;
  uint32_t flashAt, missAt;
};

// =================== 13. Balapan ===================
struct GRace {
  int lane;
  float carX, dist, speed, nextGap, sinceSpawn;
  struct Car { int lane; float y; bool on; } e[5];
};

// =================== 14. Lompat Awan ===================
struct GJump {
  float x, y, vy;
  struct Pl { float x, y; int w; bool on; } p[7];
  float height;
};

// =================== 15. Helikopter ===================
struct GCopter {
  static const int N = 34;  // kolom gua, lebar 4 piksel
  float y, vy, off, center, gap;
  int dist;
  int8_t top[N], bot[N], obs[N];
};

// =================== 16. Tumpuk Balok ===================
struct GStack {
  int16_t lx[40], lw[40];
  int n;
  float x, dir, speed;
  uint32_t perfectAt;
};

// =================== 17. Pukul Tikus ===================
struct GMole {
  int hole, kind, lives, fxHole;
  uint32_t upAt, until, next, fxAt;
  bool fxHit;
};

// =================== 18. Besar Kecil ===================
struct GHiLo {
  int cur, nxt, phase;
  uint32_t t;
  bool ok;
};

// =================== 19. Hitung Kilat ===================
struct GMath {
  int a, b, op, res, shown, lives;
  bool truth, fbOk;
  uint32_t t, window, fbAt;
};

// =================== 20. Tiga Sebaris ===================
struct GTTT {
  uint8_t b[9];
  int cur, phase, winner;
  int8_t line[3];
  uint32_t t;
};

// =================== 21. Seberang Jalan ===================
struct GCross {
  int col, row, lives, nCar[4];
  float cx[4][3], sp[4];
  int cw[4];
  uint32_t hitAt;
};

// =================== 22. Labirin ===================
struct GMaze {
  static const int W = 15, H = 7;
  uint8_t wall[H][W];
  int px, py, dir;
  uint32_t t0, limit, lastMove;
};

struct Games {
  GameCtx c;
  GFlappy fl;
  GDino di;
  GSnake sn;
  GBreak br;
  GPong po;
  GCatch ca;
  GSimon si;
  GReflex re;
  GTetris te;
  GInvade in;
  GVoice vo;
  GRhythm rh;
  GRace ra;
  GJump ju;
  GCopter co;
  GStack st;
  GMole mo;
  GHiLo hl;
  GMath ma;
  GTTT tt;
  GCross cr;
  GMaze mz;
};

inline void resetExtra(Games &G, uint32_t now);  // game 15-22 (di bawah)

// ---------------- Mulai ulang tiap game ----------------
inline void gameReset(Games &G, uint32_t now) {
  GameCtx &c = G.c;
  c.score = 0;
  c.newRecord = c.won = false;
  c.stateAt = now;
  switch (c.cur) {
    case 0: {
      GFlappy &f = G.fl;
      f.y = 30; f.vy = 0; f.speed = 38;
      for (int i = 0; i < 3; i++) { f.px[i] = 128 + i * 52; f.gap[i] = c.range(22, 48); f.passed[i] = false; }
      break;
    }
    case 1: {
      GDino &d = G.di;
      d.y = 0; d.vy = 0; d.dist = 0; d.speed = 55; d.nextGap = 60;
      for (int i = 0; i < 3; i++) d.ob[i].x = -50;
      break;
    }
    case 2: {
      GSnake &s = G.sn;
      s.len = 4; s.dir = s.nextDir = 0; s.acc = 0; s.interval = 150;
      for (int i = 0; i < s.len; i++) { s.x[i] = 10 - i; s.y[i] = 7; }
      s.fx = 22; s.fy = 7;
      break;
    }
    case 3: {
      GBreak &b = G.br;
      b.px = 54; b.stuck = true; b.lives = 3; b.level = 1; b.left = 32;
      memset(b.bricks, 1, sizeof(b.bricks));
      break;
    }
    case 4: {
      GPong &p = G.po;
      p.py = p.ay = 30; p.bx = 64; p.by = 34; p.vx = 0; p.vy = 0; p.sp = p.sa = 0; p.serveAt = now + 900;
      p.aiErr = 0;
      break;
    }
    case 5: {
      GCatch &k = G.ca;
      k.bx = 56; k.spawn = 0; k.interval = 1100; k.lives = 3; k.hitAt = 0;
      for (auto &i : k.it) i.on = false;
      break;
    }
    case 6: {
      GSimon &s = G.si;
      s.len = 1; s.pos = 0; s.phase = 0; s.lit = -1; s.t = now + 700;
      for (auto &v : s.seq) v = c.range(0, 2);
      break;
    }
    case 7: {
      GReflex &r = G.re;
      r.dir = -1; r.t = now + 900; r.window = 1300; r.lookX = r.lookY = 0; r.happyAt = 0;
      break;
    }
    case 8: {
      GTetris &t = G.te;
      memset(t.board, 0, sizeof(t.board));
      t.lines = 0; t.fall = 0; t.lHeld = t.rHeld = 0;
      t.next = c.range(0, 6);
      t.type = c.range(0, 6); t.rot = 0; t.px = 3; t.py = 0;
      break;
    }
    case 9: {
      GInvade &v = G.in;
      v.sx = 60; v.lives = 3; v.wave = 1; v.bombAcc = 0; v.hitAt = 0;
      v.ax = 20; v.ay = 12; v.adir = 1; v.left = 18;
      for (auto &r : v.alive) for (auto &a : r) a = true;
      for (auto &x : v.shot) x.on = false;
      for (auto &x : v.bomb) x.on = false;
      break;
    }
    case 10: {
      GVoice &v = G.vo;
      v.y = 34; v.vy = 0; v.speed = 30;
      for (int i = 0; i < 3; i++) { v.px[i] = 140 + i * 58; v.gap[i] = c.range(24, 46); v.passed[i] = false; }
      break;
    }
    case 11: {
      GRhythm &r = G.rh;
      for (auto &n : r.n) n.on = false;
      r.acc = 0; r.interval = 750; r.lives = 5; r.combo = 0; r.lastLane = 1; r.flashLane = -1;
      r.flashAt = r.missAt = 0;
      break;
    }
    case 12: {
      GRace &r = G.ra;
      r.lane = 1; r.carX = 64; r.dist = 0; r.speed = 50; r.nextGap = 40; r.sinceSpawn = 0;
      for (auto &e : r.e) e.on = false;
      break;
    }
    case 14: case 15: case 16: case 17: case 18: case 19: case 20: case 21:
      resetExtra(G, now);
      break;
    case 13: {
      GJump &j = G.ju;
      j.x = 60; j.y = 40; j.vy = -150; j.height = 0;
      for (int i = 0; i < 7; i++) {
        j.p[i].on = true;
        j.p[i].w = 18;
        j.p[i].y = 58 - i * 10;
        j.p[i].x = i == 0 ? 54 : c.range(4, 104);
      }
      break;
    }
  }
}

inline void gameOver(Games &G, uint32_t now, bool won = false) {
  GameCtx &c = G.c;
  c.st = GS_OVER;
  c.overAt = now;
  c.won = won;
  if (c.score > gameHigh[c.cur]) {
    gameHigh[c.cur] = c.score;
    gameHighDirty = true;
    c.newRecord = true;
    gameSfx(784, 90); gameSfx(988, 90); gameSfx(1319, 180);
  } else {
    gameSfx(392, 140); gameSfx(262, 260);
  }
}

// ---------------- Logika per game ----------------
inline void stepFlappy(Games &G, const GBtn &b, float dt, uint32_t now) {
  GFlappy &f = G.fl;
  float s = dt / 1000.0f;
  if (b.okP || b.lP || b.rP) { f.vy = -92; gameSfx(1100, 25); }  // naik ~12 piksel per kepakan
  f.vy += 360 * s;
  f.y += f.vy * s;
  f.speed = fminf(38 + G.c.score * 1.6f, 70);
  for (int i = 0; i < 3; i++) {
    f.px[i] -= f.speed * s;
    if (f.px[i] < -12) {
      float far = fmaxf(fmaxf(f.px[0], f.px[1]), f.px[2]);
      f.px[i] = far + 52;
      f.gap[i] = G.c.range(22, 48);
      f.passed[i] = false;
    }
    int gh = G.c.score > 20 ? 22 : (G.c.score > 8 ? 24 : 26);  // celah menyempit bertahap
    bool inX = f.px[i] < 27 && f.px[i] + 10 > 21;
    if (inX && (f.y - 3 < f.gap[i] - gh / 2 || f.y + 3 > f.gap[i] + gh / 2)) { gameOver(G, now); return; }
    if (!f.passed[i] && f.px[i] + 10 < 21) { f.passed[i] = true; G.c.score++; gameSfx(1568, 30); }
  }
  if (f.y > 61 || f.y < 9) gameOver(G, now);
}

inline void stepDino(Games &G, const GBtn &b, float dt, uint32_t now) {
  GDino &d = G.di;
  float s = dt / 1000.0f;
  if ((b.okP || b.rP || b.lP) && d.y >= 0) { d.vy = -205; gameSfx(880, 30); }
  d.vy += 640 * s;
  d.y += d.vy * s;
  if (d.y > 0) { d.y = 0; d.vy = 0; }
  d.speed = fminf(55 + d.dist / 60, 120);
  d.dist += d.speed * s;
  int sc = (int)(d.dist / 25);
  if (sc / 50 > G.c.score / 50 && sc > 0) gameSfx(1319, 40);
  G.c.score = sc;
  float far = -50;
  for (auto &o : d.ob) {
    o.x -= d.speed * s;
    far = fmaxf(far, o.x);
  }
  for (auto &o : d.ob) {
    if (o.x < -12 && far < 128 - d.nextGap) {
      o.x = 132;
      o.kind = (G.c.score > 12 && G.c.range(0, 3) == 0) ? 1 : 0;
      o.w = o.kind ? 8 : G.c.range(4, 8);
      o.h = o.kind ? 5 : G.c.range(8, 12);
      d.nextGap = G.c.range(42, 80) + d.speed / 4;
      far = o.x;
    }
    // tabrakan: dino x 12..21, kaki di 56 + y
    int top = o.kind ? 40 : 56 - o.h, bot = o.kind ? 45 : 56;
    int dTop = 44 + (int)d.y, dBot = 56 + (int)d.y;
    if (o.x < 20 && o.x + o.w > 13 && dBot > top + 1 && dTop < bot - 1) { gameOver(G, now); return; }
  }
}

inline void stepSnake(Games &G, const GBtn &b, float dt, uint32_t now) {
  GSnake &s = G.sn;
  if (b.padDir != 255) {  // gamepad: arah mutlak (tidak bisa langsung berbalik arah)
    if (b.padDir != (s.dir + 2) % 4 && b.padDir != s.nextDir) { s.nextDir = b.padDir; gameSfx(660, 15); }
  } else {
    if (b.lP) { s.nextDir = (s.dir + 3) % 4; gameSfx(660, 15); }  // belok kiri (relatif)
    if (b.rP) { s.nextDir = (s.dir + 1) % 4; gameSfx(660, 15); }  // belok kanan
  }
  s.acc += dt;
  if (s.acc < s.interval) return;
  s.acc -= s.interval;
  s.dir = s.nextDir;
  static const int DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};
  int nx = s.x[0] + DX[s.dir], ny = s.y[0] + DY[s.dir];
  if (nx < 0 || ny < 0 || nx >= GSnake::W || ny >= GSnake::H) { gameOver(G, now); return; }
  for (int i = 0; i < s.len - 1; i++)
    if (s.x[i] == nx && s.y[i] == ny) { gameOver(G, now); return; }
  bool eat = nx == s.fx && ny == s.fy;
  if (eat && s.len < GSnake::MAXLEN) s.len++;
  for (int i = s.len - 1; i > 0; i--) { s.x[i] = s.x[i - 1]; s.y[i] = s.y[i - 1]; }
  s.x[0] = nx; s.y[0] = ny;
  if (eat) {
    G.c.score++;
    gameSfx(1046, 40);
    s.interval = fmaxf(65, 150 - G.c.score * 4);
    for (int tries = 0; tries < 200; tries++) {  // makanan di kotak kosong
      int fx = G.c.range(0, GSnake::W - 1), fy = G.c.range(0, GSnake::H - 1);
      bool busy = false;
      for (int i = 0; i < s.len; i++) if (s.x[i] == fx && s.y[i] == fy) { busy = true; break; }
      if (!busy) { s.fx = fx; s.fy = fy; break; }
    }
  }
}

inline void stepBreak(Games &G, const GBtn &b, float dt, uint32_t now) {
  GBreak &k = G.br;
  float s = dt / 1000.0f;
  if (b.l) k.px -= 105 * s;
  if (b.r) k.px += 105 * s;
  k.px = fmaxf(0, fminf(108, k.px));
  if (k.stuck) {
    k.bx = k.px + 10; k.by = 57;
    if (b.okP) {
      k.stuck = false;
      float sp = 62 + k.level * 8;
      k.vx = (G.c.frand() - 0.5f) * sp; k.vy = -sp;
      gameSfx(880, 30);
    }
    return;
  }
  // langkah kecil supaya bola cepat tidak menembus bata
  int sub = 3;
  for (int n = 0; n < sub; n++) {
    k.bx += k.vx * s / sub;
    k.by += k.vy * s / sub;
    if (k.bx < 1) { k.bx = 1; k.vx = fabsf(k.vx); }
    if (k.bx > 126) { k.bx = 126; k.vx = -fabsf(k.vx); }
    if (k.by < 9) { k.by = 9; k.vy = fabsf(k.vy); }
    if (k.vy > 0 && k.by >= 58 && k.by <= 61 && k.bx >= k.px - 1 && k.bx <= k.px + 21) {
      float hit = (k.bx - (k.px + 10)) / 10.0f;  // -1..1: pantulan tergantung titik kena
      float sp = sqrtf(k.vx * k.vx + k.vy * k.vy) * 1.02f;
      sp = fminf(sp, 150);
      k.vx = hit * sp * 0.75f;
      k.vy = -sqrtf(fmaxf(sp * sp - k.vx * k.vx, 400));
      gameSfx(660, 20);
    }
    int col = (int)(k.bx - 1) / 16, row = (int)(k.by - 11) / 6;
    if (k.by >= 11 && row >= 0 && row < 4 && col >= 0 && col < 8 && k.bricks[row][col]) {
      k.bricks[row][col] = 0;
      k.vy = -k.vy;
      k.left--;
      G.c.score += 4 - row;
      gameSfx(1200 + row * 150, 25);
      if (!k.left) {  // level berikutnya
        k.level++;
        memset(k.bricks, 1, sizeof(k.bricks));
        k.left = 32;
        k.stuck = true;
        gameSfx(1319, 120);
        return;
      }
    }
  }
  if (k.by > 64) {
    k.lives--;
    gameSfx(300, 120);
    if (k.lives <= 0) { gameOver(G, now); return; }
    k.stuck = true;
  }
}

inline void stepPong(Games &G, const GBtn &b, float dt, uint32_t now) {
  GPong &p = G.po;
  float s = dt / 1000.0f;
  if (b.l) p.py -= 85 * s;
  if (b.r) p.py += 85 * s;
  p.py = fmaxf(16, fminf(56, p.py));
  float aiSpeed = 44 + (p.sp + p.sa) * 2;  // lawan makin cekatan
  // robot baru bereaksi setelah bola melewati tengah, dan tebakannya kadang meleset
  float target = (p.vx > 0 && p.bx > 50) ? p.by + p.aiErr : 36;
  if (fabsf(target - p.ay) > 2) p.ay += (target > p.ay ? 1 : -1) * aiSpeed * s;
  p.ay = fmaxf(16, fminf(56, p.ay));
  if (now < p.serveAt) { p.bx = 64; p.by = 36; return; }
  if (p.vx == 0) {
    float dir = ((p.sp + p.sa) % 2) ? 1 : -1;
    p.vx = 58 * dir; p.vy = (G.c.frand() - 0.5f) * 60;
    p.aiErr = (G.c.frand() * 2 - 1) * 5;
  }
  p.bx += p.vx * s;
  p.by += p.vy * s;
  if (p.by < 10) { p.by = 10; p.vy = fabsf(p.vy); gameSfx(500, 15); }
  if (p.by > 62) { p.by = 62; p.vy = -fabsf(p.vy); gameSfx(500, 15); }
  auto bounce = [&](float padY, float x, float dir) {
    if (fabsf(p.by - padY) <= 8) {
      float hit = (p.by - padY) / 8.0f;
      float sp = fminf(sqrtf(p.vx * p.vx + p.vy * p.vy) * 1.06f, 130);
      p.vy = hit * sp * 0.85f;  // kena ujung raket = sudut tajam
      if (dir > 0) {            // kamu memukul: robot menebak arah bola, kadang meleset
        float miss = 4 + (p.sp + p.sa) * 0.3f;
        p.aiErr = (G.c.frand() * 2 - 1) * (G.c.range(0, 2) == 0 ? miss + 7 : miss);
      }
      p.vx = dir * sqrtf(fmaxf(sp * sp - p.vy * p.vy, 900));
      p.bx = x;
      gameSfx(dir > 0 ? 880 : 700, 20);
      return true;
    }
    return false;
  };
  if (p.vx < 0 && p.bx <= 6 && p.bx > 2) bounce(p.py, 6, 1);
  if (p.vx > 0 && p.bx >= 121 && p.bx < 125) bounce(p.ay, 121, -1);
  if (p.bx < 0 || p.bx > 128) {
    if (p.bx > 128) { p.sp++; G.c.score++; gameSfx(1319, 80); }
    else { p.sa++; gameSfx(330, 120); }
    p.vx = 0;
    p.serveAt = now + 800;
    if (p.sp >= 5 || p.sa >= 5) gameOver(G, now, p.sp >= 5);
  }
}

inline void stepCatch(Games &G, const GBtn &b, float dt, uint32_t now) {
  GCatch &k = G.ca;
  float s = dt / 1000.0f;
  if (b.l) k.bx -= 100 * s;
  if (b.r) k.bx += 100 * s;
  k.bx = fmaxf(0, fminf(110, k.bx));
  k.spawn += dt;
  if (k.spawn > k.interval) {
    k.spawn = 0;
    k.interval = fmaxf(420, 1100 - G.c.score * 18);
    for (auto &i : k.it) {
      if (i.on) continue;
      int r = G.c.range(0, 99);
      i.kind = r < 70 ? 0 : (r < 95 ? 1 : 2);
      i.x = G.c.range(4, 122);
      i.y = 6;
      i.v = 26 + G.c.score * 0.8f + G.c.range(0, 14);
      i.on = true;
      break;
    }
  }
  for (auto &i : k.it) {
    if (!i.on) continue;
    i.y += i.v * s;
    if (i.y > 54 && i.y < 61 && i.x > k.bx - 3 && i.x < k.bx + 19) {
      i.on = false;
      if (i.kind == 0) { G.c.score++; gameSfx(1568, 30); }
      else if (i.kind == 2) { if (k.lives < 5) k.lives++; gameSfx(1046, 60); }
      else { k.lives--; k.hitAt = now; gameSfx(220, 140); if (k.lives <= 0) { gameOver(G, now); return; } }
    } else if (i.y > 66) {
      i.on = false;
    }
  }
}

static const int SIMON_TONE[3] = {523, 659, 784};

inline void stepSimon(Games &G, const GBtn &b, float dt, uint32_t now) {
  GSimon &s = G.si;
  if (s.phase == 0) {  // robot memperagakan urutan
    if ((int32_t)(now - s.t) < 0) return;
    int idx = s.pos / 2;
    if (idx >= s.len) { s.phase = 1; s.pos = 0; s.lit = -1; return; }
    if (s.pos % 2 == 0) {
      s.lit = s.seq[idx];
      gameSfx(SIMON_TONE[s.lit], 280);
      s.t = now + 380;
    } else {
      s.lit = -1;
      s.t = now + 160;
    }
    s.pos++;
    return;
  }
  if (s.phase == 2) {  // jeda setelah benar
    if ((int32_t)(now - s.t) >= 0) { s.phase = 0; s.pos = 0; s.t = now; }
    return;
  }
  if (s.lit >= 0 && (int32_t)(now - s.t) >= 0) s.lit = -1;
  int press = b.lP ? 0 : (b.okP ? 1 : (b.rP ? 2 : -1));
  if (press < 0) return;
  s.lit = press;
  s.t = now + 220;
  if (press != s.seq[s.pos]) { gameOver(G, now); return; }
  gameSfx(SIMON_TONE[press], 160);
  s.pos++;
  if (s.pos >= s.len) {
    G.c.score = s.len;
    if (s.len < 64) s.len++;
    s.phase = 2;
    s.t = now + 900;
  }
}

inline void stepReflex(Games &G, const GBtn &b, float dt, uint32_t now) {
  GReflex &r = G.re;
  float tx = 0, ty = 0;
  if (r.dir == 0) tx = -14;
  if (r.dir == 1) ty = -9;
  if (r.dir == 2) tx = 14;
  float k = fminf(1, dt / 45.0f);  // mata melirik cepat tapi halus
  r.lookX += (tx - r.lookX) * k;
  r.lookY += (ty - r.lookY) * k;
  int press = b.lP ? 0 : (b.okP ? 1 : (b.rP ? 2 : -1));
  if (r.dir < 0) {
    if (press >= 0) { gameOver(G, now); return; }  // terlalu cepat menekan
    if ((int32_t)(now - r.t) >= 0) {
      r.dir = G.c.range(0, 2);
      r.t = now;
      gameSfx(1000, 25);
    }
    return;
  }
  if (press >= 0) {
    if (press != r.dir) { gameOver(G, now); return; }
    G.c.score++;
    r.happyAt = now;
    gameSfx(1319, 40);
    r.dir = -1;
    r.window = (uint32_t)fmaxf(380, 1300 - G.c.score * 38);
    r.t = now + G.c.range(350, 1100);
    return;
  }
  if (now - r.t > r.window) gameOver(G, now);
}

// ---------------- Gambar per game ----------------
inline void drawFlappy(Games &G, Adafruit_GFX &g, uint32_t now) {
  GFlappy &f = G.fl;
  int gh = G.c.score > 20 ? 22 : (G.c.score > 8 ? 24 : 26);
  for (int i = 0; i < 3; i++) {
    int x = (int)f.px[i];
    int top = f.gap[i] - gh / 2, bot = f.gap[i] + gh / 2;
    g.fillRect(x, 8, 10, top - 8, C_ON);
    g.fillRect(x - 1, top - 3, 12, 3, C_ON);
    g.fillRect(x, bot, 10, 64 - bot, C_ON);
    g.fillRect(x - 1, bot, 12, 3, C_ON);
  }
  int y = (int)f.y;
  g.fillCircle(24, y, 3, C_ON);
  g.drawPixel(25, y - 1, C_OFF);
  g.drawLine(26, y, 28, y + (f.vy < 0 ? 1 : 0), C_ON);  // paruh
  int wing = (now / 80) % 2 ? -2 : 1;
  g.drawLine(20, y, 22, y + wing, C_OFF);
  gHud(g, G.c.score);
}

inline void drawDino(Games &G, Adafruit_GFX &g, uint32_t now) {
  GDino &d = G.di;
  g.drawFastHLine(0, 57, 128, C_ON);
  int off = (int)(d.dist) % 16;
  for (int x = -off; x < 128; x += 16) g.drawPixel(x + 5, 60, C_ON), g.drawPixel(x + 11, 62, C_ON);
  int y = 56 + (int)d.y;
  g.fillRect(12, y - 10, 7, 7, C_ON);   // badan
  g.fillRect(16, y - 14, 6, 5, C_ON);   // kepala
  g.drawPixel(20, y - 13, C_OFF);       // mata
  g.drawPixel(11, y - 8, C_ON);         // ekor
  bool run = d.y >= 0 && (now / 90) % 2;
  g.drawFastVLine(run ? 13 : 14, y - 3, 3, C_ON);
  g.drawFastVLine(run ? 17 : 16, y - 3, 3, C_ON);
  for (auto &o : d.ob) {
    int x = (int)o.x;
    if (x < -10 || x > 130) continue;
    if (o.kind == 0) {
      g.fillRect(x + o.w / 2 - 1, 56 - o.h, 3, o.h, C_ON);
      g.fillRect(x, 56 - o.h + 3, 2, 3, C_ON);
      g.fillRect(x + o.w - 2, 56 - o.h + 2, 2, 3, C_ON);
    } else {
      g.fillRect(x + 2, 41, 5, 3, C_ON);
      bool up = (now / 120) % 2;
      g.drawLine(x + 3, 41, x + (up ? 1 : 5), up ? 37 : 46, C_ON);
      g.drawPixel(x, 42, C_ON);
    }
  }
  gHud(g, G.c.score);
}

inline void drawSnake(Games &G, Adafruit_GFX &g, uint32_t now) {
  GSnake &s = G.sn;
  g.drawRect(0, 8, 128, 56, C_ON);
  for (int i = 0; i < s.len; i++) {
    int x = s.x[i] * 4, y = 8 + s.y[i] * 4;
    if (i == 0) g.fillRect(x, y, 4, 4, C_ON);
    else g.fillRect(x + (i % 2 ? 1 : 0), y + 1, 3, 2, C_ON), g.fillRect(x + 1, y, 2, 4, C_ON);
  }
  if ((now / 250) % 3) {  // makanan berkedip
    int fx = s.fx * 4 + 1, fy = 8 + s.fy * 4 + 1;
    g.fillRect(fx, fy, 2, 2, C_ON);
    g.drawPixel(fx + 1, fy - 1, C_ON);
  }
  gHud(g, G.c.score);
}

inline void drawBreak(Games &G, Adafruit_GFX &g, uint32_t now) {
  GBreak &k = G.br;
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 8; c++)
      if (k.bricks[r][c]) {
        if (r % 2) g.fillRect(1 + c * 16, 11 + r * 6, 15, 4, C_ON);
        else g.drawRect(1 + c * 16, 11 + r * 6, 15, 4, C_ON);
      }
  g.fillRoundRect((int)k.px, 60, 21, 3, 1, C_ON);
  g.fillCircle((int)k.bx, (int)k.by, 1, C_ON);
  gHud(g, G.c.score, k.lives);
  if (k.stuck && (now / 400) % 2) gPrintC(g, "Tekan OK", 44);
}

inline void drawPong(Games &G, Adafruit_GFX &g, uint32_t now) {
  GPong &p = G.po;
  for (int y = 9; y < 64; y += 4) g.drawPixel(64, y, C_ON);
  g.fillRect(2, (int)p.py - 7, 3, 14, C_ON);
  g.fillRect(123, (int)p.ay - 7, 3, 14, C_ON);
  g.fillRect((int)p.bx - 1, (int)p.by - 1, 3, 3, C_ON);
  char b[12];
  snprintf(b, sizeof(b), "%d  %d", p.sp, p.sa);
  gPrintC(g, b, 0);
  g.setCursor(0, 0); g.print("Kamu");
  g.setCursor(98, 0); g.print("Robot");
}

inline void drawCatch(Games &G, Adafruit_GFX &g, uint32_t now) {
  GCatch &k = G.ca;
  bool flash = k.hitAt && now - k.hitAt < 300 && (now / 60) % 2;
  int bx = (int)k.bx;
  g.drawLine(bx, 56, bx + 3, 62, C_ON);
  g.drawLine(bx + 16, 56, bx + 13, 62, C_ON);
  g.drawFastHLine(bx + 3, 62, 11, C_ON);
  if (!flash) g.drawFastHLine(bx, 56, 17, C_ON);
  for (auto &i : k.it) {
    if (!i.on) continue;
    int x = (int)i.x, y = (int)i.y;
    if (i.kind == 0) drawSparkle(g, x, y, 2 + ((now / 150) % 2));
    else if (i.kind == 2) drawHeart(g, x, y, 2);
    else { g.fillCircle(x, y, 3, C_ON); g.drawLine(x + 2, y - 3, x + 4, y - 5, C_ON); }
  }
  gHud(g, G.c.score, k.lives);
}

inline void drawSimon(Games &G, Adafruit_GFX &g, uint32_t now) {
  GSimon &s = G.si;
  static const char *lbl[3] = {"KIRI", "OK", "KANAN"};
  for (int i = 0; i < 3; i++) {
    int x = 4 + i * 42;
    if (s.lit == i) {
      g.fillRoundRect(x, 18, 36, 30, 6, C_ON);
      g.setTextColor(C_OFF);
    } else {
      g.drawRoundRect(x, 18, 36, 30, 6, C_ON);
      g.setTextColor(C_ON);
    }
    g.setTextSize(1);
    g.setCursor(x + 18 - (int)strlen(lbl[i]) * 3, 29);
    g.print(lbl[i]);
  }
  g.setTextColor(C_ON);
  gPrintC(g, s.phase == 1 ? "Giliranmu!" : (s.phase == 2 ? "Benar!" : "Perhatikan..."), 54);
  gHud(g, G.c.score);
}

inline void drawReflex(Games &G, Adafruit_GFX &g, uint32_t now) {
  GReflex &r = G.re;
  bool happy = r.happyAt && now - r.happyAt < 250;
  for (int i = 0; i < 2; i++) {
    int cx = i ? 86 : 42;
    int x = cx - 15 + (int)r.lookX, y = 18 + (int)r.lookY;
    g.fillRoundRect(x, y, 30, 28, 8, C_ON);
    if (happy) g.fillCircle(cx + (int)r.lookX, y + 34, 22, C_OFF);  // mata senang
  }
  if (r.dir >= 0) {  // batas waktu tersisa
    float left = 1 - (float)(now - r.t) / r.window;
    if (left > 0) g.fillRect(0, 62, (int)(128 * left), 2, C_ON);
  } else if (G.c.score == 0) {
    gPrintC(g, "Tunggu lirikanku...", 54);
  }
  gHud(g, G.c.score);
}

// ---------------- 9. Balok ----------------
static const int8_t TET[7][4][2] = {{{0, 1}, {1, 1}, {2, 1}, {3, 1}}, {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
                                    {{1, 0}, {0, 1}, {1, 1}, {2, 1}}, {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
                                    {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
                                    {{2, 0}, {0, 1}, {1, 1}, {2, 1}}};

inline void tetCell(int type, int rot, int i, int &x, int &y) {
  x = TET[type][i][0];
  y = TET[type][i][1];
  if (type == 1) return;  // kotak O tidak berputar
  int size = type == 0 ? 4 : 3;
  for (int r = 0; r < (rot & 3); r++) { int nx = size - 1 - y; y = x; x = nx; }
}

inline bool tetFits(GTetris &t, int px, int py, int rot) {
  for (int i = 0; i < 4; i++) {
    int x, y;
    tetCell(t.type, rot, i, x, y);
    int bx = px + x, by = py + y;
    if (bx < 0 || bx >= GTetris::W || by >= GTetris::H) return false;
    if (by >= 0 && t.board[by][bx]) return false;
  }
  return true;
}

inline void tetLock(Games &G, uint32_t now) {
  GTetris &t = G.te;
  for (int i = 0; i < 4; i++) {
    int x, y;
    tetCell(t.type, t.rot, i, x, y);
    if (t.py + y < 0) { gameOver(G, now); return; }
    t.board[t.py + y][t.px + x] = 1;
  }
  int cleared = 0;
  for (int y = GTetris::H - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < GTetris::W; x++) full &= t.board[y][x] != 0;
    if (!full) continue;
    cleared++;
    for (int yy = y; yy > 0; yy--) memcpy(t.board[yy], t.board[yy - 1], GTetris::W);
    memset(t.board[0], 0, GTetris::W);
    y++;  // periksa baris yang sama lagi (sudah turun)
  }
  static const int PTS[5] = {0, 10, 30, 50, 80};
  G.c.score += PTS[cleared];
  t.lines += cleared;
  if (cleared) { gameSfx(1046, 50); gameSfx(1568, 70); } else gameSfx(300, 15);
  t.type = t.next;
  t.next = G.c.range(0, 6);
  t.rot = 0; t.px = 3; t.py = 0;
  if (!tetFits(t, t.px, t.py, t.rot)) gameOver(G, now);
}

inline void stepTetris(Games &G, const GBtn &b, float dt, uint32_t now) {
  GTetris &t = G.te;
  if (b.l && b.r && (b.lP || b.rP)) {  // kiri + kanan bersamaan = jatuhkan langsung
    while (tetFits(t, t.px, t.py + 1, t.rot)) t.py++;
    tetLock(G, now);
    t.lHeld = t.rHeld = now + 100000;  // jangan ikut bergeser setelah jatuh
    return;
  }
  auto slide = [&](bool held, bool pressed, uint32_t &since, int dir) {
    if (pressed) { since = now; if (tetFits(t, t.px + dir, t.py, t.rot)) t.px += dir; return; }
    if (!held) { since = 0; return; }
    if (since && now > since + 170) {  // ditahan: bergeser berulang
      if (tetFits(t, t.px + dir, t.py, t.rot)) t.px += dir;
      since = now - 170 + 70;
    }
  };
  if (!(b.l && b.r)) {
    slide(b.l, b.lP, t.lHeld, -1);
    slide(b.r, b.rP, t.rHeld, 1);
  }
  if (b.okP) {
    static const int KICK[5] = {0, -1, 1, -2, 2};
    for (int k : KICK)
      if (tetFits(t, t.px + k, t.py, t.rot + 1)) { t.px += k; t.rot = (t.rot + 1) & 3; gameSfx(880, 12); break; }
  }
  float interval = fmaxf(90, 620 - t.lines * 22);
  t.fall += dt;
  if (t.fall >= interval) {
    t.fall = 0;
    if (tetFits(t, t.px, t.py + 1, t.rot)) t.py++;
    else tetLock(G, now);
  }
}

inline void drawTetris(Games &G, Adafruit_GFX &g, uint32_t now) {
  GTetris &t = G.te;
  const int X0 = 49, Y0 = 9;
  g.drawRect(X0 - 2, Y0 - 1, GTetris::W * 3 + 3, GTetris::H * 3 + 1, C_ON);
  for (int y = 0; y < GTetris::H; y++)
    for (int x = 0; x < GTetris::W; x++)
      if (t.board[y][x]) g.fillRect(X0 + x * 3, Y0 + y * 3, 2, 2, C_ON);
  if (G.c.st == GS_PLAY) {
    int gy = t.py;  // bayangan tempat jatuh
    while (tetFits(t, t.px, gy + 1, t.rot)) gy++;
    for (int i = 0; i < 4; i++) {
      int x, y;
      tetCell(t.type, t.rot, i, x, y);
      if (gy != t.py) g.drawPixel(X0 + (t.px + x) * 3, Y0 + (gy + y) * 3 + 1, C_ON);
      if (t.py + y >= 0) g.fillRect(X0 + (t.px + x) * 3, Y0 + (t.py + y) * 3, 3, 3, C_ON);
    }
  }
  g.setTextSize(1);
  g.setTextColor(C_ON);
  g.setCursor(90, 12); g.print("Next");
  for (int i = 0; i < 4; i++) {
    int x, y;
    tetCell(t.next, 0, i, x, y);
    g.fillRect(92 + x * 4, 24 + y * 4, 3, 3, C_ON);
  }
  g.setCursor(2, 12); g.print("Baris");
  char b[8];
  snprintf(b, sizeof(b), "%d", t.lines);
  g.setCursor(2, 24); g.print(b);
  gHud(g, G.c.score);
}

// ---------------- 10. Tembak Alien ----------------
inline void stepInvade(Games &G, const GBtn &b, float dt, uint32_t now) {
  GInvade &v = G.in;
  float s = dt / 1000.0f;
  if (b.l) v.sx -= 80 * s;
  if (b.r) v.sx += 80 * s;
  v.sx = fmaxf(0, fminf(120, v.sx));
  if (b.okP)
    for (auto &x : v.shot)
      if (!x.on) { x = {v.sx + 3, 52, true}; gameSfx(1500, 15); break; }
  // gerombolan alien
  float speed = 9 + (18 - v.left) * 1.6f + v.wave * 4;
  v.ax += v.adir * speed * s;
  int minC = 6, maxC = -1, maxR = -1;
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 6; c++)
      if (v.alive[r][c]) { minC = c < minC ? c : minC; maxC = c > maxC ? c : maxC; maxR = r > maxR ? r : maxR; }
  if (maxC < 0) {  // gelombang berikutnya
    v.wave++; v.left = 18; v.ax = 20; v.ay = 12; v.adir = 1;
    for (auto &r : v.alive) for (auto &a : r) a = true;
    gameSfx(1319, 120);
    return;
  }
  if (v.ax + minC * 14 < 2 && v.adir < 0) { v.adir = 1; v.ay += 4; }
  if (v.ax + maxC * 14 + 8 > 126 && v.adir > 0) { v.adir = -1; v.ay += 4; }
  if (v.ay + maxR * 9 + 6 >= 52) { gameOver(G, now); return; }
  for (auto &x : v.shot) {
    if (!x.on) continue;
    x.y -= 120 * s;
    if (x.y < 8) { x.on = false; continue; }
    int c = (int)((x.x - v.ax) / 14), r = (int)((x.y - v.ay) / 9);
    if (c >= 0 && c < 6 && r >= 0 && r < 3 && v.alive[r][c]) {
      float lx = x.x - (v.ax + c * 14), ly = x.y - (v.ay + r * 9);
      if (lx >= 0 && lx <= 8 && ly >= 0 && ly <= 6) {
        v.alive[r][c] = false; v.left--; x.on = false;
        G.c.score += 10 * (3 - r);
        gameSfx(400 + r * 200, 25);
      }
    }
  }
  v.bombAcc += dt;
  if (v.bombAcc > fmaxf(380, 1300 - v.wave * 150)) {  // alien menjatuhkan bom
    v.bombAcc = 0;
    for (int tries = 0; tries < 6; tries++) {
      int c = G.c.range(0, 5);
      for (int r = 2; r >= 0; r--)
        if (v.alive[r][c]) {
          for (auto &x : v.bomb)
            if (!x.on) { x = {v.ax + c * 14 + 4, v.ay + r * 9 + 6, true}; break; }
          tries = 99;
          break;
        }
    }
  }
  for (auto &x : v.bomb) {
    if (!x.on) continue;
    x.y += (40 + v.wave * 5) * s;
    if (x.y > 64) { x.on = false; continue; }
    if (x.y > 55 && x.x >= v.sx - 1 && x.x <= v.sx + 8) {
      x.on = false; v.lives--; v.hitAt = now;
      gameSfx(200, 150);
      if (v.lives <= 0) { gameOver(G, now); return; }
    }
  }
}

inline void drawInvade(Games &G, Adafruit_GFX &g, uint32_t now) {
  GInvade &v = G.in;
  bool frame = (now / 400) % 2;
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 6; c++) {
      if (!v.alive[r][c]) continue;
      int x = (int)v.ax + c * 14, y = (int)v.ay + r * 9;
      g.fillRect(x + 1, y, 6, 4, C_ON);
      g.drawPixel(x + 2, y + 1, C_OFF); g.drawPixel(x + 5, y + 1, C_OFF);
      g.drawPixel(x + (frame ? 0 : 1), y + 4, C_ON); g.drawPixel(x + (frame ? 7 : 6), y + 4, C_ON);
      g.drawPixel(x + (frame ? 1 : 0), y + 5, C_ON); g.drawPixel(x + (frame ? 6 : 7), y + 5, C_ON);
    }
  bool blink = v.hitAt && now - v.hitAt < 600 && (now / 80) % 2;
  if (!blink) {
    int x = (int)v.sx;
    g.fillRect(x + 3, 54, 1, 2, C_ON);
    g.fillRect(x + 1, 56, 5, 2, C_ON);
    g.fillRect(x, 58, 7, 3, C_ON);
  }
  for (auto &x : v.shot) if (x.on) g.drawFastVLine((int)x.x, (int)x.y, 3, C_ON);
  for (auto &x : v.bomb) if (x.on) { g.drawPixel((int)x.x, (int)x.y, C_ON); g.drawPixel((int)x.x + 1, (int)x.y + 1, C_ON); g.drawPixel((int)x.x, (int)x.y + 2, C_ON); }
  gHud(g, G.c.score, v.lives);
}

// ---------------- 11. Teriak Terbang (mic) ----------------
inline void stepVoice(Games &G, const GBtn &b, float dt, uint32_t now) {
  GVoice &v = G.vo;
  float s = dt / 1000.0f;
  float lvl = gameMicLevel;
  v.vy += (240 - lvl * 620) * s;  // suara keras = dorongan ke atas
  v.vy = fmaxf(-70, fminf(70, v.vy));
  v.y += v.vy * s;
  if (v.y < 11) { v.y = 11; v.vy = 0; }
  v.speed = fminf(30 + G.c.score * 1.2f, 55);
  for (int i = 0; i < 3; i++) {
    v.px[i] -= v.speed * s;
    if (v.px[i] < -12) {
      float far = fmaxf(fmaxf(v.px[0], v.px[1]), v.px[2]);
      v.px[i] = far + 58;
      v.gap[i] = G.c.range(24, 46);
      v.passed[i] = false;
    }
    bool inX = v.px[i] < 27 && v.px[i] + 9 > 21;
    if (inX && (v.y - 3 < v.gap[i] - 15 || v.y + 3 > v.gap[i] + 15)) { gameOver(G, now); return; }
    if (!v.passed[i] && v.px[i] + 9 < 21) { v.passed[i] = true; G.c.score++; }
  }
  if (v.y > 61) gameOver(G, now);
}

inline void drawVoice(Games &G, Adafruit_GFX &g, uint32_t now) {
  GVoice &v = G.vo;
  for (int i = 0; i < 3; i++) {
    int x = (int)v.px[i];
    g.fillRect(x, 8, 9, v.gap[i] - 15 - 8, C_ON);
    g.fillRect(x, v.gap[i] + 15, 9, 64 - v.gap[i] - 15, C_ON);
  }
  int y = (int)v.y;
  g.fillCircle(24, y, 3, C_ON);
  g.drawPixel(25, y - 1, C_OFF);
  float lvl = gameMicLevel;
  if (lvl > 0.15f) {  // gelombang suara dari mulut burung
    g.drawCircle(18, y, 5 + (int)(lvl * 4), C_ON);
  }
  g.drawRect(0, 9, 4, 54, C_ON);  // meteran suara
  int h = (int)(lvl * 52);
  g.fillRect(1, 62 - h, 2, h, C_ON);
  gHud(g, G.c.score);
}

// ---------------- 12. Irama ----------------
static const int RH_X[3] = {26, 64, 102};
static const int RH_TONE[3] = {523, 659, 784};

inline void stepRhythm(Games &G, const GBtn &b, float dt, uint32_t now) {
  GRhythm &r = G.rh;
  float s = dt / 1000.0f;
  float fall = fminf(36 + G.c.score * 0.35f, 80);
  r.interval = fmaxf(300, 750 - G.c.score * 5);
  r.acc += dt;
  if (r.acc >= r.interval) {
    r.acc = 0;
    int lane = G.c.range(0, 2);
    if (lane == r.lastLane && G.c.range(0, 1)) lane = (lane + G.c.range(1, 2)) % 3;
    r.lastLane = lane;
    for (auto &n : r.n) if (!n.on) { n = {11, lane, true}; break; }
  }
  const bool press[3] = {b.lP, b.okP, b.rP};
  for (int lane = 0; lane < 3; lane++) {
    if (!press[lane]) continue;
    r.flashLane = lane; r.flashAt = now;
    GRhythm::Note *best = nullptr;
    for (auto &n : r.n)
      if (n.on && n.lane == lane && fabsf(n.y - 54) <= 7 && (!best || n.y > best->y)) best = &n;
    if (best) {
      best->on = false;
      r.combo++;
      G.c.score += 1 + (r.combo >= 30 ? 3 : r.combo / 10);  // bonus kombo maksimal +3
      gameSfx(RH_TONE[lane] * (r.combo % 8 == 0 ? 2 : 1), 70);
    } else {
      r.combo = 0;  // salah tekan: kombo putus
    }
  }
  for (auto &n : r.n) {
    if (!n.on) continue;
    n.y += fall * s;
    if (n.y > 62) {
      n.on = false; r.lives--; r.combo = 0; r.missAt = now;
      gameSfx(196, 90);
      if (r.lives <= 0) { gameOver(G, now); return; }
    }
  }
}

inline void drawRhythm(Games &G, Adafruit_GFX &g, uint32_t now) {
  GRhythm &r = G.rh;
  for (int y = 10; y < 64; y += 3) { g.drawPixel(45, y, C_ON); g.drawPixel(83, y, C_ON); }
  for (int lane = 0; lane < 3; lane++) {
    int x = RH_X[lane];
    bool fl = r.flashLane == lane && now - r.flashAt < 120;
    if (fl) g.fillRoundRect(x - 12, 51, 24, 7, 2, C_ON);
    else g.drawRoundRect(x - 12, 51, 24, 7, 2, C_ON);
  }
  for (auto &n : r.n) if (n.on) g.fillRoundRect(RH_X[n.lane] - 9, (int)n.y - 2, 18, 4, 1, C_ON);
  if (r.combo >= 5) {
    char b[12];
    snprintf(b, sizeof(b), "x%d", r.combo);
    gPrintC(g, b, 0);
  }
  gHud(g, G.c.score, r.lives);
}

// ---------------- 13. Balapan ----------------
static const int RACE_X[3] = {38, 64, 90};

inline void stepRace(Games &G, const GBtn &b, float dt, uint32_t now) {
  GRace &r = G.ra;
  float s = dt / 1000.0f;
  if (b.lP && r.lane > 0) { r.lane--; gameSfx(700, 15); }
  if (b.rP && r.lane < 2) { r.lane++; gameSfx(700, 15); }
  float target = RACE_X[r.lane];
  r.carX += (target - r.carX) * fminf(1, dt / 55.0f);  // pindah jalur halus
  r.speed = fminf(50 + r.dist / 45, 125);
  float d = r.speed * s;
  r.dist += d;
  G.c.score = (int)(r.dist / 20);
  r.sinceSpawn += d;
  if (r.sinceSpawn >= r.nextGap) {
    r.sinceSpawn = 0;
    r.nextGap = G.c.range(30, 56);
    for (auto &e : r.e) if (!e.on) { e = {G.c.range(0, 2), -12, true}; break; }
  }
  for (auto &e : r.e) {
    if (!e.on) continue;
    e.y += d * 0.6f;  // mobil lain lebih lambat dari kita
    if (e.y > 66) { e.on = false; continue; }
    if (fabsf(r.carX - RACE_X[e.lane]) < 9 && e.y + 11 > 49 && e.y < 61) { gameOver(G, now); return; }
  }
}

inline void drawCar(Adafruit_GFX &g, int x, int y) {
  g.fillRoundRect(x - 5, y, 11, 12, 2, C_ON);
  g.fillRect(x - 3, y + 2, 7, 3, C_OFF);
  g.drawFastVLine(x - 6, y + 2, 3, C_ON); g.drawFastVLine(x + 6, y + 2, 3, C_ON);
  g.drawFastVLine(x - 6, y + 8, 3, C_ON); g.drawFastVLine(x + 6, y + 8, 3, C_ON);
}

inline void drawRace(Games &G, Adafruit_GFX &g, uint32_t now) {
  GRace &r = G.ra;
  g.drawFastVLine(24, 8, 56, C_ON);
  g.drawFastVLine(104, 8, 56, C_ON);
  int off = (int)r.dist % 10;
  for (int y = -10 + off; y < 64; y += 10) {
    if (y + 5 < 9) continue;
    g.drawFastVLine(51, y < 9 ? 9 : y, 5, C_ON);
    g.drawFastVLine(77, y < 9 ? 9 : y, 5, C_ON);
  }
  for (auto &e : r.e) if (e.on && e.y > -12) drawCar(g, RACE_X[e.lane], (int)e.y);
  drawCar(g, (int)r.carX, 49);
  gHud(g, G.c.score);
}

// ---------------- 14. Lompat Awan ----------------
inline void stepJump(Games &G, const GBtn &b, float dt, uint32_t now) {
  GJump &j = G.ju;
  float s = dt / 1000.0f;
  if (b.l) j.x -= 75 * s;
  if (b.r) j.x += 75 * s;
  if (j.x < -6) j.x = 128;  // tembus sisi layar
  if (j.x > 128) j.x = -6;
  float prevY = j.y;
  j.vy += 300 * s;
  j.y += j.vy * s;
  if (j.vy > 0)  // mendarat di awan
    for (auto &p : j.p)
      if (p.on && prevY + 3 <= p.y && j.y + 3 >= p.y && j.x + 3 > p.x && j.x - 3 < p.x + p.w) {
        j.vy = -155; j.y = p.y - 3;
        gameSfx(880 + (G.c.score % 5) * 80, 20);
        break;
      }
  if (j.y < 26) {  // kamera naik
    float sh = 26 - j.y;
    j.y = 26;
    j.height += sh;
    G.c.score = (int)(j.height / 10);
    for (auto &p : j.p) p.y += sh;
  }
  float top = 64;
  for (auto &p : j.p) top = fminf(top, p.y);
  for (auto &p : j.p)
    if (p.y > 66) {  // awan lama di bawah pindah ke atas
      p.y = top - G.c.range(9, 15);
      p.w = (int)fmaxf(10, 18 - G.c.score / 15);
      p.x = G.c.range(2, 126 - p.w);
      top = p.y;
    }
  if (j.y > 66) gameOver(G, now);
}

inline void drawJump(Games &G, Adafruit_GFX &g, uint32_t now) {
  GJump &j = G.ju;
  for (auto &p : j.p) {
    if (!p.on || p.y < 8) continue;
    int x = (int)p.x, y = (int)p.y;
    g.fillRoundRect(x, y, p.w, 3, 1, C_ON);
    g.fillCircle(x + p.w / 3, y, 2, C_ON);
    g.fillCircle(x + p.w * 2 / 3, y - 1, 2, C_ON);
  }
  int x = (int)j.x, y = (int)j.y;
  g.fillRoundRect(x - 3, y - 4, 7, 7, 2, C_ON);
  g.drawPixel(x - 1, y - 2, C_OFF); g.drawPixel(x + 1, y - 2, C_OFF);
  gHud(g, G.c.score);
}

// ======================================================================
// ================ Game 15-22 ================
// ======================================================================

// ---------------- 15. Helikopter ----------------
inline void copterNewCol(Games &G, int i) {
  GCopter &k = G.co;
  k.center += G.c.range(-3, 3);
  k.gap = fmaxf(20, 42 - k.dist / 90.0f);
  float lo = 9 + k.gap / 2 + 1, hi = 63 - k.gap / 2;
  k.center = fmaxf(lo, fminf(hi, k.center));
  k.top[i] = (int8_t)(k.center - k.gap / 2);
  k.bot[i] = (int8_t)(k.center + k.gap / 2);
  k.obs[i] = -1;
  if (k.dist > 120 && G.c.range(0, 17) == 0 && k.gap >= 24) {  // batu di tengah gua
    int room = k.bot[i] - k.top[i] - 6;
    k.obs[i] = (int8_t)(k.top[i] + (G.c.range(0, 1) ? 2 : room - 2));
  }
}

inline void stepCopter(Games &G, const GBtn &b, float dt, uint32_t now) {
  GCopter &k = G.co;
  float s = dt / 1000.0f;
  bool hold = b.ok || b.l || b.r;
  k.vy += (hold ? -300 : 300) * s;
  k.vy = fmaxf(-75, fminf(75, k.vy));
  k.y += k.vy * s;
  float speed = fminf(40 + k.dist / 30.0f, 75);
  k.off += speed * s;
  while (k.off >= 4) {
    k.off -= 4;
    memmove(k.top, k.top + 1, GCopter::N - 1);
    memmove(k.bot, k.bot + 1, GCopter::N - 1);
    memmove(k.obs, k.obs + 1, GCopter::N - 1);
    k.dist += 4;
    copterNewCol(G, GCopter::N - 1);
    int sc = k.dist / 10;
    if (sc / 50 > G.c.score / 50) gameSfx(1319, 40);
    G.c.score = sc;
  }
  for (int x = 16; x <= 26; x += 5) {
    int c = (int)((x + k.off) / 4);
    if (c < 0 || c >= GCopter::N) continue;
    if (k.y - 3 < k.top[c] || k.y + 3 > k.bot[c]) { gameOver(G, now); return; }
    if (k.obs[c] >= 0 && k.y + 3 > k.obs[c] && k.y - 3 < k.obs[c] + 6) { gameOver(G, now); return; }
  }
}

inline void drawCopter(Games &G, Adafruit_GFX &g, uint32_t now) {
  GCopter &k = G.co;
  for (int i = 0; i < GCopter::N; i++) {
    int x = i * 4 - (int)k.off;
    if (k.top[i] > 8) g.fillRect(x, 8, 4, k.top[i] - 8, C_ON);
    g.fillRect(x, k.bot[i], 4, 64 - k.bot[i], C_ON);
    if (k.obs[i] >= 0) g.fillRect(x, k.obs[i], 4, 6, C_ON);
  }
  int y = (int)k.y;
  g.fillRoundRect(16, y - 2, 10, 5, 2, C_ON);  // badan
  g.drawFastHLine(10, y - 1, 6, C_ON);         // ekor
  g.drawFastVLine(10, y - 3, 3, C_ON);
  int r = (now / 50) % 2 ? 7 : 4;              // baling-baling
  g.drawFastHLine(21 - r, y - 4, r * 2, C_ON);
  g.drawFastHLine(17, y + 4, 8, C_ON);         // kaki
  g.drawPixel(23, y - 1, C_OFF);               // jendela
  g.fillRect(100, 0, 28, 8, C_OFF);
  gHud(g, G.c.score);
}

// ---------------- 16. Tumpuk Balok ----------------
inline void stepStack(Games &G, const GBtn &b, float dt, uint32_t now) {
  GStack &k = G.st;
  float s = dt / 1000.0f;
  int w = k.lw[k.n - 1];
  k.x += k.dir * k.speed * s;
  if (k.x < 0) { k.x = 0; k.dir = 1; }
  if (k.x + w > 128) { k.x = 128 - w; k.dir = -1; }
  if (!b.okP) return;
  int px = k.lx[k.n - 1], pw = k.lw[k.n - 1];
  int x = (int)roundf(k.x);
  int left = x > px ? x : px, right = (x + w) < (px + pw) ? (x + w) : (px + pw);
  if (abs(x - px) <= 2) {  // pas: tidak terpotong
    left = px; right = px + pw;
    k.perfectAt = now;
    gameSfx(1568, 60); gameSfx(2093, 60);
  } else if (right - left > 0) {
    gameSfx(880 + G.c.score * 20, 30);
  }
  if (right - left <= 0) { gameOver(G, now); return; }
  if (k.n >= 40) {  // geser riwayat supaya array tidak penuh
    memmove(k.lx, k.lx + 20, 20 * sizeof(k.lx[0]));
    memmove(k.lw, k.lw + 20, 20 * sizeof(k.lw[0]));
    k.n -= 20;
  }
  k.lx[k.n] = left;
  k.lw[k.n] = right - left;
  k.n++;
  G.c.score++;
  k.speed = fminf(45 + G.c.score * 3.0f, 115);
  k.dir = (G.c.score % 2) ? -1 : 1;
  k.x = k.dir > 0 ? 0 : 128 - (right - left);
}

inline void drawStack(Games &G, Adafruit_GFX &g, uint32_t now) {
  GStack &k = G.st;
  int s0 = k.n > 9 ? k.n - 9 : 0;
  for (int i = s0; i < k.n; i++) {
    int y = 63 - 5 * (i - s0 + 1) + 1;
    g.fillRect(k.lx[i], y, k.lw[i], 4, C_ON);
  }
  if (G.c.st == GS_PLAY) {
    int y = 63 - 5 * (k.n - s0 + 1) + 1;
    g.fillRect((int)k.x, y, k.lw[k.n - 1], 4, C_ON);
    g.drawFastHLine((int)k.x, y - 2, k.lw[k.n - 1], C_ON);  // bayangan atas: penanda balok aktif
  }
  if (k.perfectAt && now - k.perfectAt < 500) gPrintC(g, "PAS!", 10);
  gHud(g, G.c.score);
}

// ---------------- 17. Pukul Tikus ----------------
static const int MOLE_X[3] = {24, 64, 104};

inline void stepMole(Games &G, const GBtn &b, float dt, uint32_t now) {
  GMole &k = G.mo;
  if (k.hole >= 0 && (int32_t)(now - k.until) >= 0) {  // tikus kabur
    if (k.kind == 0) {
      k.lives--;
      gameSfx(220, 120);
      if (k.lives <= 0) { gameOver(G, now); return; }
    }
    k.hole = -1;
    k.next = now + G.c.range(250, 650);
  }
  if (k.hole < 0 && (int32_t)(now - k.next) >= 0) {
    k.hole = G.c.range(0, 2);
    k.kind = (G.c.score > 5 && G.c.range(0, 4) == 0) ? 1 : 0;  // kadang muncul bom
    uint32_t window = (uint32_t)fmaxf(480, 1300 - G.c.score * 25);
    k.upAt = now;
    k.until = now + window;
  }
  int press = b.lP ? 0 : (b.okP ? 1 : (b.rP ? 2 : -1));
  if (press < 0) return;
  k.fxAt = now;
  k.fxHole = press;
  if (k.hole == press && k.kind == 0) {
    k.fxHit = true;
    G.c.score++;
    gameSfx(1319, 40);
  } else {  // meleset atau memukul bom
    k.fxHit = false;
    k.lives--;
    gameSfx(k.hole == press ? 150 : 300, 140);
    if (k.lives <= 0) { k.hole = -1; gameOver(G, now); return; }
  }
  if (k.hole == press) {
    k.hole = -1;
    k.next = now + G.c.range(200, 550);
  }
}

inline void drawMole(Games &G, Adafruit_GFX &g, uint32_t now) {
  GMole &k = G.mo;
  for (int i = 0; i < 3; i++) {
    int x = MOLE_X[i];
    if (k.hole == i) {
      int h = (int)fminf(18, (now - k.upAt) * 18 / 120.0f);
      int top = 52 - h;
      if (k.kind == 0) {  // tikus
        g.fillRoundRect(x - 8, top, 16, h + 4, 6, C_ON);
        if (h > 10) {
          g.fillRect(x - 4, top + 4, 2, 2, C_OFF); g.fillRect(x + 2, top + 4, 2, 2, C_OFF);
          g.drawPixel(x, top + 8, C_OFF);
          g.fillCircle(x - 7, top + 2, 2, C_ON); g.fillCircle(x + 7, top + 2, 2, C_ON);  // telinga
        }
      } else {  // bom
        g.fillCircle(x, top + 7, 7, C_ON);
        g.drawLine(x + 4, top + 1, x + 7, top - 3, C_ON);
        if ((now / 100) % 2) drawSparkle(g, x + 8, top - 4, 2);
      }
    }
    g.fillRect(x - 15, 53, 30, 11, C_OFF);  // lubang menutup bagian bawah
    g.drawRoundRect(x - 14, 50, 28, 7, 3, C_ON);
    g.drawFastHLine(x - 12, 60, 24, C_ON);
  }
  if (k.fxAt && now - k.fxAt < 180) {  // palu
    int x = MOLE_X[k.fxHole];
    g.fillRect(x - 6, 20, 12, 7, C_ON);
    g.drawFastVLine(x, 12, 8, C_ON);
    if (k.fxHit) drawSparkle(g, x, 30, 4);
  }
  gHud(g, G.c.score, k.lives);
}

// ---------------- 18. Besar Kecil ----------------
inline const char *cardName(int v) {
  static const char *N[14] = {"", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
  return N[v < 1 ? 1 : (v > 13 ? 13 : v)];
}

inline void drawCard(Adafruit_GFX &g, int x, int y, const char *label, bool faceDown) {
  g.fillRoundRect(x, y, 30, 40, 4, C_OFF);
  g.drawRoundRect(x, y, 30, 40, 4, C_ON);
  if (faceDown) {
    for (int i = 4; i < 36; i += 4) g.drawLine(x + 3, y + i, x + 26, y + i, C_ON);
    return;
  }
  g.setTextSize(strlen(label) > 1 ? 2 : 3);
  g.setTextColor(C_ON);
  int w = (int)strlen(label) * (strlen(label) > 1 ? 12 : 18);
  g.setCursor(x + 15 - w / 2 + 1, y + (strlen(label) > 1 ? 13 : 10));
  g.print(label);
  g.setTextSize(1);
}

inline void stepHiLo(Games &G, const GBtn &b, float dt, uint32_t now) {
  GHiLo &k = G.hl;
  if (k.phase == 0) {
    if (!b.lP && !b.rP) return;
    k.nxt = G.c.range(1, 13);
    k.ok = k.nxt == k.cur || (b.rP ? k.nxt > k.cur : k.nxt < k.cur);  // sama = selamat
    k.phase = 1;
    k.t = now;
    gameSfx(660, 30);
    return;
  }
  if (now - k.t < 900) return;
  if (!k.ok) { gameOver(G, now); return; }
  G.c.score++;
  gameSfx(1319, 50);
  k.cur = k.nxt;
  k.phase = 0;
}

inline void drawHiLo(Games &G, Adafruit_GFX &g, uint32_t now) {
  GHiLo &k = G.hl;
  drawCard(g, 22, 11, cardName(k.cur), false);
  bool reveal = k.phase == 1 && now - k.t > 250;
  drawCard(g, 76, 11, cardName(k.nxt), !reveal);
  g.setTextSize(1);
  g.setTextColor(C_ON);
  if (k.phase == 0) {
    g.setCursor(0, 56); g.print("<KECIL");
    g.setCursor(92, 56); g.print("BESAR>");
    int a = (now / 300) % 2;
    g.fillTriangle(60 + a, 31, 66 + a, 27, 66 + a, 35, C_ON);
  } else if (reveal) {
    gPrintC(g, k.ok ? "BENAR!" : "SALAH", 55);
  }
  gHud(g, G.c.score);
}

// ---------------- 19. Hitung Kilat ----------------
inline void mathNew(Games &G, uint32_t now) {
  GMath &k = G.ma;
  int lvl = G.c.score;
  k.op = G.c.range(0, lvl < 5 ? 1 : 3);  // + -  lalu  x :
  int hi = 10 + lvl * 2;
  if (k.op == 0) { k.a = G.c.range(1, hi); k.b = G.c.range(1, hi); k.res = k.a + k.b; }
  else if (k.op == 1) { k.a = G.c.range(2, hi); k.b = G.c.range(1, k.a); k.res = k.a - k.b; }
  else if (k.op == 2) { k.a = G.c.range(2, 9 + lvl / 4); k.b = G.c.range(2, 9); k.res = k.a * k.b; }
  else { k.b = G.c.range(2, 9); k.res = G.c.range(2, 9 + lvl / 4); k.a = k.b * k.res; }
  k.truth = G.c.range(0, 1);
  if (k.truth) {
    k.shown = k.res;
  } else {
    int d = G.c.range(1, k.res > 20 ? 3 : 2) * (k.op >= 2 && G.c.range(0, 1) ? 10 : 1);
    k.shown = k.res + (G.c.range(0, 1) || k.res - d < 0 ? d : -d);
  }
  k.window = (uint32_t)fmaxf(1700, 4200 - lvl * 90);
  k.t = now;
}

inline void stepMath(Games &G, const GBtn &b, float dt, uint32_t now) {
  GMath &k = G.ma;
  int ans = b.lP ? 0 : (b.rP ? 1 : -1);
  bool timeout = now - k.t > k.window;
  if (ans < 0 && !timeout) return;
  bool ok = !timeout && (ans == 1) == k.truth;
  k.fbAt = now;
  k.fbOk = ok;
  if (ok) {
    G.c.score++;
    gameSfx(1319, 40);
  } else {
    k.lives--;
    gameSfx(220, 140);
    if (k.lives <= 0) { gameOver(G, now); return; }
  }
  mathNew(G, now);
}

inline void drawMath(Games &G, Adafruit_GFX &g, uint32_t now) {
  GMath &k = G.ma;
  static const char OPS[4] = {'+', '-', 'x', ':'};
  char eq[24];
  snprintf(eq, sizeof(eq), "%d%c%d=%d", k.a, OPS[k.op], k.b, k.shown);
  int big = (int)strlen(eq) * 12 <= 124;
  gPrintC(g, eq, big ? 20 : 24, big ? 2 : 1);
  g.setTextSize(1);
  float left = 1 - (float)(now - k.t) / k.window;
  if (left > 0) g.fillRect(0, 44, (int)(128 * left), 3, C_ON);
  g.setCursor(0, 56); g.print("<SALAH");
  g.setCursor(86, 56); g.print("BENAR>");
  if (k.fbAt && now - k.fbAt < 400) {  // centang / silang
    if (k.fbOk) { g.drawLine(58, 6, 62, 10, C_ON); g.drawLine(62, 10, 70, 2, C_ON); }
    else { g.drawLine(59, 2, 67, 10, C_ON); g.drawLine(67, 2, 59, 10, C_ON); }
  }
  gHud(g, G.c.score, k.lives);
}

// ---------------- 20. Tiga Sebaris ----------------
static const int8_t TTT_LINES[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
                                       {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};

inline int tttWinner(const uint8_t *b, int8_t *line) {  // 1 kamu, 2 robot, 3 seri, 0 lanjut
  for (auto &l : TTT_LINES)
    if (b[l[0]] && b[l[0]] == b[l[1]] && b[l[1]] == b[l[2]]) {
      if (line) memcpy(line, l, 3);
      return b[l[0]];
    }
  for (int i = 0; i < 9; i++) if (!b[i]) return 0;
  return 3;
}

inline int tttFind(uint8_t *b, uint8_t who) {  // langkah yang langsung membuat `who` menang
  for (int i = 0; i < 9; i++) {
    if (b[i]) continue;
    b[i] = who;
    bool win = tttWinner(b, nullptr) == who;
    b[i] = 0;
    if (win) return i;
  }
  return -1;
}

inline int tttNextEmpty(const uint8_t *b, int from, int dir) {
  for (int k = 1; k <= 9; k++) {
    int i = ((from + dir * k) % 9 + 9) % 9;
    if (!b[i]) return i;
  }
  return from;
}

inline void tttRobot(Games &G) {
  GTTT &k = G.tt;
  float mistake = fmaxf(0.08f, 0.35f - G.c.score * 0.05f);  // makin sering menang, robot makin pintar
  int m = tttFind(k.b, 2);
  if (m < 0) m = tttFind(k.b, 1);
  if (m >= 0 && G.c.frand() < mistake * 0.6f) m = -1;
  if (m < 0 && !k.b[4] && G.c.frand() > mistake) m = 4;
  if (m < 0) {
    static const int CORNERS[4] = {0, 2, 6, 8};
    int start = G.c.range(0, 3);
    for (int j = 0; j < 4 && m < 0; j++)
      if (!k.b[CORNERS[(start + j) % 4]] && G.c.frand() > mistake) m = CORNERS[(start + j) % 4];
  }
  if (m < 0) m = tttNextEmpty(k.b, G.c.range(0, 8), 1);
  k.b[m] = 2;
  gameSfx(587, 40);
}

inline void tttNewRound(Games &G, uint32_t now) {
  GTTT &k = G.tt;
  memset(k.b, 0, sizeof(k.b));
  k.cur = 4;
  k.phase = 0;
  k.winner = 0;
  k.t = now;
}

inline void stepTTT(Games &G, const GBtn &b, float dt, uint32_t now) {
  GTTT &k = G.tt;
  if (k.phase == 2) {  // tampilkan hasil sebentar
    if (now - k.t < 1500) return;
    if (k.winner == 2) { gameOver(G, now); return; }
    tttNewRound(G, now);
    return;
  }
  if (k.phase == 1) {
    if (now - k.t < 450) return;
    tttRobot(G);
  } else {
    if (b.lP) { k.cur = tttNextEmpty(k.b, k.cur, -1); gameSfx(700, 12); }
    if (b.rP) { k.cur = tttNextEmpty(k.b, k.cur, 1); gameSfx(700, 12); }
    if (!b.okP || k.b[k.cur]) return;
    k.b[k.cur] = 1;
    gameSfx(880, 40);
  }
  int w = tttWinner(k.b, k.line);
  if (w) {
    k.winner = w;
    k.phase = 2;
    k.t = now;
    if (w == 1) { G.c.score++; gameSfx(1319, 80); gameSfx(1568, 120); }
    return;
  }
  k.phase = k.phase == 0 ? 1 : 0;
  k.t = now;
  if (k.phase == 0 && k.b[k.cur]) k.cur = tttNextEmpty(k.b, k.cur, 1);
}

inline void drawTTT(Games &G, Adafruit_GFX &g, uint32_t now) {
  GTTT &k = G.tt;
  const int X0 = 40, Y0 = 12, C = 16;
  g.drawFastVLine(X0 + C, Y0, C * 3, C_ON); g.drawFastVLine(X0 + 2 * C, Y0, C * 3, C_ON);
  g.drawFastHLine(X0, Y0 + C, C * 3, C_ON); g.drawFastHLine(X0, Y0 + 2 * C, C * 3, C_ON);
  for (int i = 0; i < 9; i++) {
    int cx = X0 + (i % 3) * C + C / 2, cy = Y0 + (i / 3) * C + C / 2;
    if (k.b[i] == 1) { g.drawLine(cx - 5, cy - 5, cx + 5, cy + 5, C_ON); g.drawLine(cx + 5, cy - 5, cx - 5, cy + 5, C_ON); }
    if (k.b[i] == 2) g.drawCircle(cx, cy, 5, C_ON);
    if (k.phase == 0 && i == k.cur && (now / 250) % 2) g.drawRect(cx - 7, cy - 7, 15, 15, C_ON);
  }
  g.setTextSize(1);
  g.setTextColor(C_ON);
  g.setCursor(2, 24); g.print("Kamu");
  g.setCursor(10, 36); g.print("X");
  g.setCursor(94, 24); g.print("Robot");
  g.setCursor(106, 36); g.print("O");
  if (k.phase == 1) g.fillCircle(110, 50, 1 + (now / 150) % 3, C_ON);  // robot berpikir
  if (k.phase == 2) {
    if (k.winner != 3) {
      int a = k.line[0], c = k.line[2];
      g.drawLine(X0 + (a % 3) * C + C / 2, Y0 + (a / 3) * C + C / 2, X0 + (c % 3) * C + C / 2, Y0 + (c / 3) * C + C / 2, C_ON);
    }
    g.fillRect(0, 0, 100, 9, C_OFF);
    g.setCursor(0, 0);
    g.print(k.winner == 1 ? "KAMU MENANG!" : (k.winner == 2 ? "ROBOT MENANG" : "SERI"));
  }
  gHud(g, G.c.score);
}

// ---------------- 21. Seberang Jalan ----------------
inline int crossRowY(int row) { return 10 + row * 9; }

inline void crossLevel(Games &G) {
  GCross &k = G.cr;
  float boost = 1 + G.c.score * 0.12f;
  for (int i = 0; i < 4; i++) {
    k.sp[i] = ((i % 2) ? -1 : 1) * (22 + G.c.range(0, 14)) * boost;
    k.cw[i] = G.c.range(10, 18);
    k.nCar[i] = G.c.range(2, 3);
    for (int j = 0; j < k.nCar[i]; j++) k.cx[i][j] = j * (148.0f / k.nCar[i]) + G.c.range(0, 16);
  }
}

inline void stepCross(Games &G, const GBtn &b, float dt, uint32_t now) {
  GCross &k = G.cr;
  float s = dt / 1000.0f;
  if (b.lP && k.col > 0) { k.col--; gameSfx(700, 10); }
  if (b.rP && k.col < 15) { k.col++; gameSfx(700, 10); }
  if (b.okP) {
    k.row--;
    gameSfx(880, 15);
    if (k.row == 0) {  // sampai seberang
      G.c.score++;
      gameSfx(1319, 60); gameSfx(1568, 80);
      k.row = 5;
      crossLevel(G);
    }
  }
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < k.nCar[i]; j++) {
      float &x = k.cx[i][j];
      x += k.sp[i] * s;
      if (k.sp[i] > 0 && x > 128) x -= 148;
      if (k.sp[i] < 0 && x < -k.cw[i]) x += 148;
    }
  if (k.row >= 1 && k.row <= 4) {
    int lane = k.row - 1, px = k.col * 8;
    for (int j = 0; j < k.nCar[lane]; j++) {
      float x = k.cx[lane][j];
      if (px + 7 > x && px + 1 < x + k.cw[lane]) {
        k.lives--;
        k.hitAt = now;
        k.row = 5;
        gameSfx(200, 160);
        if (k.lives <= 0) { gameOver(G, now); return; }
        break;
      }
    }
  }
}

inline void drawCross(Games &G, Adafruit_GFX &g, uint32_t now) {
  GCross &k = G.cr;
  for (int x = 0; x < 128; x += 8) g.drawPixel(x + 3, crossRowY(0) + 3, C_ON);  // garis finis
  for (int r = 1; r <= 5; r++)
    for (int x = (r % 2) * 4; x < 128; x += 8) g.drawPixel(x, crossRowY(r) - 1, C_ON);
  for (int i = 0; i < 4; i++) {
    int y = crossRowY(i + 1) + 1;
    for (int j = 0; j < k.nCar[i]; j++) {
      int x = (int)k.cx[i][j];
      g.fillRoundRect(x, y, k.cw[i], 6, 2, C_ON);
      g.fillRect(x + (k.sp[i] > 0 ? k.cw[i] - 4 : 2), y + 1, 2, 2, C_OFF);  // kaca depan
    }
  }
  bool blink = k.hitAt && now - k.hitAt < 600 && (now / 80) % 2;
  if (!blink && G.c.st == GS_PLAY) {
    int x = k.col * 8 + 1, y = crossRowY(k.row) + 1;
    g.fillRoundRect(x, y + 1, 6, 5, 2, C_ON);
    g.drawPixel(x + 1, y, C_ON); g.drawPixel(x + 4, y, C_ON);
  }
  gHud(g, G.c.score, k.lives);
}

// ---------------- 22. Labirin ----------------
static const int MZ_DX[4] = {1, 0, -1, 0}, MZ_DY[4] = {0, 1, 0, -1};
static const uint8_t MZ_BIT[4] = {2, 4, 8, 1};  // T S B U (timur, selatan, barat, utara)

inline void mazeGen(Games &G, uint32_t now) {
  GMaze &k = G.mz;
  memset(k.wall, 15, sizeof(k.wall));
  static int16_t stack[GMaze::W * GMaze::H];
  bool seen[GMaze::H][GMaze::W] = {};
  int sp = 0;
  stack[sp++] = 0;
  seen[0][0] = true;
  while (sp) {
    int cur = stack[sp - 1], x = cur % GMaze::W, y = cur / GMaze::W;
    int opts[4], n = 0;
    for (int d = 0; d < 4; d++) {
      int nx = x + MZ_DX[d], ny = y + MZ_DY[d];
      if (nx >= 0 && ny >= 0 && nx < GMaze::W && ny < GMaze::H && !seen[ny][nx]) opts[n++] = d;
    }
    if (!n) { sp--; continue; }
    int d = opts[G.c.range(0, n - 1)];
    int nx = x + MZ_DX[d], ny = y + MZ_DY[d];
    k.wall[y][x] &= ~MZ_BIT[d];
    k.wall[ny][nx] &= ~MZ_BIT[(d + 2) % 4];
    seen[ny][nx] = true;
    stack[sp++] = ny * GMaze::W + nx;
  }
  k.px = k.py = 0;
  k.dir = 0;
  k.t0 = now;
  k.limit = (uint32_t)fmaxf(25000, 60000 - G.c.score * 5000);
}

inline void stepMaze(Games &G, const GBtn &b, float dt, uint32_t now) {
  GMaze &k = G.mz;
  if (now - k.t0 > k.limit) { gameOver(G, now); return; }
  bool step;
  if (b.padDir != 255 || b.padHold != 255) {  // gamepad: arah = langsung jalan ke sana, tahan = terus
    k.dir = b.padDir != 255 ? b.padDir : b.padHold;
    step = b.padDir != 255 || now - k.lastMove > 170;
  } else {
    if (b.lP) { k.dir = (k.dir + 3) % 4; gameSfx(700, 10); }
    if (b.rP) { k.dir = (k.dir + 1) % 4; gameSfx(700, 10); }
    step = b.okP || (b.ok && now - k.lastMove > 170);  // tahan OK = maju terus
  }
  if (!step) return;
  k.lastMove = now;
  if (k.wall[k.py][k.px] & MZ_BIT[k.dir]) { gameSfx(200, 25); return; }  // tembok
  k.px += MZ_DX[k.dir];
  k.py += MZ_DY[k.dir];
  if (k.px == GMaze::W - 1 && k.py == GMaze::H - 1) {
    G.c.score++;
    gameSfx(1319, 60); gameSfx(1568, 90);
    mazeGen(G, now);
  }
}

inline void drawMaze(Games &G, Adafruit_GFX &g, uint32_t now) {
  GMaze &k = G.mz;
  const int X0 = 4, Y0 = 8, C = 8;
  for (int y = 0; y < GMaze::H; y++)
    for (int x = 0; x < GMaze::W; x++) {
      int px = X0 + x * C, py = Y0 + y * C;
      uint8_t w = k.wall[y][x];
      if (w & 1) g.drawFastHLine(px, py, C + 1, C_ON);
      if (w & 8) g.drawFastVLine(px, py, C + 1, C_ON);
      if (w & 4) g.drawFastHLine(px, py + C, C + 1, C_ON);
      if (w & 2) g.drawFastVLine(px + C, py, C + 1, C_ON);
    }
  if ((now / 300) % 2) g.fillRect(X0 + (GMaze::W - 1) * C + 2, Y0 + (GMaze::H - 1) * C + 2, 5, 5, C_ON);  // tujuan
  int cx = X0 + k.px * C + C / 2, cy = Y0 + k.py * C + C / 2;
  int fx = MZ_DX[k.dir], fy = MZ_DY[k.dir];
  g.fillTriangle(cx + fx * 3, cy + fy * 3, cx - fx * 2 - fy * 2, cy - fy * 2 - fx * 2, cx - fx * 2 + fy * 2,
                 cy - fy * 2 + fx * 2, C_ON);
  float left = 1 - (float)(now - k.t0) / k.limit;  // sisa waktu
  if (left > 0) g.fillRect(0, 2, (int)(96 * left), 3, C_ON);
  gHud(g, G.c.score);
}

// ---------------- Mulai ulang game 15-22 ----------------
inline void resetExtra(Games &G, uint32_t now) {
  GameCtx &c = G.c;
  switch (c.cur) {
    case 14: {
      GCopter &k = G.co;
      k.y = 36; k.vy = 0; k.off = 0; k.dist = 0; k.center = 36; k.gap = 42;
      for (int i = 0; i < GCopter::N; i++) { k.top[i] = 15; k.bot[i] = 57; k.obs[i] = -1; }
      break;
    }
    case 15: {
      GStack &k = G.st;
      k.n = 1; k.lx[0] = 34; k.lw[0] = 60; k.x = 0; k.dir = 1; k.speed = 45; k.perfectAt = 0;
      break;
    }
    case 16: {
      GMole &k = G.mo;
      k.hole = -1; k.lives = 3; k.next = now + 800; k.fxAt = 0;
      break;
    }
    case 17: {
      GHiLo &k = G.hl;
      k.cur = c.range(2, 12); k.nxt = 1; k.phase = 0;
      break;
    }
    case 18: {
      G.ma.lives = 3; G.ma.fbAt = 0;
      mathNew(G, now);
      break;
    }
    case 19:
      tttNewRound(G, now);
      break;
    case 20: {
      GCross &k = G.cr;
      k.col = 8; k.row = 5; k.lives = 3; k.hitAt = 0;
      crossLevel(G);
      break;
    }
    case 21:
      G.mz.lastMove = 0;
      mazeGen(G, now);
      break;
  }
}

// ---------------- Ikon menu (24x24, pusat cx, cy) ----------------
inline void drawGameIcon(Adafruit_GFX &g, int id, int cx, int cy, uint32_t now) {
  switch (id) {
    case 0:  // burung & pipa
      g.fillRect(cx + 4, cy - 12, 6, 8, C_ON); g.fillRect(cx + 4, cy + 4, 6, 8, C_ON);
      g.fillCircle(cx - 5, cy + (int)(sinf(now / 200.0f) * 2), 4, C_ON);
      break;
    case 1:  // dino
      g.fillRect(cx - 6, cy - 2, 8, 8, C_ON); g.fillRect(cx - 2, cy - 7, 7, 6, C_ON);
      g.drawPixel(cx + 3, cy - 6, C_OFF);
      g.drawFastVLine(cx - 5, cy + 6, 4, C_ON); g.drawFastVLine(cx, cy + 6, 4, C_ON);
      g.fillRect(cx + 8, cy + 1, 3, 9, C_ON);
      break;
    case 2:  // ular
      for (int i = 0; i < 5; i++) g.fillRect(cx - 10 + i * 4, cy + (i % 2 ? 2 : -2), 4, 4, C_ON);
      g.fillRect(cx + 8, cy - 8, 3, 3, C_ON);
      break;
    case 3:  // bata
      for (int r = 0; r < 2; r++) for (int c = 0; c < 3; c++) g.fillRect(cx - 11 + c * 8, cy - 10 + r * 5, 7, 3, C_ON);
      g.fillCircle(cx + 2, cy + 3, 2, C_ON); g.fillRect(cx - 7, cy + 9, 14, 2, C_ON);
      break;
    case 4:  // pong
      g.fillRect(cx - 12, cy - 6, 2, 10, C_ON); g.fillRect(cx + 10, cy - 3, 2, 10, C_ON);
      g.fillRect(cx - 1 + (int)(sinf(now / 300.0f) * 6), cy - 1, 3, 3, C_ON);
      break;
    case 5:  // tangkap bintang
      drawSparkle(g, cx, cy - 6 + (int)((now / 60) % 8), 3);
      g.drawLine(cx - 8, cy + 6, cx - 5, cy + 11, C_ON); g.drawLine(cx + 8, cy + 6, cx + 5, cy + 11, C_ON);
      g.drawFastHLine(cx - 5, cy + 11, 11, C_ON); g.drawFastHLine(cx - 8, cy + 6, 17, C_ON);
      break;
    case 6:  // ingat urutan
      for (int i = 0; i < 3; i++) {
        bool on = (now / 350) % 3 == (uint32_t)i;
        if (on) g.fillRoundRect(cx - 12 + i * 9, cy - 6, 7, 12, 2, C_ON);
        else g.drawRoundRect(cx - 12 + i * 9, cy - 6, 7, 12, 2, C_ON);
      }
      break;
    case 7: {  // refleks: mata melirik
      int lx = (int)(sinf(now / 400.0f) * 3);
      g.fillRoundRect(cx - 11 + lx, cy - 6, 9, 12, 3, C_ON);
      g.fillRoundRect(cx + 2 + lx, cy - 6, 9, 12, 3, C_ON);
      break;
    }
    case 8:  // balok
      g.fillRect(cx - 9, cy + 3, 6, 6, C_ON); g.fillRect(cx - 3, cy + 3, 6, 6, C_ON);
      g.fillRect(cx - 3, cy - 3, 6, 6, C_ON); g.fillRect(cx + 3, cy + 3, 6, 6, C_ON);
      g.drawRect(cx + 3, cy - 10 + (int)((now / 200) % 8), 6, 6, C_ON);
      break;
    case 9:  // alien
      g.fillRect(cx - 7, cy - 6, 14, 8, C_ON);
      g.fillRect(cx - 4, cy - 4, 2, 2, C_OFF); g.fillRect(cx + 2, cy - 4, 2, 2, C_OFF);
      g.drawLine(cx - 7, cy + 2, cx - 9, cy + 5, C_ON); g.drawLine(cx + 6, cy + 2, cx + 8, cy + 5, C_ON);
      g.drawFastVLine(cx, cy + 6, 4, C_ON);
      break;
    case 10: {  // teriak: burung + gelombang suara
      g.fillCircle(cx + 4, cy, 4, C_ON);
      int w = (now / 250) % 3;
      for (int i = 0; i <= w; i++) g.drawCircle(cx - 4, cy, 3 + i * 4, C_ON);
      g.fillCircle(cx + 4, cy, 4, C_ON);
      break;
    }
    case 11:  // irama: tiga jalur
      for (int i = 0; i < 3; i++) g.drawRoundRect(cx - 12 + i * 9, cy + 7, 7, 4, 1, C_ON);
      g.fillRoundRect(cx - 3, cy - 10 + (int)((now / 60) % 16), 7, 3, 1, C_ON);
      break;
    case 12:  // mobil
      g.drawFastVLine(cx - 12, cy - 11, 22, C_ON); g.drawFastVLine(cx + 12, cy - 11, 22, C_ON);
      drawCar(g, cx, cy - 6);
      break;
    case 13: {  // lompat awan
      int jy = (int)(fabsf(sinf(now / 260.0f)) * -8);
      g.fillRoundRect(cx - 10, cy + 6, 20, 3, 1, C_ON);
      g.fillRoundRect(cx - 3, cy - 1 + jy, 7, 7, 2, C_ON);
      break;
    }
    case 14:  // helikopter
      g.fillRoundRect(cx - 5, cy - 2, 12, 6, 2, C_ON);
      g.drawFastHLine(cx - 12, cy, 7, C_ON);
      g.drawFastHLine(cx - 8 + (int)((now / 80) % 2) * 3, cy - 5, 16 - (int)((now / 80) % 2) * 6, C_ON);
      g.drawFastHLine(cx - 4, cy + 6, 10, C_ON);
      break;
    case 15:  // tumpukan
      for (int i = 0; i < 4; i++) g.fillRect(cx - 10 + i * 2, cy + 8 - i * 5, 20 - i * 3, 4, C_ON);
      break;
    case 16:  // tikus & lubang
      g.fillRoundRect(cx - 6, cy - 6 + (int)(fabsf(sinf(now / 250.0f)) * 3), 12, 12, 5, C_ON);
      g.fillRect(cx - 12, cy + 5, 24, 7, C_OFF);
      g.drawRoundRect(cx - 11, cy + 4, 22, 6, 3, C_ON);
      break;
    case 17:  // dua kartu
      g.drawRoundRect(cx - 12, cy - 8, 11, 16, 2, C_ON);
      g.fillRoundRect(cx + 1, cy - 8, 11, 16, 2, C_ON);
      g.setTextSize(1); g.setTextColor(C_ON); g.setCursor(cx - 9, cy - 3); g.print("7");
      break;
    case 18:  // hitung
      g.setTextSize(1); g.setTextColor(C_ON); g.setCursor(cx - 11, cy - 4); g.print("1+2");
      g.drawFastHLine(cx - 10, cy + 7, 20, C_ON);
      break;
    case 19:  // grid tiga sebaris
      g.drawFastVLine(cx - 3, cy - 9, 18, C_ON); g.drawFastVLine(cx + 3, cy - 9, 18, C_ON);
      g.drawFastHLine(cx - 9, cy - 3, 18, C_ON); g.drawFastHLine(cx - 9, cy + 3, 18, C_ON);
      g.drawLine(cx - 8, cy - 8, cx - 5, cy - 5, C_ON); g.drawCircle(cx + 6, cy + 6, 2, C_ON);
      break;
    case 20:  // jalan & mobil
      g.drawFastHLine(cx - 12, cy - 5, 24, C_ON); g.drawFastHLine(cx - 12, cy + 5, 24, C_ON);
      g.fillRoundRect(cx - 12 + (int)((now / 40) % 18), cy - 3, 8, 5, 2, C_ON);
      g.fillRect(cx - 2, cy + 8, 4, 4, C_ON);
      break;
    case 21:  // labirin
      g.drawRect(cx - 11, cy - 10, 22, 20, C_ON);
      g.drawFastVLine(cx - 4, cy - 10, 13, C_ON); g.drawFastVLine(cx + 3, cy - 3, 13, C_ON);
      g.drawFastHLine(cx - 11, cy + 3, 5, C_ON);
      g.fillRect(cx + 6, cy + 5, 3, 3, C_ON);
      break;
    default:  // keluar: pintu
      g.drawRect(cx - 6, cy - 10, 12, 20, C_ON);
      g.fillCircle(cx + 3, cy, 1, C_ON);
      g.drawLine(cx + 9, cy, cx + 13, cy, C_ON);
      g.drawLine(cx + 11, cy - 2, cx + 13, cy, C_ON); g.drawLine(cx + 11, cy + 2, cx + 13, cy, C_ON);
      break;
  }
}

inline void drawMenu(Games &G, Adafruit_GFX &g, uint32_t now) {
  GameCtx &c = G.c;
  gPrintC(g, "PILIH GAME", 0);
  int n = NGAMES + 1;
  // korsel: ikon bergeser halus, yang di tengah paling besar (dibingkai)
  for (int i = 0; i < n; i++) {
    float d = i - c.carousel;
    if (fabsf(d) > 2.2f) continue;
    int cx = 64 + (int)(d * 44);
    int cy = 26;
    if (fabsf(d) < 0.5f) g.drawRoundRect(cx - 17, cy - 15, 34, 30, 6, C_ON);
    drawGameIcon(g, i, cx, cy, now);
  }
  const char *name = c.sel < NGAMES ? GAME_NAMES[c.sel] : "Keluar";
  gPrintC(g, name, 44);
  if (c.sel < NGAMES) {
    char b[24];
    snprintf(b, sizeof(b), "Rekor: %d", gameHigh[c.sel]);
    gPrintC(g, b, 55);
  }
  // panah kiri/kanan berdenyut
  int a = (now / 300) % 2;
  g.fillTriangle(2 - a, 26, 6 - a, 22, 6 - a, 30, C_ON);
  g.fillTriangle(125 + a, 26, 121 + a, 22, 121 + a, 30, C_ON);
}

inline void drawOver(Games &G, Adafruit_GFX &g, uint32_t now) {
  GameCtx &c = G.c;
  float t = fminf(1, (now - c.overAt) / 300.0f);
  float e = 1 - (1 - t) * (1 - t);
  int h = (int)(44 * e);  // kotak membesar dari tengah
  g.fillRect(4, 32 - h / 2, 120, h, C_OFF);
  g.drawRoundRect(4, 32 - h / 2, 120, h, 4, C_ON);
  if (t < 1) return;
  const char *title = c.cur == 4 ? (c.won ? "KAMU MENANG!" : "ROBOT MENANG") : "SELESAI";
  gPrintC(g, title, 14);
  char b[24];
  snprintf(b, sizeof(b), "Skor: %d", c.score);
  gPrintC(g, b, 26);
  if (c.newRecord) {
    int bob = (int)(fabsf(sinf(now / 150.0f)) * 2);
    gPrintC(g, "REKOR BARU!", 36 - bob);
  } else {
    snprintf(b, sizeof(b), "Rekor: %d", gameHigh[c.cur]);
    gPrintC(g, b, 36);
  }
  g.setTextSize(1);
  g.setCursor(7, 45);
  g.print("Tap:lagi Tahan:menu");
}

// Gamepad: atas/bawah dipetakan sesuai game (tombol fisik hanya punya kiri, OK, kanan).
inline void gamePadMap(int game, GBtn &in) {
  switch (game) {
    case 2: case 21:  // Ular, Labirin: pakai arah mutlak dari gamepad
      if (in.padDir != 255) { in.lP = in.rP = false; }
      break;
    case 4:  // Pong: atas = naik, bawah = turun
      in.l |= in.u; in.r |= in.d; in.lP |= in.uP; in.rP |= in.dP;
      break;
    case 8:  // Balok: atas = putar, bawah = jatuhkan
      if (in.uP) in.okP = true;
      if (in.dP) { in.l = in.r = true; in.lP = true; }
      break;
    case 20:  // Seberang Jalan: atas = maju
      if (in.uP) in.okP = true;
      break;
    case 12: case 9:  // Balapan, Tembak Alien: atas juga = OK
      if (in.uP) in.okP = true;
      break;
    default:
      if (in.uP) in.okP = true;  // game satu tombol (Burung, Dino, Lompat...): atas = lompat
      in.ok |= in.u;
      break;
  }
}

typedef void (*StepFn)(Games &, const GBtn &, float, uint32_t);
typedef void (*DrawFn)(Games &, Adafruit_GFX &, uint32_t);
static const StepFn GAME_STEP[NGAMES] = {stepFlappy, stepDino, stepSnake, stepBreak, stepPong, stepCatch, stepSimon,
                                         stepReflex, stepTetris, stepInvade, stepVoice, stepRhythm, stepRace, stepJump,
                                         stepCopter, stepStack, stepMole, stepHiLo, stepMath, stepTTT, stepCross,
                                         stepMaze};
static const DrawFn GAME_DRAW[NGAMES] = {drawFlappy, drawDino, drawSnake, drawBreak, drawPong, drawCatch, drawSimon,
                                         drawReflex, drawTetris, drawInvade, drawVoice, drawRhythm, drawRace, drawJump,
                                         drawCopter, drawStack, drawMole, drawHiLo, drawMath, drawTTT, drawCross,
                                         drawMaze};

// ---------------- Satu frame game. false = keluar dari game ----------------
inline bool gameFrame(Games &G, Adafruit_GFX &g, const GBtn &b, uint32_t now, float dt) {
  GameCtx &c = G.c;
  if (dt > 60) dt = 60;  // setelah jeda panjang: jangan melompat jauh
  switch (c.st) {
    case GS_MENU: {
      int n = NGAMES + 1;
      if (b.backP) return false;  // B di menu = keluar dari game
      if (b.lP || b.uP) { c.sel = (c.sel + n - 1) % n; gameSfx(700, 15); }
      if (b.rP || b.dP) { c.sel = (c.sel + 1) % n; gameSfx(700, 15); }
      // korsel bergerak halus (dan mengambil jalan memutar saat melompat dari ujung ke ujung)
      float k = 1 - powf(0.0025f, dt / 1000.0f);
      c.carousel += (c.sel - c.carousel) * fminf(1, k * 6);
      if (b.okP) {
        if (c.sel == NGAMES) return false;
        c.cur = c.sel;
        c.st = GS_PLAY;
        gameSfx(1046, 40); gameSfx(1568, 60);
        gameReset(G, now);
      }
      drawMenu(G, g, now);
      break;
    }
    case GS_PLAY: {
      GBtn in = b;
      if (now - c.stateAt < 250) in = GBtn();  // abaikan tekanan sisa dari menu
      if (in.backP) { c.st = GS_MENU; c.stateAt = now; gameSfx(660, 30); drawMenu(G, g, now); break; }  // B = ke menu
      gamePadMap(c.cur, in);
      GAME_STEP[c.cur](G, in, dt, now);
      GAME_DRAW[c.cur](G, g, now);
      if (now - c.stateAt < 1300 && c.st == GS_PLAY) {  // nama & cara main di awal
        g.fillRect(0, 54, 128, 10, C_OFF);
        gPrintC(g, GAME_HINTS[c.cur], 55);
      }
      if (c.st == GS_OVER) drawOver(G, g, now);
      break;
    }
    case GS_OVER: {
      GAME_DRAW[c.cur](G, g, now);  // layar game tetap terlihat di belakang kotak
      drawOver(G, g, now);
      if (now - c.overAt > 500) {
        if (b.okP) { c.st = GS_PLAY; gameReset(G, now); gameSfx(1046, 40); }
        else if (b.lP || b.backP) { c.st = GS_MENU; c.stateAt = now; }
      }
      break;
    }
  }
  gameWantMic = c.st == GS_PLAY && c.cur == GAME_VOICE;
  return true;
}
