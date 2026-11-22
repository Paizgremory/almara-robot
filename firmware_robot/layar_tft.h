// layar_tft.h — layar TFT 1,8" 128x160 (ST7735, SPI) + slot microSD bawaan (modul V1.1).
//
// Cara kerja: seluruh kode gambar (wajah.h, tampilan.h, games.h, oled_info.h) TIDAK diubah karena
// tetap menggambar ke kanvas 1-bit berukuran 128x64 dan 128x32 (kelas MonoPanel, isi buffer sama
// persis dengan SSD1306). Setelah satu frame selesai, kanvas dipetakan ke layar fisik 128x160:
//
//     y   0..1    margin atas (2 piksel)
//     y   2..97   "OLED utama"  128x64 -> 128x96  (skala 1,5x vertikal, 1x horizontal)
//     y  98..103  celah (6 piksel)
//     y 104..151  "OLED info"   128x32 -> 128x48  (skala 1,5x vertikal)
//     y 152..159  margin bawah (8 piksel)
//
// Lebar kanvas (128) sama persis dengan lebar layar fisik (128) -> tidak ada penskalaan horizontal.
// Hanya pita (8 baris sumber = 12 baris layar) yang berubah yang dikirim; penuh tiap 1 detik.
//
// Wiring modul (layar + SD satu modul, berbagi MOSI/SCK):
//   VCC   -> 3,3V
//   GND   -> GND
//   CS    -> GPIO 47   (chip select layar)
//   RESET -> GPIO 38
//   A0/DC -> GPIO 42   (data/command)
//   SDA   -> GPIO 13   (MOSI)
//   SCK   -> GPIO 45
//   LED   -> GPIO 11   (lampu latar, PWM)
//   SD_CS   -> GPIO 9  (chip select kartu SD)
//   SD_MOSI -> GPIO 13 (sama dengan SDA layar)
//   SD_MISO -> GPIO 3
//   SD_SCK  -> GPIO 45 (sama dengan SCK layar)
//
// CATATAN: layar ST7735 dan SD berbagi bus SPI yang sama (FSPI). SD tetap pakai SPIClass terpisah
// (sdSpi) supaya sdmusik.h tidak perlu diubah, tapi pinnya sekarang mengacu ke bus yang sama.
// CS layar (GPIO 47) dan CS SD (GPIO 9) TIDAK boleh aktif bersamaan — sudah ditangani otomatis
// oleh library Adafruit dan SD karena masing-masing mengontrol CS-nya sendiri.

#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <freertos/semphr.h>

#ifndef SSD1306_WHITE
#define SSD1306_WHITE 1
#define SSD1306_BLACK 0
#define SSD1306_INVERSE 2
#endif

// ---- pin ----
#define TFT_SCK  45
#define TFT_MOSI 13
#define TFT_CS   47
#define TFT_DC   42
#define TFT_RST  38
#define TFT_BLK  11
#define TFT_BLK_CH 6   // kanal LEDC lampu latar

// ---- kecepatan & mode SPI ----
// ST7735 umumnya stabil di 27 MHz. Naikkan ke 40 MHz kalau ingin lebih cepat,
// turunkan ke 20 MHz kalau layar bergaris atau acak.
#define TFT_SPI_HZ   27000000UL
#define TFT_SPI_MODE SPI_MODE0  // ST7735 pakai MODE0 (berbeda dari ST7789 yang MODE3)

// ---- warna ----
#define C565(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))
#define TFT_BG_DARK   C565(0,0,0)
#define TFT_MAIN_FG   C565(120,215,255)   // warna kanvas utama (mode gelap)
#define TFT_INFO_FG   C565(255,190,60)    // warna kanvas info  (mode gelap)
#define TFT_BG_LIGHT  C565(238,238,230)
#define TFT_FG_LIGHT  C565(15,25,60)
#define TFT_LINE      C565(40,70,110)

// ---- tata letak layar 128x160 ----
// Kanvas utama 128x64 -> dipetakan ke 128x96 (tinggi x1.5)
// Kanvas info  128x32 -> dipetakan ke 128x48 (tinggi x1.5)
#define TFT_W        128
#define TFT_H        160
#define TFT_MAIN_Y   2       // baris layar mulai kanvas utama
#define TFT_MAIN_PH  96      // tinggi piksel layar untuk kanvas utama (64 baris x 1.5)
#define TFT_INFO_Y   104     // baris layar mulai kanvas info
#define TFT_INFO_PH  48      // tinggi piksel layar untuk kanvas info  (32 baris x 1.5)

// Tinggi per baris sumber (8 baris per pita):
// 64 baris sumber -> 96 baris layar: tiap baris = 1 atau 2 piksel (pola 2,1,2,1,2,1,2,1 agar pas 12/pita x 8 = 96)
// Rumus: baris ke-r dalam pita -> (r%2==0) ? 2 : 1  => total per pita = 4*2+4*1 = 12 piksel
// 8 pita x 12 = 96 ✓
// Untuk info 32 baris -> 48: sama persis (4 pita x 12 = 48) ✓
static const uint8_t TFT_ROWH[8] = {2,1,2,1,2,1,2,1};  // tinggi piksel tiap baris sumber dalam satu pita

// ---------------------------------------------------------------------------------------------
// Kanvas 1-bit dengan tata letak buffer SSD1306 (1 byte = 8 piksel vertikal), API mirip Adafruit_SSD1306.
// ---------------------------------------------------------------------------------------------
class MonoPanel : public Adafruit_GFX {
  uint8_t *buf;
  uint16_t nbytes;
 public:
  MonoPanel(int16_t w, int16_t h) : Adafruit_GFX(w,h), buf(nullptr),
    nbytes((uint16_t)(w*((h+7)/8))) { buf=(uint8_t*)calloc(nbytes,1); }
  bool begin(uint8_t=0,uint8_t=0) { return buf!=nullptr; }
  uint8_t *getBuffer() { return buf; }
  void clearDisplay() { if(buf) memset(buf,0,nbytes); }
  void display() {}
  void invertDisplay(bool) {}
  void dim(bool);
  void ssd1306_command(uint8_t) {}
  bool getPixel(int16_t x,int16_t y) {
    if(x<0||y<0||x>=_width||y>=_height||!buf) return false;
    switch(getRotation()) {
      case 1:{int16_t t=x;x=WIDTH-1-y;y=t;break;}
      case 2:x=WIDTH-1-x;y=HEIGHT-1-y;break;
      case 3:{int16_t t=x;x=y;y=HEIGHT-1-t;break;}
    }
    return buf[x+(y>>3)*WIDTH]&(1<<(y&7));
  }
  void drawPixel(int16_t x,int16_t y,uint16_t color) override {
    if(x<0||y<0||x>=_width||y>=_height||!buf) return;
    switch(getRotation()) {
      case 1:{int16_t t=x;x=WIDTH-1-y;y=t;break;}
      case 2:x=WIDTH-1-x;y=HEIGHT-1-y;break;
      case 3:{int16_t t=x;x=y;y=HEIGHT-1-t;break;}
    }
    uint8_t *p=&buf[x+(y>>3)*WIDTH];
    uint8_t m=1<<(y&7);
    if(color==SSD1306_INVERSE) *p^=m;
    else if(color) *p|=m;
    else *p&=~m;
  }
};

// ---------------------------------------------------------------------------------------------
// Bus SPI: FSPI dipakai bersama layar (Adafruit) DAN SD (sdSpi di sdmusik.h).
// sdSpi di sdmusik.h juga dideklarasikan pakai FSPI supaya berbagi bus yang sama.
// CS layar = GPIO 47, CS SD = GPIO 9 — tidak pernah aktif bersamaan.
// ---------------------------------------------------------------------------------------------
static SPIClass tftSpi(FSPI);
static Adafruit_ST7735 tft(&tftSpi, TFT_CS, TFT_DC, TFT_RST);
static SemaphoreHandle_t tftMtx = nullptr;
static uint8_t tftLevel = 255;
static bool tftOk = false;

struct TftSent {
  uint8_t sent[1024];
  bool valid = false;
  bool light = false;
  uint32_t fullAt = 0;
};
static TftSent tftSentMain, tftSentInfo;

static inline void tftBacklight(uint8_t v) {
  tftLevel = v;
  pwmWrite(TFT_BLK, v);
}
static inline void tftContrast(uint8_t c) {
  uint32_t v = (uint32_t)c * 255 / 207;
  if(v>255) v=255;
  if(v<12)  v=12;
  tftBacklight((uint8_t)v);
}
inline void MonoPanel::dim(bool on) { tftContrast(on ? 40 : 207); }

static void tftFrameLines() {
  // garis pemisah tipis di atas dan bawah setiap zona
  tft.fillRect(0, TFT_MAIN_Y - 2,       TFT_W, 1, TFT_LINE);
  tft.fillRect(0, TFT_MAIN_Y + TFT_MAIN_PH + 1, TFT_W, 1, TFT_LINE);
  tft.fillRect(0, TFT_INFO_Y - 2,       TFT_W, 1, TFT_LINE);
  tft.fillRect(0, TFT_INFO_Y + TFT_INFO_PH + 1, TFT_W, 1, TFT_LINE);
}

static bool tftBegin() {
  if(tftMtx) return tftOk;
  tftMtx = xSemaphoreCreateMutex();
  pwmAttach(TFT_BLK, TFT_BLK_CH, 5000, 8);
  tftBacklight(0);
  tftSpi.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  // initR: INITR_BLACKTAB = tab hitam paling umum untuk modul 1.8" ini.
  // Kalau warna terbalik/aneh, coba INITR_REDTAB atau INITR_GREENTAB.
  tft.initR(INITR_BLACKTAB);
  tft.setSPISpeed(TFT_SPI_HZ);
  tft.setRotation(0);  // 0=portrait normal. Ganti ke 2 kalau terbalik atas-bawah.
  tft.fillScreen(TFT_BG_DARK);
  tftFrameLines();
  tftBacklight(255);
  tftOk = true;
  return true;
}

// Kirim satu kanvas ke layar. y0 = baris layar teratas. Hanya pita yang berubah dikirim.
// Lebar layar = 128 = lebar kanvas -> tidak ada penskalaan horizontal, hanya vertikal (x1.5).
static void tftShow(MonoPanel &pan, TftSent &st, int y0, uint16_t fgDark, bool light, bool force) {
  if(!tftOk) return;
  // Buffer satu pita: 128 piksel lebar x 12 piksel tinggi maksimal (sum TFT_ROWH = 12)
  static uint16_t band[128 * 12];
  const int W = pan.width();   // selalu 128
  const int P = pan.height() / 8;
  uint8_t *b = pan.getBuffer();
  uint32_t now = millis();
  const uint16_t fg = light ? TFT_FG_LIGHT : fgDark;
  const uint16_t bg = light ? TFT_BG_LIGHT : TFT_BG_DARK;
  xSemaphoreTake(tftMtx, portMAX_DELAY);
  bool full = force || !st.valid || light!=st.light || now-st.fullAt>1000;
  if(full) { st.fullAt=now; st.valid=true; st.light=light; tftFrameLines(); }
  tft.startWrite();
  for(int p=0; p<P; p++) {
    uint8_t *nb = b + p*W, *ob = st.sent + p*W;
    if(!full && !memcmp(nb,ob,W)) continue;
    // hitung tinggi total pita ini untuk setAddrWindow
    int pitaH = 0;
    for(int r=0;r<8;r++) pitaH += TFT_ROWH[r];  // = 12
    uint16_t *o = band;
    for(int r=0; r<8; r++) {
      uint16_t *row = o;
      for(int x=0; x<W; x++) {
        row[x] = (nb[x]>>r)&1 ? fg : bg;
      }
      // duplikasi baris sesuai TFT_ROWH[r]
      for(int k=1; k<TFT_ROWH[r]; k++)
        memcpy(row + W*k, row, W*sizeof(uint16_t));
      o += W * TFT_ROWH[r];
    }
    tft.setAddrWindow(0, y0 + p*pitaH, W, pitaH);
    tft.writePixels(band, W*pitaH);
    memcpy(ob, nb, W);
  }
  tft.endWrite();
  xSemaphoreGive(tftMtx);
}

static inline void tftShowMain(MonoPanel &pan, bool light, bool force=false) {
  tftShow(pan, tftSentMain, TFT_MAIN_Y, TFT_MAIN_FG, light, force);
}
static inline void tftShowInfo(MonoPanel &pan, bool force=false) {
  tftShow(pan, tftSentInfo, TFT_INFO_Y, TFT_INFO_FG, false, force);
}
