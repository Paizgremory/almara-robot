// Almara (robot ngobrol) — ESP32-S3 N16R8. Kontrol lewat remote ESP32-C3 via BLE sendiri (ble_pad.h),
// BUKAN Bluepad32/gamepad X3, jadi pakai board package ESP32 Arduino biasa (bukan paket Bluepad32).
//
// Wiring (lihat juga panduan wiring):
//   Daya     : 1 sel Li-ion (+BMS) -> saklar -> step up 5V (min. 2A) -> 5V ESP32 (lewat dioda 1N5819),
//              VIN MAX98357A dari 3,3V terpisah (lihat bawah). Semua GND disatukan.
//   INMP441  : SCK=5, WS=4, SD=6, VDD=3.3V, GND=GND, L/R=GND
//   MAX98357A: BCLK=15, LRC=16, DIN=7, VIN=3,3V (regulator 3,3V >= 1A, BUKAN dari pin 3V3 ESP32; kapasitor 470uF + 100nF dekat VIN), GND=GND, SD=GPIO 2, GAIN dibiarkan kosong (9 dB)
//   Layar TFT IPS 1,3" 240x240 ST7789 (satu layar, menggantikan OLED 128x64 dan OLED 128x32): GND=GND, VCC=3.3V,
//                SCK=45, MOSI=13, DC=42, CS=47, RES=38, BLK=11 (lampu latar, PWM).
//   Sensor sentuh TTP223 : OUT=GPIO 10, VCC=3.3V, GND=GND (sentuh = bicara; tombol fisik 11/12/13 = kiri-atas / OK / kanan-bawah, ke GND, lihat bawah)
//   DHT22    : DATA=8, VCC=3.3V, GND=GND (sensor polos 4 kaki: tambah resistor 10k DATA ke 3.3V)
//   Tombol fisik (3 buah, tiap tombol: satu kaki ke GPIO, kaki lain ke GND): GPIO 11 = kiri/atas, GPIO 12 = OK/pilih, GPIO 13 = kanan/bawah
//   Modul SD     : SD built-in di modul layar TFT; CS=9, SCK=45, MOSI=13, MISO=3 (berbagi bus FSPI dengan layar)
//   Baterai  : pembagi 120k (atas) / 120k (bawah) dari + baterai 1 sel ke GPIO 1, 0,1 uF ke GND
//
// Cara pakai:
//   - Tahan tombol bicara (GPIO 10 / BOOT / LB gamepad) sambil bicara, lepas -> robot menjawab.
//     Mode KLIK ("mode bicara klik"): tekan sekali, robot berhenti mendengar sendiri saat kamu diam.
//   - Tekan tombol bicara saat robot bicara / memutar lagu / timer berbunyi -> berhenti. Tekan sebentar = dielus.
//   - Tahan tombol bicara 10 detik tanpa bicara -> setup WiFi (hotspot "Robot-Setup", buka 192.168.4.1).
//   - Remote ESP32-C3 (lewat BLE, lihat ble_pad.h & robot_remote_c3.ino): joystick kaki = navigasi
//     menu dan game, tombol B = berhenti/kembali, A = OK/dielus, Y = menu game, tombol bicara = LB,
//     tangan kiri/kanan = gerak lengan sekali ayun (gerak.h).
//   - Menu Pengaturan di layar berisi setelan robot dan setelan server (keluaran suara, mesin suara, otak AI,
//     laju & nada bicara, kekerasan, jam tenang). Tombol fisik 11/12/13 = atas / pilih / bawah.
//   - Perintah suara: lihat daftar di server (musik, radio, timer, pengingat, catatan, cuaca, game, dll).
//   - Update online: taruh firmware baru di ~/robot-server/firmware.bin, lalu ucapkan "perbarui firmware"
//     (token & WiFi tetap tersimpan). Flash kabel: file _0xE000.bin di alamat 0xE000 juga tidak menghapusnya.
//   - Kalau server mati, robot tetap hidup: wajah, game, dan suara cadangan tetap jalan.
//   - Restart tak normal: penyebab & langkah terakhir tampil di layar dan dikirim ke halaman /pengaturan.
//   - Tampilan layar TFT digambar di inti prosesor 0, audio dan jaringan di inti 1.
//   - v9.6: ganti layar ke TFT 1,8" 128x160 ST7735 + slot microSD built-in. SD musik memakai slot SD layar (tidak perlu modul SD terpisah).
//   - v9.5: layar tunggal IPS TFT 1,3" 240x240 (ST7789) menggantikan dua OLED (lihat layar_tft.h). Fitur motor (L298N / roda),
//     relay (lampu & lampu tembak), dan sensor ultrasonik HC-SR04 (ikuti aku, lambaian, sapa tamu, game Teremin) DIHAPUS.
//   - v9.1: OLED hanya mengirim bagian layar yang berubah (data I2C turun ~65%), update firmware WiFi dibuat selalu tuntas:
//     3 putaran unduh + sambung ulang WiFi, dan kalau tetap terhalang, otomatis lanjut lewat hotspot Almara-Update.
//   - v9.0: tampilan lebih mulus: OLED 128x32 di I2C 1 MHz & ~40 fps (dulu 400 kHz, 16-20 fps), OLED utama tanpa jeda berlebih
//     (secepat I2C mengizinkan) dan frame yang sama tidak dikirim ulang; gelombang suara OLED kecil tak lagi bergantung fps.
//   - v8.9: 11 tingkah wajah baru (konfeti, disko, equalizer, roket, hujan, centang, wow, zzz, riak, petir cas, cium),
//     muncul saat musik mulai/jeda/ganti lagu, gamepad tersambung, alarm, sedang diisi daya; menu pengaturan berdenyut saat diklik.
//   - v8.8: tiap ~35-75 dtk robot bicara sendiri (komentar iseng, fakta, lelucon, penyemangat, atau AJAKAN NGOBROL: mic langsung
//     terbuka tanpa disentuh). Dimatikan lewat menu Pengaturan > Robot > "Ajak ngobrol". Butuh server.py terbaru (endpoint /spontan).
//     Jam tenang: tidak bicara sendiri.
//   - Layar info (bagian bawah TFT): beranda berganti halaman (jam, mata hidup, suhu/kelembapan).

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <FFat.h>
#include <Update.h>
#include <DHT.h>
#include <esp_system.h>
bool safeMode = false;  // mode aman: gamepad Bluetooth tidak dinyalakan (setelah 2x restart berturut)
#include "kompat.h"   // ESP_I2S (Arduino 3.x) atau pembungkus I2S lama
#include "layar_tft.h"   // layar TFT 1,8" 128x160 (ST7735) + slot microSD built-in
#include "gerak.h"     // taruh setelah #include "layar_tft.h"
// di setup():  gerakBegin();
// di loop():   gerakUpdate();   // tidak pakai delay, aman dipanggil tiap putaran
#include "wajah.h"
#include "games.h"
#include "tipe.h"      // tipe Pad/Timer/Trans/Phrase + react()
#include "ble_pad.h"
#include "tampilan.h"
#include "oled_info.h"   // kanvas info 128x32 (bagian bawah layar TFT)

// WiFi tanpa hemat daya = lebih cepat. TAPI karena BLE (remote via ble_pad.h) aktif bareng WiFi,
// ESP32 WAJIB modem sleep supaya radio Bluetooth & WiFi tidak rebutan; mematikannya bisa membuat
// sistem abort (crash/restart).
inline void wifiFast() {
  WiFi.setSleep(WIFI_PS_MIN_MODEM);
}

#define MIC_SCK 5
#define MIC_WS  4
#define MIC_SD  6
#define SPK_BCLK 15
#define SPK_LRC  16
#define SPK_DIN  7
#define BTN_PIN  0    // tombol BOOT
#define TALK_PIN 10   // sensor sentuh TTP223: OUT ke GPIO 10, VCC 3,3V, GND (HIGH saat disentuh)
#define TALK_ACTIVE_HIGH 1   // 1 = TTP223 bawaan (HIGH saat disentuh). 0 = tombol biasa ke GND (LOW saat ditekan)
// Tombol fisik: GPIO 11 (kiri/atas), 12 (OK), 13 (kanan/bawah). Satu kaki tombol ke GPIO, satu kaki ke GND (pull-up internal).
// LOW = ditekan. Modul microSD sudah dipindah ke pin lain (lihat sdmusik.h).
#define GBTN_L  11
#define GBTN_OK 12
#define GBTN_R  13
inline int gbRead(uint8_t) { return HIGH; }  // tombol fisik sudah dihapus: pin 11/12 dipakai ulang (lampu TFT, servo), selalu "tidak ditekan"
#define BAT_PIN  1    // ADC baterai (opsional)
#define DHT_PIN  8    // sensor suhu & kelembapan DHT22
#define SAMPLE_RATE 16000
#define MAX_SECONDS 30          // lama bicara maksimal sekali tekan (detik)
#define MAX_REPLY_SECONDS 90    // ukuran buffer streaming (lagu boleh lebih panjang)
#define BLOCK 256
#define SEND_PEAK 20000         // volume rekaman yang dikirim ke Whisper
#define MIN_PEAK_24 20000       // batas penguatan rekaman
// MAX98357A dipasang di AMP_VCC volt (speaker 4 ohm 3W). Ayunan tegangan keluaran amplifier sebanding dengan VIN-nya,
// jadi di 3,3V batas "tidak pecah" hanya ~66% dari di 5V (daya maksimal sekitar 1,4W, aman untuk speaker 3W).
// Ganti AMP_VCC kalau VIN amplifier diubah lagi (mis. 5.0f).
#define AMP_VCC 3.3f
#define AMP_HEADROOM (AMP_VCC / 5.0f)   // 1.0 di 5V, 0.66 di 3,3V
// Volume 1..10 -> penguatan. Di atas 1.0 suara dipaksa lebih keras:
// bagian pelan ikut naik, puncaknya ditekan oleh limiter lembut (softLimit) supaya tidak pecah kasar.
static const float VOL_GAIN[11] = {0, 0.04f, 0.08f, 0.13f, 0.20f, 0.28f, 0.40f, 0.55f, 0.75f, 1.00f, 1.40f};

// Limiter lembut: di bawah lutut (20000 x AMP_HEADROOM) tidak diubah, di atasnya ditekan 4:1,
// puncak maksimal 32000 x AMP_HEADROOM (di 3,3V: lutut 13200, puncak 21120) supaya amplifier tidak terpotong.
inline int16_t softLimit(float y) {
  const float K = 20000.0f * AMP_HEADROOM;
  const float PEAK = 32000.0f * AMP_HEADROOM;
  float a = fabsf(y);
  if (a > K) a = fminf(PEAK, K + (a - K) * 0.25f);
  return (int16_t)(y < 0 ? -a : a);
}
#define TONE_VOL 5000
#define SETUP_HOLD_MS 10000
#define SERVER_PORT 8000
#define AP_NAME "Robot-Setup"
#define UPDATE_AP_NAME "Almara-Update"   // hotspot khusus "jalankan mode update firmware"
#define UPDATE_MODE_SEC 300               // tutup otomatis setelah 5 menit
#define QUIET_VOL_DEFAULT 3               // volume maksimal saat jam tenang (1..10), bisa diubah di menu Speaker
#define QUIET_CONTRAST 40                 // kecerahan layar saat jam tenang (bawaan 207)
#define BT_TX 17                          // ke ESP32 klasik (jembatan speaker Bluetooth) GPIO 16
#define BT_RX 18                          // dari ESP32 klasik GPIO 17 (laporan "BT:1" = speaker tersambung)
#define BT_BAUD 921600
#define BAT_RATIO 2.0f          // pembagi 120k / 120k (tegangan dibagi dua)
// Baterai 1 sel Li-ion (2x 18650 paralel): 4.2V penuh, ~3.0V habis. Kurva tegangan -> persen (turun tidak lurus).
static const float BAT_CURVE[][2] = {{4.20f, 100}, {4.10f, 90}, {4.00f, 80}, {3.90f, 65}, {3.80f, 50},
                                     {3.70f, 35}, {3.60f, 20}, {3.50f, 10}, {3.40f, 5}, {3.30f, 2}, {3.00f, 0}};
static const int BAT_CURVE_N = sizeof(BAT_CURVE) / sizeof(BAT_CURVE[0]);

int batPercentFromVolt(float v) {
  if (v >= BAT_CURVE[0][0]) return 100;
  for (int i = 1; i < BAT_CURVE_N; i++)
    if (v >= BAT_CURVE[i][0]) {
      float t = (v - BAT_CURVE[i][0]) / (BAT_CURVE[i - 1][0] - BAT_CURVE[i][0]);
      return (int)(BAT_CURVE[i][1] + t * (BAT_CURVE[i - 1][1] - BAT_CURVE[i][1]) + 0.5f);
    }
  return 0;
}
#define MAX_TIMERS 4
#define FW_VERSION "9.6"
SET_LOOP_TASK_STACK_SIZE(24 * 1024);  // decoder MP3 butuh tumpukan besar
#define HOT_C 31.0f            // di atas ini robot sesekali mengingatkan ruangan panas
#define HOT_REMIND_MS (2UL * 3600 * 1000)

I2SClass mic;
I2SClass spk;
// Kanvas 1-bit 128x64 (isi buffer sama dengan SSD1306). Dikirim berwarna ke bagian atas layar TFT (lihat layar_tft.h).
MonoPanel oled(128, 64);
Preferences prefs;
DHT dht(DHT_PIN, DHT22);
float roomT = NAN, roomH = NAN;   // suhu (C) & kelembapan (%) ruangan, NAN = belum terbaca
uint32_t lastHotRemind = 0;
WiFiManager wm;
WiFiManagerParameter pServer("server", "Alamat server (auto = hotspot HP, atau contoh 192.168.0.104)", "auto", 63);
WiFiManagerParameter pToken("token", "ROBOT_TOKEN (kosongkan = tetap pakai token lama)", "", 64);

const size_t MAX_SAMPLES = SAMPLE_RATE * MAX_SECONDS;
const size_t REPLY_CAP = SAMPLE_RATE * 2 * MAX_REPLY_SECONDS;
const size_t BODY_CAP = MAX_SAMPLES * 2 + 1024;
int32_t *recRaw = nullptr;   // rekaman 24-bit
int16_t *rec16 = nullptr;    // rekaman olahan 16-bit
uint8_t *body = nullptr;     // isi request (multipart + WAV)
uint8_t *reply = nullptr;    // buffer melingkar audio jawaban
int32_t rawBlock[BLOCK];

String serverCfg = "auto";
String tokenCfg = "";
String lastText = "";
int volLevel = 8;
float replyVolume = VOL_GAIN[8];
int quietVolLevel = QUIET_VOL_DEFAULT;   // volume maksimal jam tenang (menu Speaker)
int beepLevel = 5;                       // volume bip / nada (1..10, 5 = bawaan)
uint8_t oledBright = 10;                 // kecerahan layar 1..10 (10 = penuh)
bool hotRemindOn = true;                 // robot mengingatkan kalau ruangan panas
// Penjernih speaker (menu Speaker): potong bass (anti sember), potong desis, peredam kresek, batas puncak (anti pecah)
uint8_t spHp = 2;                        // indeks HPF_HZ (bawaan 120 Hz)
uint8_t spLp = 0;                        // indeks LPF_HZ (bawaan mati)
uint8_t spGate = 0;                      // indeks GATE_THR (bawaan mati)
int spCeil = 100;                        // batas puncak (% dari batas aman amplifier)
volatile bool spFxDirty = true;          // koefisien filter perlu dihitung ulang
bool serverOk = false;
bool fsOk = false;
int batPct = -1;             // -1 = baterai tidak terpasang
bool batWarned = false;
bool batShow = true;         // false = indikator baterai selalu disembunyikan
int batStable = 0;           // jumlah bacaan baterai stabil berturut-turut
float batSmooth = 0;         // tegangan baterai yang sudah dihaluskan
int batLowCount = 0;         // bacaan rendah berturut-turut sebelum memperingatkan

// ---- Game ----
volatile bool gameActive = false;
uint8_t viewMode = 0;            // 0 = wajah, 1 = teks, 2 = wajah + teks
volatile bool volAdjActive = false;   // bilah "atur volume" sedang tampil
volatile int8_t volAdjLevel = 8;
volatile bool volAdjDone = false;
volatile uint32_t volAdjUntil = 0;
volatile uint32_t volAdjOpenAt = 0, volAdjCloseAt = 0, volAdjMoveAt = 0;
volatile int8_t volAdjDir = 0;
bool quietOn = true;             // mode jangan ganggu (jadwal dari server)
int quietStart = 22, quietEnd = 5;
volatile bool quietNow = false;  // sedang di jam tenang?
// Alarm (tersimpan di memori robot, tetap ada walau restart; butuh jam internet sekali saat menyala)
volatile bool alarmOn = false;
volatile int8_t alarmH = 6, alarmM = 0, alarmRep = 1;  // rep: 0 sekali, 1 setiap hari, 2 Senin-Jumat
volatile int8_t alarmTone = 0;    // pilihan suara alarm (lihat O_ALTONE)
volatile bool alarmRinging = false;
volatile bool padAEdge = false;    // tombol A gamepad baru ditekan (untuk mematikan alarm)
String pendingReminder;          // pengingat berulang dari server yang menunggu diucapkan
uint8_t btMode = 0;              // speaker Bluetooth: 0 mati, 1 hanya Bluetooth, 2 robot + Bluetooth
volatile uint32_t btSeen = 0;    // kapan jembatan terakhir melapor
volatile bool btConn = false;    // speaker Bluetooth tersambung ke jembatan?
volatile int8_t zapReq = 0;      // -1 = tombol kiri (sebelumnya), +1 = tombol kanan (berikutnya)
String pendingCmd;               // perintah untuk server yang menunggu dikirim (diproses loop, tanpa rekursi)
volatile uint8_t quickReq = 0;   // 1 = ganti mode terang/gelap, 2 = ganti efek (dari tombol, diproses loop)
bool serverOld = false, serverOldShown = false;
volatile uint8_t gameReq = 0;          // 1 = buka menu, 2 = tutup (diproses task tampilan)
int gameHigh[NGAMES] = {0};
volatile bool gameHighDirty = false;
volatile float gameMicLevel = 0;   // kekerasan suara 0..1 untuk game Teriak Terbang
volatile bool gameWantMic = false;
struct Sfx { uint16_t f, ms; };
Sfx sfxQ[16];
volatile uint8_t sfxHead = 0, sfxTail = 0;
void gameSfx(int freq, int ms) {  // dipanggil dari task tampilan, diputar oleh loop utama
  uiClickAt = millis() | 1;
  uint8_t n = (sfxHead + 1) % 16;
  if (n == sfxTail) return;  // antrean penuh: lewati
  sfxQ[sfxHead] = {(uint16_t)freq, (uint16_t)ms};
  sfxHead = n;
}
bool faceMode = true;
bool lightMode = false;      // mode terang: latar putih
bool fxOn = true;            // efek latar animasi
String robotName = "Almara"; // nama dari server, untuk layar pembuka
bool jogetAuto = true;       // berjoget sendiri saat mendengar musik di ruangan
String pendingMsg;           // pesan dari HP yang menunggu diucapkan
// Teman belajar (Pomodoro)
bool focusOn = false, focusRest = false;
uint32_t focusEnd = 0, focusTotal = 0;
int focusWork = 25, focusBreak = 5, focusSessions = 0;
int musicId = 0;             // sesi musik terakhir (dari server)
float musicSec = 0;          // detik musik yang sudah terdengar saat dihentikan
String fwMd5;                // sidik firmware yang sedang berjalan (untuk cek pembaruan)
bool fwAvailable = false;

Timer timers[MAX_TIMERS];

// ---------------- Tampilan ----------------
// Semua penggambaran OLED dilakukan task tampilan. Kode lain hanya mengubah "disp".
struct Disp {
  bool face = true;            // true = mode wajah, false = mode teks
  bool mix = false;            // true = wajah + teks
  bool light = false;          // mode terang
  bool fx = true;              // efek latar
  char name[16] = "Almara";
  bool focusOn = false, focusRest = false;  // layar belajar
  uint32_t focusEnd = 0, focusTotal = 0;
  int focusSessions = 0;
  Act act = ACT_INFO;
  char expr[12] = "normal";
  bool joget = false;
  char title[40] = "";
  char text[1400] = "";
  uint32_t textSince = 0;
  uint32_t infoUntil = 0;      // ACT_INFO: 0 = tampil terus
  int bat = -1;
  bool online = true;
  char timerTxt[10] = "";
  char clock[6] = "";          // "HH:MM" dari internet, kosong kalau belum tersinkron
  uint8_t event = BH_NONE;     // tingkah yang dipicu (dielus, alarm)
  uint32_t eventAt = 0;
};
Disp disp;
SemaphoreHandle_t dispMtx;
volatile float audioLevel = 0;      // 0..1, untuk animasi mulut / gelombang suara
volatile uint32_t lastActivity = 0;

void setText(const String &title, const String &text) {
  xSemaphoreTake(dispMtx, portMAX_DELAY);
  strlcpy(disp.title, title.c_str(), sizeof(disp.title));
  strlcpy(disp.text, text.c_str(), sizeof(disp.text));
  disp.textSince = millis();
  xSemaphoreGive(dispMtx);
}

void setAct(Act a, uint32_t infoMs = 0) {
  xSemaphoreTake(dispMtx, portMAX_DELAY);
  disp.act = a;
  disp.infoUntil = infoMs ? millis() + infoMs : 0;
  xSemaphoreGive(dispMtx);
  lastActivity = millis();
}

void setExpr(const String &ekspresi, const String &gerak) {
  xSemaphoreTake(dispMtx, portMAX_DELAY);
  strlcpy(disp.expr, ekspresi.length() ? ekspresi.c_str() : "normal", sizeof(disp.expr));
  disp.joget = (gerak == "joget");
  xSemaphoreGive(dispMtx);
}

String clockText() {
  struct tm tm;
  if (!getLocalTime(&tm, 0)) return "";
  char b[6];
  snprintf(b, sizeof(b), "%02d:%02d", tm.tm_hour, tm.tm_min);
  return b;
}

void setStatus(const char *timerTxt) {
  String clk = clockText();
  xSemaphoreTake(dispMtx, portMAX_DELAY);
  strlcpy(disp.clock, clk.c_str(), sizeof(disp.clock));
  disp.bat = batPct;
  disp.online = serverOk && WiFi.status() == WL_CONNECTED;
  strlcpy(disp.timerTxt, timerTxt, sizeof(disp.timerTxt));
  xSemaphoreGive(dispMtx);
}

void faceEvent(Behav b) {
  xSemaphoreTake(dispMtx, portMAX_DELAY);
  disp.event = b;
  disp.eventAt = millis() | 1;  // tidak pernah 0
  xSemaphoreGive(dispMtx);
}

uint8_t viewFromName(const String &n) { return n == "TEKS" ? 1 : (n == "CAMPUR" ? 2 : 0); }
const char *viewName(uint8_t v) { return v == 1 ? "TEKS" : (v == 2 ? "CAMPUR" : "WAJAH"); }

void setView(uint8_t v) {
  viewMode = v > 2 ? 0 : v;
  faceMode = viewMode == 0;
  xSemaphoreTake(dispMtx, portMAX_DELAY);
  disp.face = viewMode == 0;
  disp.mix = viewMode == 2;
  xSemaphoreGive(dispMtx);
  prefs.begin("robot", false);
  prefs.putUChar("view", viewMode);
  prefs.putBool("face", viewMode != 1);
  prefs.end();
}

void setFaceMode(bool face) { setView(face ? 0 : 1); }

void setTheme(bool light) {
  lightMode = light;
  xSemaphoreTake(dispMtx, portMAX_DELAY);
  disp.light = light;
  xSemaphoreGive(dispMtx);
  prefs.begin("robot", false);
  prefs.putBool("light", light);
  prefs.end();
}

void setFx(bool on) {
  fxOn = on;
  xSemaphoreTake(dispMtx, portMAX_DELAY);
  disp.fx = on;
  xSemaphoreGive(dispMtx);
  prefs.begin("robot", false);
  prefs.putBool("fx", on);
  prefs.end();
}

void setRobotName(const String &name) {
  if (!name.length() || name == robotName) return;
  robotName = name;
  xSemaphoreTake(dispMtx, portMAX_DELAY);
  strlcpy(disp.name, name.c_str(), sizeof(disp.name));
  xSemaphoreGive(dispMtx);
  prefs.begin("robot", false);
  prefs.putString("name", name);
  prefs.end();
}

void syncFocus() {
  xSemaphoreTake(dispMtx, portMAX_DELAY);
  disp.focusOn = focusOn;
  disp.focusRest = focusRest;
  disp.focusEnd = focusEnd;
  disp.focusTotal = focusTotal;
  disp.focusSessions = focusSessions;
  xSemaphoreGive(dispMtx);
}

void startFocus(bool rest) {
  focusOn = true;
  focusRest = rest;
  focusTotal = (uint32_t)(rest ? focusBreak : focusWork) * 60000UL;
  focusEnd = millis() + focusTotal;
  syncFocus();
}

void stopFocus() {
  focusOn = false;
  syncFocus();
}

// Pesan sistem (setup, error): selalu tampil sebagai teks, di mode wajah sekalipun.
void screen(const String &title, const String &text, uint32_t ms = 0) {
  setText(title, text);
  setAct(ACT_INFO, ms);
  Serial.println(title + " | " + text);
}

void showIdle() {
  setText("Siap. Tekan tombol", lastText);
  setExpr("normal", "diam");
  setAct(ACT_IDLE);
}

void showError(const String &msg) {
  lastText = msg;
  setText("Perhatian", msg);
  setAct(ACT_INFO, 6000);  // di mode wajah: tampil 6 detik lalu kembali ke wajah
}

// ---- Transisi antar layar: digabung langsung di buffer layar ----
static uint8_t frameLast[1024], frameFrom[1024], frameNext[1024];

inline bool bufPix(const uint8_t *b, int x, int y) { return (b[(y >> 3) * 128 + x] >> (y & 7)) & 1; }
inline float easeInOut(float t) { return t < 0.5f ? 4 * t * t * t : 1 - powf(-2 * t + 2, 3) / 2; }

void composeTransition(uint8_t *out, Trans type, float p) {
  memcpy(frameNext, out, 1024);
  float e = easeInOut(p);
  if (type == TR_LEFT || type == TR_RIGHT) {  // geser per kolom (cepat)
    int off = (int)(e * 128);
    for (int pg = 0; pg < 8; pg++) {
      uint8_t *o = out + pg * 128;
      const uint8_t *fr = frameFrom + pg * 128, *nx = frameNext + pg * 128;
      for (int x = 0; x < 128; x++) {
        if (type == TR_LEFT) o[x] = x + off < 128 ? fr[x + off] : nx[x + off - 128];
        else o[x] = x - off >= 0 ? fr[x - off] : nx[x - off + 128];
      }
    }
    return;
  }
  memset(out, 0, 1024);
  int off = (int)(e * 64);
  float r = e * 74, r2 = r * r;
  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 128; x++) {
      bool on;
      if (type == TR_UP) on = y + off < 64 ? bufPix(frameFrom, x, y + off) : bufPix(frameNext, x, y + off - 64);
      else if (type == TR_DOWN) on = y - off >= 0 ? bufPix(frameFrom, x, y - off) : bufPix(frameNext, x, y - off + 64);
      else {  // TR_IRIS: lingkaran membuka dari tengah, dengan cincin di tepinya
        float dx = x - 64, dy = (y - 32) * 1.15f, d2 = dx * dx + dy * dy;
        on = d2 < r2 ? bufPix(frameNext, x, y) : bufPix(frameFrom, x, y);
        if (d2 >= (r - 1.5f) * (r - 1.5f) && d2 < r2 && p < 0.95f) on = true;
      }
      if (on) out[(y >> 3) * 128 + x] |= 1 << (y & 7);
    }
  }
}

void setContrast(uint8_t v) {   // kecerahan = lampu latar TFT (BLK)
  tftContrast(v);
}

// ---------------- Gamepad X3 (BLE, langsung ke S3) ----------------

bool btActive();

Pad padGet() {
  Pad p;
  taskENTER_CRITICAL(&padMux);
  p = padNow;
  taskEXIT_CRITICAL(&padMux);
  if (millis() - p.at > 400) p.conn = false;  // data basi = anggap terputus (dianggap tidak ada gamepad)
  return p;
}
volatile int8_t bridgeSlot = -1;  // 0 = slot speaker, 1 = slot gamepad, -1 = belum tahu / firmware lama
volatile uint32_t slotSeen = 0;
int8_t slotWant = -1;
uint32_t slotReqAt = 0;
uint8_t slotTries = 0;
uint32_t lastMusicEnd = 0;
uint8_t talkMode = 0;             // 0 = tahan lalu bicara, 1 = klik sekali (berhenti sendiri saat diam)
volatile bool musicPlaying = false;
volatile bool otaBusy = false;     // update firmware berjalan: Bluetooth gamepad dimatikan supaya WiFi tidak berebut radio
volatile bool setActive = false;   // menu pengaturan OLED terbuka
const bool lcdActive = false;      // LCD 16x2 tidak dipakai: semua tampilan di OLED
volatile bool dancing = false;    // joget karena mendengar musik di ruangan
volatile bool listeningNow = false;
bool batCritical = false;           // baterai benar-benar habis
volatile bool sdActive = false;   // sedang memutar musik dari kartu SD
volatile bool padStop = false;    // tombol B: hentikan suara / musik
volatile bool tapListening = false;
bool spontanOn = true;             // robot bicara / mengajak ngobrol sendiri tanpa disentuh (menu "Ajak ngobrol")
volatile bool inviteListen = false;  // mic dibuka otomatis karena ROBOT yang mengajak ngobrol
uint32_t tapWaitMs = 6000;        // lama menunggu kamu mulai bicara di mode dengar otomatis
volatile float tapSilence = 0;
volatile uint8_t rumbleReq = 0;
// Baca laporan ESP32 klasik: "BT:1", "M:0/1", "G:1,lx,ly,rx,ry,tombol,dpad,misc" / "G:0"
void bridgeLine(char *s) {
  uint32_t now = millis();
  if (!strncmp(s, "BT:", 3)) {
    btSeen = now;
    btConn = s[3] == '1';
    if (bridgeSlot != 1) { bridgeSlot = bridgeSlot < 0 ? -1 : 0; }
  } else if (!strncmp(s, "M:", 2)) {
    bridgeSlot = s[2] == '1' ? 1 : 0;
    slotSeen = now;
  } else if (!strncmp(s, "G:", 2)) {
    Pad p;
    p.at = now;
    int lx, ly, rx, ry;
    unsigned b, d, m;
    if (s[2] == '1' && sscanf(s + 4, "%d,%d,%d,%d,%x,%x,%x", &lx, &ly, &rx, &ry, &b, &d, &m) == 7) {
      p.conn = true;
      p.lx = lx; p.ly = ly; p.rx = rx; p.ry = ry;
      p.btn = b; p.dpad = d; p.misc = m;
    }
    taskENTER_CRITICAL(&padMux);
    padNow = p;
    taskEXIT_CRITICAL(&padMux);
    bridgeSlot = 1;
    slotSeen = now;
  }
}

void bridgeRx() {
  static char line[96];
  static uint8_t len = 0;
  int n = Serial1.available();
  while (n-- > 0) {
    char c = Serial1.read();
    if (c == '\n') {
      line[len] = 0;
      bridgeLine(line);
      len = 0;
    } else if (c != '\r') {
      if (len < sizeof(line) - 1) line[len++] = c;
      else len = 0;
    }
  }
}

// Gamepad/remote dibaca langsung oleh ESP32-S3 lewat BLE sendiri (BUKAN Bluepad32 lagi) — lihat
// blePadBegin() / blePadTick() di ble_pad.h. rumbleReq tidak berlaku lagi (remote C3 tidak punya
// motor getar), dibiarkan sebagai variabel tak terpakai supaya kode lain yang menyetelnya tetap kompil.

void spkIdleTick();
// Task gamepad: membaca laporan gamepad / jembatan Bluetooth tiap 10 ms.
void padTask(void *) {
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    bridgeRx();
    blePadTick();
    spkIdleTick();
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(10));
  }
}

void sendBridgeCmd(uint8_t k) {
  uint8_t pkt[4] = {0xA5, 0x5C, k, (uint8_t)~k};
  Serial1.write(pkt, 4);
}

// ---- Notifikasi geser (digambar task tampilan) ----
struct Toast {
  uint8_t icon = 0;
  char t1[20] = "", t2[22] = "";
  uint32_t at = 0, dur = 0;
  bool spin = false;
};
Toast toastSt;

void toast(uint8_t icon, const char *t1, const char *t2, uint32_t ms = 2200, bool spin = false) {
  xSemaphoreTake(dispMtx, portMAX_DELAY);
  toastSt.icon = icon;
  strlcpy(toastSt.t1, t1, sizeof(toastSt.t1));
  strlcpy(toastSt.t2, t2, sizeof(toastSt.t2));
  toastSt.at = millis() | 1;
  toastSt.dur = ms;
  toastSt.spin = spin;
  xSemaphoreGive(dispMtx);
}

bool bridgeNew() { return bridgeSlot >= 0 && millis() - slotSeen < 4000; }  // jembatan dua slot terpasang

void requestSlot(int8_t s) {
  if (!bridgeNew()) return;  // tidak ada jembatan / firmware lama satu slot
  if (bridgeSlot == s) { slotWant = -1; return; }
  if (slotWant == s && millis() - slotReqAt < 4000) return;
  slotWant = s;
  slotReqAt = millis();
  slotTries = 1;
  sendBridgeCmd(s);
  if (s) toast(TI_PAD, "Mode kontrol", "Menyambung", 12000, true);
  else toast(TI_SPK, "Speaker BT", "Menyambung", 12000, true);
}

void slotHousekeeping() {
  static bool padWas = false, btWas = false;
  static int8_t slotWas = -2;
  Pad p = padGet();
  if (p.conn != padWas) {
    padWas = p.conn;
    if (p.conn) { toast(TI_OK, "Gamepad", "tersambung!", 2200); }
    else if (bridgeSlot == 1 || PAD_DIRECT) toast(TI_PAD, "Gamepad", "terputus", 2000);
  }
#if PAD_DIRECT
  (void)btWas; (void)slotWas;
  return;  // gamepad langsung di S3: tidak ada jembatan ESP32 / slot speaker
#endif
  bool bt = btActive();
  if (bt != btWas) {
    btWas = bt;
    if (bt) toast(TI_SPK, "Speaker BT", "tersambung!", 2200);
  }
  if (bridgeSlot != slotWas) {
    if (slotWas != -2 || bridgeSlot >= 0) {
      if (bridgeSlot == 1) toast(TI_PAD, "Mode kontrol", p.conn ? "aktif" : "Nyalakan pad", 3000, !p.conn);
      else if (bridgeSlot == 0 && btMode) toast(TI_SPK, "Speaker BT", bt ? "tersambung!" : "Mencari", 3500, !bt);
    }
    slotWas = bridgeSlot;
  }
  if (slotWant >= 0) {
    if (bridgeSlot == slotWant) {
      slotWant = -1;
    } else if (millis() - slotReqAt > 4000) {
      if (slotTries >= 3) {
        slotWant = -1;
        toast(TI_X, "Gagal pindah", "Cek kabel ESP", 2500);
      } else {
        slotTries++;
        slotReqAt = millis();
        sendBridgeCmd(slotWant);
      }
    }
  }
  // Kembali ke mode kontrol: speaker BT mati, atau 90 detik setelah musik terakhir
  if (bridgeNew() && bridgeSlot == 0 && slotWant < 0 && !musicPlaying &&
      (btMode == 0 || millis() - lastMusicEnd > 90000))
    requestSlot(1);
  if (gameActive && bridgeNew() && bridgeSlot == 0 && slotWant < 0) requestSlot(1);
  if (rumbleReq && bridgeSlot == 1) { sendBridgeCmd(rumbleReq); rumbleReq = 0; }
}

void saveMotion() {   // menyimpan mode bicara
  prefs.begin("robot", false);
  prefs.putUChar("tkm", talkMode);
  prefs.end();
}

void setTalkMode(uint8_t m, bool announce) {
  talkMode = m ? 1 : 0;
  saveMotion();
  if (announce) toast(TI_MIC, "Mode bicara:", talkMode ? "KLIK SEKALI" : "TAHAN TOMBOL", 2400);
}

// ---------------- Menu Pengaturan di OLED ----------------
// Buka: tahan OK (GPIO 12) 1 detik, START di gamepad, atau bilang "buka pengaturan".
// GPIO 11 = atas, GPIO 13 = bawah, GPIO 12 = pilih / simpan. Tombol bicara / B = keluar.
// Pengaturan milik server (dikirim di /health sebagai "srv"): keluaran, mesin suara, suara Edge, AI, laju,
// nada, keras jawaban (dB), batas speaker (x100), keras musik (dB), jam tenang, mulai, selesai.
volatile int16_t srvVal[12] = {0, 0, 0, 0, 0, 0, 6, 55, 0, 1, 22, 5};
volatile bool srvKnown = false;
enum SetId : uint8_t { S_VOL, S_VIEW, S_THEME, S_FX, S_TALK,
                       S_OUT, S_ENGINE, S_VOICE, S_AI, S_RATE, S_PITCH, S_TTSDB, S_LIMIT, S_MUSDB, S_QUIET, S_QSTART, S_QEND,
                       S_ALON, S_ALH, S_ALM, S_ALREP, S_ALTONE, S_ALTES,
                       S_JOGET, S_BAT,
                       S_INFO, S_FW, S_WIFI,
                       S_QVOL, S_BEEP, S_SPHP, S_SPLP, S_SPGATE, S_SPCEIL, S_SPTEST, S_SPRESET, S_BRIGHT, S_HOT, S_SPONTAN, S_ABOUT,
                       S_EXIT, S_COUNT };
struct SetDef { const char *label; int16_t mn, mx, step; const char *const *opts; uint8_t kind; };  // kind 0 angka, 1 pilihan, 2 aksi
static const char *const O_VIEW[] = {"Wajah", "Teks", "Campur"};
static const char *const O_THEME[] = {"Gelap", "Terang"};
static const char *const O_ONOFF[] = {"Mati", "Nyala"};
static const char *const O_TALK[] = {"Tahan", "Klik"};
static const char *const O_OUT[] = {"Robot", "HP", "Keduanya"};
static const char *const O_ENGINE[] = {"Edge", "Gemini"};
static const char *const O_VOICE[] = {"Gadis", "Ardi"};
static const char *const O_AI[] = {"Groq", "Gemini"};
static const char *const O_ALREP[] = {"Sekali", "Tiap hari", "Sen-Jum"};
#define N_ALTONE 12
static const char *const O_ALTONE[N_ALTONE] = {"Bip", "Weker", "Lonceng", "Melodi", "Sirene", "Burung",
                                               "Ayam", "Piano", "Arcade", "Lembut", "Kereta", "Ceria"};
static const char *const O_BAT[] = {"Sembunyi", "Otomatis"};
static const char *const O_HPF[] = {"Mati", "80Hz", "120Hz", "160Hz", "220Hz", "300Hz", "400Hz"};
static const char *const O_LPF[] = {"Mati", "7kHz", "6kHz", "5kHz", "4kHz", "3kHz"};
static const char *const O_GATE[] = {"Mati", "Rendah", "Sedang", "Tinggi"};
static const SetDef SETS[S_COUNT] = {
  {"Volume", 1, 10, 1, nullptr, 0},       {"Tampilan", 0, 2, 1, O_VIEW, 1},     {"Warna layar", 0, 1, 1, O_THEME, 1},
  {"Efek latar", 0, 1, 1, O_ONOFF, 1},    {"Mode bicara", 0, 1, 1, O_TALK, 1},
  // --- disimpan di server (HP); diubah lewat jaringan ---
  {"Keluaran", 0, 2, 1, O_OUT, 1},        {"Mesin suara", 0, 1, 1, O_ENGINE, 1}, {"Suara Edge", 0, 1, 1, O_VOICE, 1},
  {"Otak AI", 0, 1, 1, O_AI, 1},          {"Laju bicara", -30, 50, 5, nullptr, 0}, {"Nada suara", -30, 60, 5, nullptr, 0},
  {"Keras jawab", 0, 14, 1, nullptr, 0},  {"Batas spk", 40, 95, 5, nullptr, 0}, {"Keras musik", -12, 9, 1, nullptr, 0},
  {"Jam tenang", 0, 1, 1, O_ONOFF, 1},    {"Tenang dari", 0, 23, 1, nullptr, 0}, {"Tenang s/d", 0, 23, 1, nullptr, 0},
  // --- alarm ---
  {"Alarm", 0, 1, 1, O_ONOFF, 1},         {"Jam alarm", 0, 23, 1, nullptr, 0}, {"Menit alarm", 0, 55, 5, nullptr, 0},
  {"Ulangi", 0, 2, 1, O_ALREP, 1},        {"Suara alarm", 0, N_ALTONE - 1, 1, O_ALTONE, 1},
  {"Tes alarm", 0, 0, 0, nullptr, 2},
  // --- robot ---
  {"Joget musik", 0, 1, 1, O_ONOFF, 1},   {"Baterai", 0, 1, 1, O_BAT, 1},
  {"Info WiFi", 0, 0, 0, nullptr, 2},     {"Update FW", 0, 0, 0, nullptr, 2},
  {"Setup WiFi", 0, 0, 0, nullptr, 2},
  // --- speaker & lainnya (ditambahkan v8.7) ---
  {"Vol tenang", 1, 10, 1, nullptr, 0},   {"Vol bip", 1, 10, 1, nullptr, 0},
  {"Potong bass", 0, 6, 1, O_HPF, 1},     {"Potong desis", 0, 5, 1, O_LPF, 1},  {"Anti kresek", 0, 3, 1, O_GATE, 1},
  {"Anti pecah", 40, 100, 5, nullptr, 0}, {"Tes suara", 0, 0, 0, nullptr, 2},   {"Reset suara", 0, 0, 0, nullptr, 2},
  {"Kecerahan", 1, 10, 1, nullptr, 0},    {"Info panas", 0, 1, 1, O_ONOFF, 1},
  {"Ajak ngobrol", 0, 1, 1, O_ONOFF, 1},
  {"Info robot", 0, 0, 0, nullptr, 2},    {"Keluar", 0, 0, 0, nullptr, 2},
};
static const char *const SET_HELP[S_COUNT] = {  // penjelasan singkat di LCD (maks 16 huruf)
  "Keras suara", "Wajah/teks", "Gelap / terang", "Animasi latar", "Tahan / klik",
  "Speaker mana", "Edge / Gemini", "Suara Edge", "Groq / Gemini", "Cepat bicara", "Tinggi suara",
  "dB jawaban", "Puncak speaker", "dB musik", "Jangan ganggu", "Jam mulai", "Jam selesai",
  "Alarm nyala", "Jam bangun", "Menit bangun", "Ulang alarm", "Bunyi alarm", "Bunyikan alarm",
  "Joget dgn musik", "Indikator batre",
  "Status jaringan", "Perbarui robot",
  "Ganti WiFi",
  "Volume jam tenang", "Keras bip & nada", "Anti sember", "Anti desis", "Anti kresek", "Anti pecah", "Putar nada uji", "Setel ulang suara",
  "Terang layar", "Ingat ruang panas", "Bicara sendiri", "Versi & memori", "Tutup menu"};
static_assert(sizeof(SET_HELP) / sizeof(SET_HELP[0]) == S_COUNT, "SET_HELP harus lengkap");
static_assert(S_QEND - S_OUT == 11, "urutan pengaturan server harus sama dengan isi srv");
volatile bool setOpenReq = false;      // dari perintah suara
volatile int8_t setApplyId = -1;       // permintaan simpan untuk loop
volatile int16_t setApplyVal = 0;

int setGet(int id) {
  switch (id) {
    case S_VOL: return volLevel;
    case S_VIEW: return viewMode;
    case S_THEME: return lightMode;
    case S_FX: return fxOn;
    case S_TALK: return talkMode;
    case S_OUT: case S_ENGINE: case S_VOICE: case S_AI: case S_RATE: case S_PITCH:
    case S_TTSDB: case S_LIMIT: case S_MUSDB: case S_QUIET: case S_QSTART: case S_QEND:
      return srvVal[id - S_OUT];
    case S_JOGET: return jogetAuto;
    case S_BAT: return batShow;
    case S_ALON: return alarmOn;
    case S_ALH: return alarmH;
    case S_ALM: return alarmM;
    case S_ALREP: return alarmRep;
    case S_ALTONE: return alarmTone;
    case S_QVOL: return quietVolLevel;
    case S_BEEP: return beepLevel;
    case S_SPHP: return spHp;
    case S_SPLP: return spLp;
    case S_SPGATE: return spGate;
    case S_SPCEIL: return spCeil;
    case S_BRIGHT: return oledBright;
    case S_HOT: return hotRemindOn;
    case S_SPONTAN: return spontanOn;
    default: return 0;
  }
}

void setFmt(int id, int v, char *out, size_t n) {
  const SetDef &s = SETS[id];
  if (s.kind == 2) { strlcpy(out, id == S_EXIT ? "" : ">", n); return; }
  if (s.kind == 1) { strlcpy(out, s.opts[constrain(v, s.mn, s.mx)], n); return; }
  if (id == S_VOL || id == S_QVOL) snprintf(out, n, "%d%%", v * 10);
  else if (id == S_BEEP || id == S_BRIGHT) snprintf(out, n, "%d/10", v);
  else if (id == S_SPCEIL) snprintf(out, n, "%d%%", v);
  else if (id == S_RATE) snprintf(out, n, "%+d%%", v);
  else if (id == S_PITCH) snprintf(out, n, "%+dHz", v);
  else if (id == S_TTSDB) snprintf(out, n, "%ddB", v);
  else if (id == S_MUSDB) snprintf(out, n, "%+ddB", v);
  else if (id == S_LIMIT) snprintf(out, n, "0.%02d", v);
  else if (id == S_QSTART || id == S_QEND) snprintf(out, n, "%02d:00", v);
  else if (id == S_ALH) snprintf(out, n, "%02d", v);
  else if (id == S_ALM) snprintf(out, n, ":%02d", v);
  else snprintf(out, n, "%d", v);
}

// Masukan menu (task tampilan). up/down/ok/back = baru ditekan; upH/downH = ditahan (ulang otomatis).
struct SetUi { int sel = 0, val = 0; bool edit = false; float hl = 0, scroll = 0; uint32_t savedAt = 0, lastIn = 0, openAt = 0;
               int grp = -1, topSel = 0; };
SetUi setUi;

// Menu dibagi per kelompok. Halaman utama = daftar kelompok + Keluar; tiap kelompok diakhiri "Kembali".
#define M_BACK (-10)  // baris "Kembali" di dalam kelompok
static const char *const GRP_NAME[6] = {"Tampilan", "Suara & AI", "Speaker", "Alarm", "Robot", "Sistem"};
static const int8_t GRP0[] = {S_VIEW, S_THEME, S_FX, S_BRIGHT, S_BAT};
static const int8_t GRP1[] = {S_TALK, S_OUT, S_ENGINE, S_VOICE, S_AI, S_RATE, S_PITCH, S_QUIET, S_QSTART, S_QEND};
// Speaker: volume, lalu penjernih (sember / kresek / pecah). Urutan: coba "Tes suara" setelah mengubah satu pengaturan.
static const int8_t GRPS[] = {S_VOL, S_QVOL, S_BEEP, S_TTSDB, S_MUSDB, S_LIMIT, S_SPHP, S_SPLP, S_SPGATE, S_SPCEIL, S_SPTEST, S_SPRESET};
static const int8_t GRPA[] = {S_ALON, S_ALH, S_ALM, S_ALREP, S_ALTONE, S_ALTES};
static const int8_t GRP2[] = {S_JOGET, S_HOT, S_SPONTAN};
static const int8_t GRP3[] = {S_INFO, S_ABOUT, S_FW, S_WIFI};
static_assert(sizeof(GRP0) + sizeof(GRP1) + sizeof(GRPS) + sizeof(GRPA) + sizeof(GRP2) + sizeof(GRP3) == S_COUNT - 1, "semua pengaturan harus masuk kelompok");
// Isi daftar yang sedang tampil: id pengaturan (>=0), kelompok (-1..-4), atau M_BACK. Kembalikan jumlahnya.
int menuList(int grp, int8_t *out) {
  static const int8_t *const G[6] = {GRP0, GRP1, GRPS, GRPA, GRP2, GRP3};
  static const uint8_t GN[6] = {sizeof(GRP0), sizeof(GRP1), sizeof(GRPS), sizeof(GRPA), sizeof(GRP2), sizeof(GRP3)};
  int n = 0;
  if (grp < 0) {
    for (int i = 0; i < 6; i++) out[n++] = -1 - i;
    out[n++] = S_EXIT;
  } else {
    for (int i = 0; i < GN[grp]; i++) out[n++] = G[grp][i];
    out[n++] = M_BACK;
  }
  return n;
}

void settingsInput(bool up, bool down, bool upH, bool downH, bool ok, bool back, uint32_t now) {
  static uint32_t holdAt = 0, repAt = 0;
  bool rep = false;
  if (upH || downH) {
    if (!holdAt) holdAt = now;
    else if (now - holdAt > 420 && now - repAt > 90) { rep = true; repAt = now; }
  } else {
    holdAt = 0;
  }
  bool u = up || (rep && upH), d = down || (rep && downH);
  if (u || d || ok || back) setUi.lastIn = now;
  SetUi &m = setUi;
  int8_t list[40];
  int n = menuList(m.grp, list);
  if (m.sel >= n) m.sel = n - 1;
  int id = list[m.sel];
  auto enterList = [&](int grp, int sel) {
    m.grp = grp;
    m.sel = sel;
    m.hl = sel;
    m.scroll = constrain(sel - 1.5f, 0.0f, (float)max(0, menuList(grp, list) - 4));
    m.openAt = now;
  };
  if (m.edit) {
    const SetDef &s = SETS[id];
    if (u) m.val = s.kind == 1 ? (m.val + 1 > s.mx ? s.mn : m.val + 1) : min((int)s.mx, m.val + s.step);
    if (d) m.val = s.kind == 1 ? (m.val - 1 < s.mn ? s.mx : m.val - 1) : max((int)s.mn, m.val - s.step);
    if (u || d) gameSfx(880, 12);
    if (ok) {  // simpan
      setApplyVal = m.val;
      setApplyId = id;
      m.edit = false;
      m.savedAt = now;
      gameSfx(1046, 30); gameSfx(1568, 50);
    }
    if (back) { m.edit = false; gameSfx(440, 30); }
    return;
  }
  if (u) { m.sel = (m.sel + n - 1) % n; gameSfx(700, 12); }
  if (d) { m.sel = (m.sel + 1) % n; gameSfx(700, 12); }
  if (back) {
    if (m.grp >= 0) { enterList(-1, m.topSel); gameSfx(660, 30); return; }  // keluar kelompok
    setActive = false; gameSfx(660, 30); gameSfx(440, 50); return;
  }
  if (ok) {
    if (id <= -1 && id >= -6) { m.topSel = m.sel; enterList(-1 - id, 0); gameSfx(988, 25); return; }
    if (id == M_BACK) { enterList(-1, m.topSel); gameSfx(660, 30); return; }
    const SetDef &s = SETS[id];
    if (s.kind == 2) {
      if (id == S_EXIT) { setActive = false; gameSfx(660, 30); gameSfx(440, 50); return; }
      setApplyVal = 0;
      setApplyId = id;
      if (id != S_SPTEST && id != S_SPRESET && id != S_ABOUT) setActive = false;  // aksi lain menutup menu
      gameSfx(1046, 40);
    } else {
      m.edit = true;
      m.val = setGet(id);
      gameSfx(988, 25);
    }
  }
}

void settingsDraw(Adafruit_GFX &g, uint32_t now, float dt) {
  SetUi &m = setUi;
  float k = fminf(1, dt / 70.0f);
  m.hl += (m.sel - m.hl) * k;
  static int8_t list[40];
  int n = menuList(m.grp, list);
  if (m.sel >= n) m.sel = n - 1;
  float target = constrain(m.sel - 1.5f, 0.0f, (float)max(0, n - 4));
  m.scroll += (target - m.scroll) * k;
  if (fabsf(m.hl - m.sel) > 6) m.hl = m.sel;  // melompat dari ujung ke ujung: langsung
  if (fabsf(m.scroll - target) > 6) m.scroll = target;
  static SetRow rows[40];
  for (int i = 0; i < n; i++) {
    int id = list[i];
    if (id == M_BACK) { rows[i].label = "< Kembali"; rows[i].val[0] = 0; continue; }
    if (id < 0) { rows[i].label = GRP_NAME[-1 - id]; strlcpy(rows[i].val, ">", sizeof(rows[i].val)); continue; }
    rows[i].label = SETS[id].label;
    setFmt(id, (m.edit && i == m.sel) ? m.val : setGet(id), rows[i].val, sizeof(rows[i].val));
  }
  drawSettings(g, rows, n, m.sel, m.scroll, m.hl, m.edit, now, m.savedAt, now - m.openAt,
               m.grp < 0 ? "PENGATURAN" : GRP_NAME[m.grp]);
}



void displayTask(void *) {
  static Disp d;  // salinan lokal supaya kunci tidak ditahan selama menggambar
  static Games games;
  FaceAnim anim;
  anim.seed ^= esp_random();
  games.c.seed ^= esp_random();
  uint32_t bootAt = millis();
  int shownLight = -1;
  int lastKey = -1;
  Trans trans = TR_NONE;
  uint32_t transAt = 0;
  const uint32_t TRANS_MS = 340;
  float contrast = 207, contrastSent = 207;
  uint32_t lastT = millis(), fpsAt = millis(), frames = 0, worstMs = 0, sumMs = 0;
  bool bPrev[3] = {false, false, false};
  uint32_t bChange[3] = {0, 0, 0};
  TickType_t wake = xTaskGetTickCount();

  for (;;) {
    xSemaphoreTake(dispMtx, portMAX_DELAY);
    d = disp;
    xSemaphoreGive(dispMtx);
    uint32_t now = millis();
    float dt = now - lastT;
    lastT = now;
    if (d.act == ACT_INFO && d.infoUntil && now > d.infoUntil) d.act = ACT_IDLE;
    if (shownLight < 0) {  // pastikan chip layar di mode normal; warna dibalik oleh ESP32 sendiri
      oled.invertDisplay(false);
      shownLight = 0;
    }

    // ---- Tombol game (dengan peredam pantulan 30 ms) ----
    GBtn btn;
    const uint8_t pins[3] = {GBTN_L, GBTN_OK, GBTN_R};
    bool held[3], pressed[3];
    for (int i = 0; i < 3; i++) {
      bool raw = gbRead(pins[i]) == LOW;
      pressed[i] = false;
      if (raw != bPrev[i] && now - bChange[i] > 30) {
        bPrev[i] = raw;
        bChange[i] = now;
        pressed[i] = raw;
      }
      held[i] = bPrev[i];
    }
    btn.l = held[0]; btn.ok = held[1]; btn.r = held[2];
    btn.lP = pressed[0]; btn.okP = pressed[1]; btn.rP = pressed[2];

    // ---- Gamepad: arah (stik kiri / d-pad), A/X = OK, B = kembali / berhenti ----
    bool pU = false, pD = false, pUH = false, pDH = false, pOK = false, pB = false, pL = false, pR = false, pStart = false;
    {
      Pad p = padGet();
      static bool pPrev[8] = {false};  // kiri, kanan, atas, bawah, A/X, B, Y, START
      static bool pPrevX = false;
      bool cur[8] = {false};
      if (p.conn) {
        int th = 260, rel = 180;  // ambang dengan histeresis supaya stik tidak "berkedip"
        cur[0] = (p.dpad & PD_LEFT) || p.lx < -(pPrev[0] ? rel : th);
        cur[1] = (p.dpad & PD_RIGHT) || p.lx > (pPrev[1] ? rel : th);
        cur[2] = (p.dpad & PD_UP) || p.ly < -(pPrev[2] ? rel : th);
        cur[3] = (p.dpad & PD_DOWN) || p.ly > (pPrev[3] ? rel : th);
        cur[4] = p.btn & PB_A;
        cur[5] = p.btn & PB_B;
        cur[6] = p.btn & PB_Y;
        cur[7] = p.misc & PM_START;
      }
      bool xNow = p.conn && (p.btn & PB_X);
      bool edge[8];
      for (int i = 0; i < 8; i++) { edge[i] = cur[i] && !pPrev[i]; pPrev[i] = cur[i]; }
      bool xEdge = xNow && !pPrevX;
      pPrevX = xNow;
      pU = edge[2]; pD = edge[3]; pUH = cur[2]; pDH = cur[3]; pOK = edge[4] || xEdge; pB = edge[5];
      pL = edge[0]; pR = edge[1]; pStart = edge[7];
      if (edge[4]) padAEdge = true;
      // L2 = volume turun, R2 = volume naik (sekali klik = satu langkah, bilah volume muncul)
      static bool l2Prev = false, r2Prev = false;
      bool l2Now = p.conn && ((p.btn & 0x40) || p.l2 > (l2Prev ? 200 : 400));
      bool r2Now = p.conn && ((p.btn & 0x80) || p.r2 > (r2Prev ? 200 : 400));
      // Stik KANAN kiri/kanan = saluran radio / lagu sebelumnya / berikutnya (sama seperti tombol fisik 11 & 13)
      static int8_t rsPrev = 0;
      int8_t rs = 0;
      if (p.conn) rs = p.rx < -(rsPrev < 0 ? 200 : 380) ? -1 : (p.rx > (rsPrev > 0 ? 200 : 380) ? 1 : 0);
      if (rs && rs != rsPrev && musicId && !gameActive && !setActive && !volAdjActive) {
        react(BH_RIPPLE);
        zapReq = rs;
        lastActivity = now;
      }
      rsPrev = rs;
      // Stik KANAN atas/bawah saat radio / lagu diputar = volume naik / turun (ditahan = terus berubah)
      static int8_t rvPrev = 0;
      static uint32_t rvRepAt = 0;
      int8_t rv = 0;
      if (p.conn && musicPlaying) rv = p.ry < -(rvPrev > 0 ? 200 : 380) ? 1 : (p.ry > (rvPrev < 0 ? 200 : 380) ? -1 : 0);
      int rvStep = 0;
      if (rv && rv != rvPrev) { rvStep = rv; rvRepAt = now + 450; }
      else if (rv && (int32_t)(now - rvRepAt) >= 0) { rvStep = rv; rvRepAt = now + 220; }
      rvPrev = rv;
      int vstep = (r2Now && !r2Prev) - (l2Now && !l2Prev) + rvStep;
      l2Prev = l2Now; r2Prev = r2Now;
      if (vstep && !gameActive && !setActive) {
        if (!volAdjActive) {  // buka bilah volume langsung dengan nilai baru
          volAdjLevel = constrain(volLevel + vstep, 1, 10);
          volAdjDone = false;
          volAdjCloseAt = 0;
          volAdjOpenAt = now;
          volAdjActive = true;
        } else if (!volAdjCloseAt) {
          volAdjLevel = constrain(volAdjLevel + vstep, 1, 10);
        }
        volAdjUntil = now + 2500;  // tutup sendiri 2,5 dtk setelah klik terakhir
        volAdjMoveAt = now;
        volAdjDir = vstep;
        lastActivity = now;
      }
      if (gameActive || volAdjActive) {  // gamepad ikut mengendalikan game & pop-up volume
        btn.l |= cur[0]; btn.r |= cur[1]; btn.u = cur[2]; btn.d = cur[3];
        btn.ok |= cur[4] || xNow;
        btn.lP |= edge[0]; btn.rP |= edge[1]; btn.uP = edge[2]; btn.dP = edge[3];
        btn.okP |= edge[4] || xEdge;
        btn.backP = edge[5];
        static const uint8_t DIRS[4] = {2, 0, 3, 1};  // kiri, kanan, atas, bawah -> 0 kanan 1 bawah 2 kiri 3 atas
        for (int i = 0; i < 4; i++) {
          if (edge[i]) btn.padDir = DIRS[i];
          if (cur[i] && btn.padHold == 255) btn.padHold = DIRS[i];
        }
        if (cur[0] || cur[1] || cur[2] || cur[3] || cur[4]) lastActivity = now;
      } else if (p.conn && !setActive) {  // di wajah: Y = menu game, A = dielus, START = pengaturan, B = stop
        if (edge[6] && d.act == ACT_IDLE && now - bootAt > WELCOME_MS) {
          gameActive = true;
          games.c.st = GS_MENU;
          games.c.stateAt = now;
          games.c.sel = 0;
          games.c.carousel = 0;
          gameSfx(1046, 40);
        }
        static bool selPrev = false;
        static uint32_t selDownAt = 0;
        static bool selUsed = false;
        bool selNow = p.misc & 0x02;  // tombol SELECT / View: tahan 1,2 dtk = mode demo
        if (selNow && !selPrev) { selDownAt = now | 1; selUsed = false; }
        if (selNow && selDownAt && !selUsed && now - selDownAt > 1200 && d.act == ACT_IDLE) { selUsed = true; quickReq = 6; }
        if (!selNow) selDownAt = 0;
        selPrev = selNow;
        if (edge[4] && d.act == ACT_IDLE) quickReq = 4;
        if (edge[5]) padStop = true;
        if (p.lx > 150 || p.lx < -150 || p.ly > 150 || p.ly < -150 || p.rx > 150 || p.rx < -150 || p.ry > 150 || p.ry < -150 || p.dpad) lastActivity = now;
      }
    }

    // ---- Buka / tutup game ----
    if (gameReq == 1 && !gameActive) { gameActive = true; games.c.st = GS_MENU; games.c.stateAt = now; }
    if (gameReq == 2 && gameActive) { gameActive = false; lastActivity = now; }
    gameReq = 0;
    // ---- Menu pengaturan ----
    auto openSettings = [&]() {
      setActive = true;
      setUi.edit = false;
      setUi.openAt = setUi.lastIn = now;
      setUi.grp = -1;
      setUi.sel = setUi.topSel;
      setUi.hl = setUi.sel;
      setUi.scroll = 0;
      gameSfx(988, 30); gameSfx(1319, 50);
    };
    bool justOpened = false;
    if ((setOpenReq || (pStart && d.act == ACT_IDLE)) && !gameActive && !setActive && !volAdjActive) {
      openSettings();
      justOpened = true;  // START yang sama jangan langsung dianggap "tutup"
    }
    setOpenReq = false;
    if (setActive) {
      // GPIO 11 = atas, GPIO 13 = bawah, GPIO 12 = pilih. Gamepad: atas/bawah, kanan/kiri = ubah nilai, A, B.
      settingsInput(btn.lP || pU || (setUi.edit && pR), btn.rP || pD || (setUi.edit && pL), btn.l || pUH, btn.r || pDH,
                    btn.okP || pOK, pB || (pStart && !justOpened), now);
      if (now - setUi.lastIn > 45000) setActive = false;  // lama tidak disentuh: tutup sendiri
      btn = GBtn();
      lastActivity = now;
    }
    if (volAdjActive) {  // "atur volume": tombol hanya untuk volume selama bilah tampil
      if (!volAdjCloseAt) {
        if (btn.lP && volAdjLevel > 1) { volAdjLevel--; volAdjUntil = now + 10000; volAdjMoveAt = now; volAdjDir = -1; }
        if (btn.rP && volAdjLevel < 10) { volAdjLevel++; volAdjUntil = now + 10000; volAdjMoveAt = now; volAdjDir = 1; }
        if (btn.okP || (int32_t)(now - volAdjUntil) > 0) volAdjCloseAt = now ? now : 1;  // tutup beranimasi
      } else if ((int32_t)(now - volAdjCloseAt) >= VOLPOP_CLOSE_MS) {
        volAdjDone = true;  // animasi tutup selesai -> loop menyimpan
      }
      btn = GBtn();
      lastActivity = now;
    }
    static uint32_t rHeldAt = 0, lHeldAt = 0;
    if (!gameActive && d.act == ACT_IDLE && btn.r && !btn.ok && !btn.l) {  // tahan KANAN 1 dtk = terang/gelap
      if (!rHeldAt) rHeldAt = now;
      if (rHeldAt != 1 && now - rHeldAt > 1000) { quickReq = 1; rHeldAt = 1; }
    } else if (!btn.r) {
      if (rHeldAt > 1 && now - rHeldAt < 700 && (musicId || sdActive) && !gameActive) zapReq = 1;  // tekan singkat
      rHeldAt = 0;
    }
    if (!gameActive && d.act == ACT_IDLE && btn.l && !btn.ok && !btn.r) {  // tahan KIRI 1 dtk = efek latar
      if (!lHeldAt) lHeldAt = now;
      if (lHeldAt != 1 && now - lHeldAt > 1000) { quickReq = 2; lHeldAt = 1; }
    } else if (!btn.l) {
      if (lHeldAt > 1 && now - lHeldAt < 700 && (musicId || sdActive) && !gameActive) zapReq = -1;  // tekan singkat
      lHeldAt = 0;
    }
    // OK (saat robot diam): tekan sebentar = menu game, tahan 1 detik = menu pengaturan
    static uint32_t okDownAt = 0;
    uint32_t okHoldMs = 0;  // OK sedang ditahan: tampilkan titik-titik "menuju pengaturan"
    static bool okUsed = false;
    if (!gameActive && !setActive && d.act == ACT_IDLE && now - bootAt > WELCOME_MS) {
      if (btn.okP) { okDownAt = now | 1; okUsed = false; }
      if (btn.ok && okDownAt && !okUsed && now - okDownAt > 900) {
        okUsed = true;
        openSettings();
      }
      if (!btn.ok && okDownAt) {
        if (!okUsed && now - okDownAt < 900) {
          gameActive = true;
          games.c.st = GS_MENU;
          games.c.stateAt = now;
          games.c.sel = 0;
          games.c.carousel = 0;
          gameSfx(1046, 40);
        }
        okDownAt = 0;
      }
      if (okDownAt && !okUsed && now - okDownAt > 250) okHoldMs = now - okDownAt - 250;
      btn.okP = false;
    } else {
      okDownAt = 0;
    }
    if (gameActive) lastActivity = now;  // jangan tertidur saat bermain

    // ---- Kunci layar: berubah = jalankan transisi ----
    int key;
    if (gameActive) key = 100 + (games.c.st == GS_MENU ? 0 : 1 + games.c.cur);
    else if (setActive) key = 200;
    else key = (d.act == ACT_INFO) ? 2 : (d.mix ? 3 : (d.face ? 0 : 1));
    if (lastKey >= 0 && key != lastKey && now - bootAt > WELCOME_MS + 100) {
      bool wasGame = lastKey >= 100, isGame = key >= 100;
      if (key == 200 || lastKey == 200) trans = key == 200 ? TR_LEFT : TR_RIGHT;  // menu pengaturan: geser
      else if (wasGame != isGame) trans = TR_IRIS;             // masuk / keluar game
      else if (isGame) trans = key == 100 ? TR_RIGHT : TR_IRIS;  // ke menu: geser, ke game: iris
      else if (key == 2) trans = TR_UP;                        // pesan sistem muncul dari bawah
      else if (lastKey == 2) trans = TR_DOWN;
      else trans = key == 1 ? TR_LEFT : TR_RIGHT;              // wajah <-> teks
      transAt = now;
      memcpy(frameFrom, frameLast, 1024);
    }
    lastKey = key;

    // ---- Redup pelan saat tidur, terang lagi saat bangun ----
    bool sleepingNow = !gameActive && d.act == ACT_IDLE && (d.face || d.mix) && now - lastActivity > 120000;
    const float fullC = oledBright >= 10 ? 207.0f : oledBright * 20.0f;  // pengatur Kecerahan (1..10)
    float target = sleepingNow ? fminf(12.0f, fullC) : (quietNow ? fminf((float)QUIET_CONTRAST, fullC) : fullC);
    float rate = sleepingNow ? 0.06f : 0.6f;  // turun pelan (~3 dtk), naik cepat (~0,4 dtk)
    contrast += (target - contrast) * fminf(1, rate * dt / 16.0f);
    if (fabsf(contrast - contrastSent) >= 2 || (target == fullC && contrast > fullC - 2 && contrastSent != fullC)) {
      contrastSent = fabsf(contrast - target) < 2 ? target : contrast;
      setContrast((uint8_t)contrastSent);
    }

    oled.clearDisplay();
    if (now - bootAt < WELCOME_MS && (btn.lP || btn.okP || btn.rP)) bootAt = now - WELCOME_MS;  // lewati pembuka
    bool focusScreen = d.focusOn && d.act == ACT_IDLE && !gameActive && !setActive;
    if (drawWelcome(oled, now - bootAt, d.name)) {  // logo Almara (~2 detik), lalu wajah
      // (tidak ada transisi selama layar pembuka)
    } else if (focusScreen) {
      uint32_t left = (long)(d.focusEnd - now) > 0 ? d.focusEnd - now : 0;
      drawFocus(oled, d.focusRest, left, d.focusTotal, d.focusSessions, now);
    } else if (setActive) {
      settingsDraw(oled, now, dt);
    } else if (gameActive) {
      if (!gameFrame(games, oled, btn, now, dt)) {  // pilih "Keluar"
        gameActive = false;
        lastActivity = now;
      }
    } else if (d.mix && d.act != ACT_INFO) {  // wajah kecil di atas + teks di bawah
      static GFXcanvas1 faceCanvas(128, 64);
      struct tm tmNow;
      bool timeOk = getLocalTime(&tmNow, 0);
      faceCanvas.fillScreen(0);
      drawFace(faceCanvas, anim, d.act, exprFromName(d.expr), d.joget, audioLevel, now, now - lastActivity,
               d.event, d.eventAt, false, timeOk ? tmNow.tm_hour : -1, false);
      drawMixedText(oled, d.text, d.act, audioLevel, now, d.textSince, timeOk, tmNow.tm_hour, tmNow.tm_min);
      drawMixedFace(oled, faceCanvas);
      drawStatusBar(oled, d.bat, d.online, d.timerTxt, true);
    } else if (d.act == ACT_INFO || !d.face) {
      int w = drawStatusBar(oled, d.bat, d.online, d.timerTxt, false);
      struct tm tmNow;
      bool timeOk = getLocalTime(&tmNow, 0);
      if (d.act == ACT_IDLE && now - d.textSince > 20000) {  // diam 20 dtk: tampilkan beranda jam
        drawHome(oled, d.name, timeOk, tmNow.tm_hour, tmNow.tm_min, tmNow.tm_wday, tmNow.tm_mday, tmNow.tm_mon,
                 roomT, now, w);
        drawStatusBar(oled, d.bat, d.online, d.timerTxt, false);
      } else {
        const char *title = d.act == ACT_IDLE ? d.name : d.title;
        drawTextUI(oled, title, d.text, d.act, audioLevel, now, d.textSince, w);
        drawStatusBar(oled, d.bat, d.online, d.timerTxt, false);
      }
    } else {
      struct tm tmNow;
      int hour = getLocalTime(&tmNow, 0) ? tmNow.tm_hour : -1;  // siang: matahari, malam: bintang
      bool hot = !isnan(roomT) && roomT > 31;                    // ruangan panas: gelombang panas
      drawFace(oled, anim, d.act, exprFromName(d.expr), d.joget, audioLevel, now, now - lastActivity,
               d.event, d.eventAt, d.fx, hour, hot);
      drawStatusBar(oled, d.bat, d.online, d.timerTxt, true);
      if (d.act == ACT_IDLE && d.clock[0]) {  // jam kecil
        oled.setTextColor(C_ON);
        if (now - lastActivity > 120000) {  // tidur: jam besar di bawah
          oled.setTextSize(2);
          oled.setCursor(34, 47);
        } else {                             // diam: jam kecil di pojok kiri atas
          oled.setTextSize(1);
          oled.setTextColor(C_ON, C_OFF);    // latar polos supaya tidak bertumpuk dengan bintang
          oled.setCursor(0, 0);
        }
        oled.print(d.clock);
      }
    }
    {
      static uint8_t stPrev = GS_MENU;  // getar saat game selesai
      if (gameActive && games.c.st == GS_OVER && stPrev != GS_OVER) rumbleReq = 0x21;
      stPrev = gameActive ? games.c.st : GS_MENU;
    }
    if (!gameActive && d.act != ACT_INFO && (d.face || d.mix) && now - bootAt > WELCOME_MS) {
      if (tapListening) drawTapListen(oled, now, tapSilence);
    }
    if (okHoldMs) {  // tahan OK: tiga titik terisi berurutan, lalu menu pengaturan terbuka
      oled.fillRoundRect(46, 54, 36, 10, 4, C_OFF);
      for (int i = 0; i < 3; i++) {
        if (okHoldMs > (uint32_t)i * 210) oled.fillCircle(54 + i * 10, 59, 2, C_ON);
        else oled.drawCircle(54 + i * 10, 59, 2, C_ON);
      }
    }
    {
      Toast t;
      xSemaphoreTake(dispMtx, portMAX_DELAY);
      t = toastSt;
      xSemaphoreGive(dispMtx);
      if (t.at && now - bootAt > WELCOME_MS) {
        drawToast(oled, t.icon, t.t1, t.t2, now - t.at, t.dur, t.spin, now);
      }
    }
    if (volAdjActive && !volAdjDone) drawVolumeOverlay(oled, volAdjLevel, now, volAdjOpenAt, volAdjCloseAt, volAdjMoveAt, volAdjDir);
    if (trans != TR_NONE) {
      float p = (float)(now - transAt) / TRANS_MS;
      if (p >= 1) trans = TR_NONE;
      else composeTransition(oled.getBuffer(), trans, p);
    }
    static bool lightPrev = false;
    bool same = trans == TR_NONE && d.light == lightPrev && !memcmp(frameLast, oled.getBuffer(), 1024);   // gambar sama dengan frame lalu?
    lightPrev = d.light;
    memcpy(frameLast, oled.getBuffer(), 1024);
    static uint32_t lastSend = 0;
    static bool lightPrevSent = false;
    tftShowMain(oled, d.light, d.light != lightPrevSent);   // hanya pita yang berubah (tetap penuh tiap 1 dtk); mode terang = warna dibalik
    lightPrevSent = d.light; (void)lastSend; (void)same;

    frames++;
    {
      uint32_t ft = millis() - now;  // waktu gambar + kirim I2C untuk frame ini
      if (ft > worstMs) worstMs = ft;
      sumMs += ft;
    }
    if (now - fpsAt >= 10000) {  // laporan fps di Serial Monitor
      Serial.printf("[OLED] %.1f fps, rata-rata %.1f ms/frame, terburuk %u ms\n", frames * 1000.0f / (now - fpsAt),
                    (float)sumMs / (frames ? frames : 1), (unsigned)worstMs);
      frames = 0;
      worstMs = 0;
      sumMs = 0;
      fpsAt = now;
    }
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(same ? 20 : 10));  // secepat I2C mengizinkan (pegas & animasi berbasis waktu, jadi tetap mulus)
    if (xTaskGetTickCount() - wake > pdMS_TO_TICKS(50)) wake = xTaskGetTickCount();  // jangan mengejar ketinggalan
  }
}

// ---------------- Tombol ----------------
// Tombol bicara dan tombol BOOT punya fungsi yang sama.
bool hwTalkPressed() {
  static uint8_t onCnt = 0;  // sensor sentuh: harus terbaca aktif 3 kali berturut-turut (tahan noise)
  bool touch = (digitalRead(TALK_PIN) == (TALK_ACTIVE_HIGH ? HIGH : LOW));
  onCnt = touch ? (onCnt < 3 ? onCnt + 1 : 3) : 0;
  return digitalRead(BTN_PIN) == LOW || onCnt >= 3;
}

// Tombol LB gamepad juga tombol bicara.
bool talkPressed() {
  if (hwTalkPressed()) return true;
  Pad p = padGet();
  return p.conn && (p.btn & PB_LB);
}

void waitRelease() {
  while (talkPressed()) delay(10);
  delay(150);
}

// ---------------- Audio ----------------
// Jembatan siap kalau melapor dalam 3 detik terakhir dan speakernya tersambung.
bool btActive() { return btMode && btConn && millis() - btSeen < 3000; }

// Kirim satu potong suara ke jembatan: A5 5A, panjang (2 byte), PCM16. Volume robot dipetakan supaya
// level 10 = suara asli (speaker Bluetooth punya pengatur volume sendiri).
void btSend(const int16_t *data, size_t n) {
  static uint8_t pkt[4 + BLOCK * 2];
  const float boost = 1.0f / VOL_GAIN[10];
  pkt[0] = 0xA5;
  pkt[1] = 0x5A;
  pkt[2] = (n * 2) & 0xFF;
  pkt[3] = (n * 2) >> 8;
  for (size_t j = 0; j < n; j++) {
    int32_t v = (int32_t)(data[j] * boost);
    v = v > 32767 ? 32767 : (v < -32768 ? -32768 : v);
    pkt[4 + 2 * j] = v & 0xFF;
    pkt[5 + 2 * j] = (v >> 8) & 0xFF;
  }
  Serial1.write(pkt, 4 + n * 2);
}

void btPoll();

// Pin SD MAX98357A ke GPIO 2: speaker dimatikan saat robot diam -> tidak ada desis.
// (SD tidak disambung pun tetap jalan seperti biasa.)
#define SPK_SD 2
volatile int fxRamp = 0;        // sisa sampel fade-in setelah speaker dibangunkan (anti "kresek" saat menyala)
volatile uint32_t spkLastSound = 0;
volatile bool spkOn = false;
inline void spkWake() {
  spkLastSound = millis();
  if (!spkOn) { digitalWrite(SPK_SD, HIGH); spkOn = true; fxRamp = 160; delay(2); }
}
void spkIdleTick() {  // dipanggil berkala: matikan modul setelah 0,6 dtk tanpa suara
  if (spkOn && millis() - spkLastSound > 600) { digitalWrite(SPK_SD, LOW); spkOn = false; }
}

// ---------------- Penjernih speaker ----------------
// Diterapkan paling akhir, hanya untuk speaker robot (bukan ke jembatan Bluetooth), jadi suara jawaban,
// musik, bip, dan alarm semuanya ikut. Urutan: potong bass -> potong desis -> peredam kresek -> batas puncak.
//  - Potong bass (HPF 4 kutub): speaker kecil tidak sanggup memainkan bass; energi bass hanya bikin cone bergetar
//    ("sember" / "brebet"). Naikkan sampai suara tidak bergetar lagi.
//  - Potong desis (LPF 4 kutub): meredam nada tinggi yang tajam / mendesis.
//  - Peredam kresek: senyapkan suara sangat pelan di sela kata (desis & noise TTS). Kalau kata pelan ikut terpotong, turunkan.
//  - Batas puncak: puncak suara ditekan lebih rendah lagi sebelum amplifier, supaya tidak pecah di volume tinggi.
static const uint16_t HPF_HZ[7] = {0, 80, 120, 160, 220, 300, 400};
static const uint16_t LPF_HZ[6] = {0, 7000, 6000, 5000, 4000, 3000};
static const uint16_t GATE_THR[4] = {0, 70, 160, 320};
struct Biquad {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
  void design(bool highpass, float fc) {  // Butterworth 2 kutub (RBJ), fs = SAMPLE_RATE
    float w0 = 2.0f * PI * fc / SAMPLE_RATE, c = cosf(w0), al = sinf(w0) / (2.0f * 0.70710678f);
    float a0 = 1.0f + al;
    if (highpass) { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; }
    else          { b0 = (1 - c) / 2; b1 = 1 - c;    b2 = (1 - c) / 2; }
    a1 = -2.0f * c / a0; a2 = (1.0f - al) / a0;
    b0 /= a0; b1 /= a0; b2 /= a0;
  }
  inline float run(float x) {
    float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
};
static Biquad fxHp[2], fxLp[2];
static float fxGateEnv = 0, fxGateGain = 1;
static float fxDc = 0;                 // penahan DC: mencegah "tok" saat suara mulai / berhenti
static bool fxHpOn = false, fxLpOn = false;
static float fxGateThr = 0, fxCeil = 1.0f;

static void fxUpdate() {
  spFxDirty = false;
  fxHpOn = spHp > 0 && spHp < 7;
  fxLpOn = spLp > 0 && spLp < 6;
  if (fxHpOn) for (auto &b : fxHp) b.design(true, HPF_HZ[spHp]);
  if (fxLpOn) for (auto &b : fxLp) b.design(false, LPF_HZ[spLp]);
  fxGateThr = spGate < 4 ? GATE_THR[spGate] : 0;
  fxCeil = constrain(spCeil, 40, 100) / 100.0f;
}

// Batas puncak akhir: di bawah lutut tidak berubah, di atasnya ditekan 4:1 dan tidak pernah melewati puncak.
inline int16_t fxLimit(float y) {
  const float K = 20000.0f * AMP_HEADROOM * fxCeil;
  const float PEAK = 32000.0f * AMP_HEADROOM * fxCeil;
  float a = fabsf(y);
  if (a > K) a = fminf(PEAK, K + (a - K) * 0.25f);
  return (int16_t)(y < 0 ? -a : a);
}

// Satu sampel melewati seluruh penjernih.
inline int16_t fxProcess(int16_t in) {
  float x = in;
  fxDc += (x - fxDc) * 0.002f;  // ~12 Hz: buang DC
  x -= fxDc;
  if (fxHpOn) { x = fxHp[0].run(x); x = fxHp[1].run(x); }
  if (fxLpOn) { x = fxLp[0].run(x); x = fxLp[1].run(x); }
  if (fxGateThr > 0) {
    float a = fabsf(x);
    fxGateEnv = a > fxGateEnv ? a : fxGateEnv * 0.9995f;
    float tgt = fxGateEnv > fxGateThr ? 1.0f : 0.0f;
    fxGateGain += (tgt - fxGateGain) * (tgt > fxGateGain ? 0.05f : 0.002f);
    x *= fxGateGain;
  }
  if (fxRamp > 0) { x *= 1.0f - (float)fxRamp / 160.0f; fxRamp--; }
  return fxLimit(x);
}

void playSamples(const int16_t *data, size_t n) {
  int16_t stereo[BLOCK * 2];
  btPoll();  // laporan jembatan tetap terbaca selama lagu panjang diputar
  if (spFxDirty) fxUpdate();
  bool bt = btActive(), robotSpk = !bt || btMode == 2;  // speaker robot diam kalau "hanya Bluetooth"
  for (size_t i = 0; i < n; i += BLOCK) {
    size_t chunk = min((size_t)BLOCK, n - i);
    if (robotSpk) for (size_t j = 0; j < chunk; j++) if (abs(data[i + j]) > 40) { spkWake(); break; }
    if (bt) btSend(data + i, chunk);
    for (size_t j = 0; j < chunk; j++) stereo[2 * j] = stereo[2 * j + 1] = robotSpk ? fxProcess(data[i + j]) : 0;
    spk.write((uint8_t *)stereo, chunk * 4);  // tetap ditulis: menjaga tempo walau speaker robot diam
  }
}

// Laporan jembatan dibaca task gerak (bridgeRx), jadi tetap terbaca walau loop sedang sibuk.
void btPoll() {}

void setBtMode(uint8_t m) {
  btMode = m > 2 ? 0 : m;
  prefs.begin("robot", false);
  prefs.putUChar("btm", btMode);
  prefs.end();
}

void playSilence(int ms) {
  int16_t buf[BLOCK] = {0};
  for (size_t done = 0; done < (size_t)SAMPLE_RATE * ms / 1000; done += BLOCK) playSamples(buf, BLOCK);
}

// Volume yang dipakai: dibatasi saat jam tenang
float effVolume() {
  if (alarmRinging) return fmaxf(replyVolume, VOL_GAIN[8]);  // alarm: tidak dipelankan jam tenang
  return quietNow ? fminf(replyVolume, VOL_GAIN[quietVolLevel]) : replyVolume;
}

bool computeQuiet() {
  if (!quietOn) return false;
  struct tm tm;
  if (!getLocalTime(&tm, 0)) return false;
  int h = tm.tm_hour;
  return quietStart < quietEnd ? (h >= quietStart && h < quietEnd) : (h >= quietStart || h < quietEnd);
}

void playTone(float freq, int ms) {
  int16_t buf[BLOCK];
  size_t total = (size_t)SAMPLE_RATE * ms / 1000;
  float phase = 0, step = 2 * PI * freq / SAMPLE_RATE;
  float vol = TONE_VOL * (effVolume() / VOL_GAIN[8]) * (beepLevel / 5.0f);  // bunyi ikut level volume & pengatur Vol bip
  for (size_t done = 0; done < total; done += BLOCK) {
    for (int j = 0; j < BLOCK; j++) {
      buf[j] = vol * sinf(phase);
      phase += step;
      if (phase > 2 * PI) phase -= 2 * PI;
    }
    playSamples(buf, BLOCK);
  }
}

void okBeep()  { playTone(880, 90); playSilence(60); playTone(1320, 120); playSilence(80); }
void errBeep() { playTone(330, 150); playSilence(60); playTone(220, 250); playSilence(80); }

// Nada uji: meluncur dari bass ke nada tinggi lewat penjernih speaker. Dengarkan di mana speaker
// bergetar (sember = bass terlalu dalam), mendesis (nada tinggi), atau pecah (terlalu keras).
void playGlide(float f0, float f1, int ms, float amp) {
  int16_t buf[BLOCK];
  size_t total = (size_t)SAMPLE_RATE * ms / 1000;
  float phase = 0;
  for (size_t done = 0; done < total; done += BLOCK) {
    for (int j = 0; j < BLOCK; j++) {
      float t = fminf(1.0f, (float)(done + j) / total);
      phase += 2 * PI * f0 * powf(f1 / f0, t) / SAMPLE_RATE;
      if (phase > 2 * PI) phase -= 2 * PI;
      buf[j] = (int16_t)(amp * sinf(phase));
    }
    playSamples(buf, BLOCK);
    if (talkPressed()) break;
  }
}

void speakerTest(bool full) {
  float amp = 8000.0f * (effVolume() / VOL_GAIN[8]);  // kira-kira setingkat suara jawaban
  if (full) {
    playGlide(60, 400, 1300, amp);    // bass: di sini kelihatan kalau speaker sember
    playSilence(120);
    playGlide(400, 3600, 1300, amp);  // tengah ke tinggi: di sini kelihatan kalau pecah / mendesis
    playSilence(120);
    playGlide(3600, 7000, 800, amp * 0.6f);
  } else {
    playGlide(100, 3600, 900, amp);
  }
  playSilence(100);
}

// Putar satu blok PCM dengan volume, dan perbarui level untuk animasi mulut.
void playScaled(const int16_t *pcm, size_t n) {
  int16_t buf[BLOCK];
  float vol = effVolume();
  double sum = 0;
  for (size_t j = 0; j < n; j++) {
    buf[j] = softLimit(pcm[j] * vol);
    sum += (double)buf[j] * buf[j];
  }
  playSamples(buf, n);
  float lvl = fminf(1.0f, sqrtf(sum / n) / 2500.0f);
  audioLevel = audioLevel * 0.6f + lvl * 0.4f;
}

size_t readMic24(int32_t *out) {
  size_t n = mic.readBytes((char *)rawBlock, sizeof(rawBlock)) / 4;
  for (size_t i = 0; i < n; i++) out[i] = rawBlock[i] >> 8;
  return n;
}

// Rekam suara.
//  - Mode TAHAN: merekam selama tombol ditahan (maksimal MAX_SECONDS).
//  - Mode KLIK: tekan sebentar lalu lepas -> robot terus mendengar dan berhenti sendiri setelah kamu diam
//    ~1 detik (maks 15 detik). Tekan lagi = selesai sekarang. Ditahan lama tetap bekerja seperti mode tahan.
//  - autoListen: langsung mendengar tanpa tombol (lanjutan setelah robot bertanya).
// wantSetup = true kalau tombol ditahan >= 10 detik TANPA ada yang bicara (supaya bicara panjang
// tidak pernah membuka setup WiFi). noSpeech = true kalau sesi klik selesai tanpa ada suara.
#define TAP_HOLD_MS 450      // lebih lama dari ini = dianggap ditahan (mode tahan)
#define TAP_SILENCE_MS 1000  // diam selama ini setelah bicara = selesai
#define TAP_MAX_MS 15000
#define TAP_WAIT_MS 6000     // tidak ada suara sama sekali selama ini = batal
size_t record(bool *wantSetup, bool *noSpeech = nullptr, bool autoListen = false) {
  bool tapAllowed = talkMode == 1 || autoListen;
  setText(autoListen ? "Aku dengarkan..." : "Mendengarkan...",
          autoListen ? "Jawab saja, aku berhenti sendiri saat kamu diam." : "Lepaskan kalau sudah selesai bicara.");
  setAct(ACT_LISTEN);
  listeningNow = true;
  unsigned long t0 = millis();
  static int32_t block[BLOCK];
  size_t count = 0;
  float peakDb = -120;      // suara paling keras selama ditahan
  int shownLeft = -1;
  uint32_t releasedAt = 0;  // peredam pantulan: tombol dianggap lepas setelah 60 ms berturut-turut
  bool tapSession = autoListen, wasReleased = autoListen;
  float noise = -60;        // bising ruangan (dB) untuk deteksi diam
  int speechBlocks = 0;
  bool speechStarted = false;
  uint32_t silentMs = 0;
  size_t speechEnd = 0;
  if (tapSession) tapListening = true;
  while (count < MAX_SAMPLES) {
    bool pressed = talkPressed();
    if (!tapSession) {
      if (pressed) releasedAt = 0;
      else if (!releasedAt) releasedAt = millis() | 1;
      else if (millis() - releasedAt > 60) {
        if (tapAllowed && millis() - t0 < TAP_HOLD_MS) {  // klik singkat: lanjut mendengar sendiri
          tapSession = true;
          tapListening = true;
          wasReleased = true;
          setText("Aku dengarkan...", "Bicara saja. Aku berhenti sendiri saat kamu diam.");
        } else {
          break;
        }
      }
    } else {
      if (!pressed) wasReleased = true;
      else if (wasReleased && millis() - t0 > 600) {  // tekan lagi = selesai sekarang
        while (talkPressed()) delay(10);
        delay(60);
        break;
      }
    }
    size_t n = min(readMic24(block), MAX_SAMPLES - count);
    memcpy(recRaw + count, block, n * sizeof(int32_t));
    count += n;
    // Level suara untuk animasi (-65 dB .. -25 dB -> 0..1)
    double sum = 0, mean = 0;
    for (size_t i = 0; i < n; i++) mean += block[i];
    mean /= n ? n : 1;
    for (size_t i = 0; i < n; i++) { double d = block[i] - mean; sum += d * d; }
    float db = n ? 20.0f * log10f(sqrtf(sum / n) / 8388608.0f + 1e-9f) : -120;
    audioLevel = audioLevel * 0.6f + 0.4f * constrain((db + 65.0f) / 40.0f, 0.0f, 1.0f);
    uint32_t el = millis() - t0;
    if (el > 300) peakDb = fmaxf(peakDb, db);  // abaikan bunyi klik tombol di awal
    if (tapSession) {
      if (el > 350) {  // deteksi bicara / diam (setelah bunyi klik tombol lewat)
        if (db < noise) noise = noise * 0.8f + db * 0.2f;  // bising turun cepat, naik pelan
        else noise += 0.003f * (db - noise);
        bool speech = db > fmaxf(noise + 10, -58.0f);
        bool quiet = db < noise + 6;
        if (speech) { speechBlocks++; silentMs = 0; speechEnd = count; }
        else if (quiet || speechStarted) silentMs += n * 1000 / SAMPLE_RATE;
        if (speechBlocks >= 6) speechStarted = true;
        tapSilence = speechStarted ? fminf(1, (float)silentMs / TAP_SILENCE_MS) : 0;
      }
      if (speechStarted && silentMs >= TAP_SILENCE_MS) break;
      if (!speechStarted && el > tapWaitMs) {
        if (noSpeech) *noSpeech = true;
        count = 0;
        break;
      }
      if (el > TAP_MAX_MS) break;
    } else {
      // Ditahan 10 detik tanpa bicara = minta setup WiFi
      if (el >= SETUP_HOLD_MS && peakDb < -45) {
        *wantSetup = true;
        audioLevel = 0;
        listeningNow = false;
        return 0;
      }
      int left = MAX_SECONDS - (int)(el / 1000);
      if (left <= 5 && left != shownLeft) {  // hitung mundur 5 detik terakhir
        shownLeft = left;
        setText("Sisa " + String(left) + " detik...", "Lepaskan kalau sudah selesai bicara.");
      }
    }
  }
  audioLevel = 0;
  tapListening = false;
  tapSilence = 0;
  listeningNow = false;
  if (tapSession) {
    if (speechEnd && count > speechEnd + SAMPLE_RATE / 4) count = speechEnd + SAMPLE_RATE / 4;  // buang diam di akhir
    return count;
  }
  if (talkPressed()) {  // batas waktu habis: tunggu dilepas, lalu langsung dikirim
    setText("Batas bicara habis", "Lepas tombol, aku kirim sekarang.");
    while (talkPressed()) delay(10);
    delay(80);
  }
  return count;
}

// Buang klik tombol, high-pass, normalisasi puncak.
size_t processRecording(size_t count) {
  const size_t trim = SAMPLE_RATE / 20;
  if (count <= 2 * trim + SAMPLE_RATE / 4) return 0;  // < ~0,35 detik: dianggap tidak sengaja
  int32_t *x = recRaw + trim;
  size_t n = count - 2 * trim;
  float prevX = x[0], prevY = 0, peak = 0;
  for (size_t i = 0; i < n; i++) {
    float y = (float)x[i] - prevX + 0.995f * prevY;
    prevX = x[i];
    prevY = y;
    x[i] = (int32_t)y;
    if (fabsf(y) > peak) peak = fabsf(y);
  }
  float gain = SEND_PEAK / fmaxf(peak, (float)MIN_PEAK_24);
  for (size_t i = 0; i < n; i++) rec16[i] = constrain(lroundf(x[i] * gain), -32768L, 32767L);
  return n;
}

// ---------------- Jaringan ----------------
String serverUrl() {
  String s = serverCfg;
  s.trim();
  if (s.length() == 0 || s.equalsIgnoreCase("auto"))
    return "http://" + WiFi.gatewayIP().toString() + ":" + String(SERVER_PORT);
  if (!s.startsWith("http")) s = "http://" + s;
  while (s.endsWith("/")) s.remove(s.length() - 1);
  if (s.indexOf(':', s.indexOf("//") + 2) < 0) s += ":" + String(SERVER_PORT);
  return s;
}

String urlDecode(const String &s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    if (s[i] == '%' && i + 2 < s.length()) {
      char hex[3] = {s[i + 1], s[i + 2], 0};
      out += (char)strtol(hex, nullptr, 16);
      i += 2;
    } else {
      out += s[i];
    }
  }
  return out;
}

String urlEncode(const String &s) {
  String out;
  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

void writeWavHeader(uint8_t *h, uint32_t dataBytes) {
  auto w32 = [&](int o, uint32_t v) { h[o] = v; h[o + 1] = v >> 8; h[o + 2] = v >> 16; h[o + 3] = v >> 24; };
  auto w16 = [&](int o, uint16_t v) { h[o] = v; h[o + 1] = v >> 8; };
  memcpy(h, "RIFF", 4); w32(4, 36 + dataBytes); memcpy(h + 8, "WAVEfmt ", 8);
  w32(16, 16); w16(20, 1); w16(22, 1); w32(24, SAMPLE_RATE); w32(28, SAMPLE_RATE * 2);
  w16(32, 2); w16(34, 16); memcpy(h + 36, "data", 4); w32(40, dataBytes);
}

size_t buildBody(size_t n, const char *boundary) {
  String head = String("--") + boundary +
                "\r\nContent-Disposition: form-data; name=\"audio\"; filename=\"rekaman.wav\"" +
                "\r\nContent-Type: audio/wav\r\n\r\n";
  String tail = String("\r\n--") + boundary + "--\r\n";
  uint8_t *p = body;
  memcpy(p, head.c_str(), head.length()); p += head.length();
  writeWavHeader(p, n * 2); p += 44;
  memcpy(p, rec16, n * 2); p += n * 2;
  memcpy(p, tail.c_str(), tail.length()); p += tail.length();
  return p - body;
}

// Putar audio sambil mengunduh. Buffer melingkar REPLY_CAP byte di PSRAM, jadi
// panjang lagu tidak dibatasi memori. Mengembalikan true kalau dihentikan dengan tombol.
// Tanda langsung bahwa tombol diterima: bunyi klik singkat + tulisan di layar (sebelum server menjawab).
void zapFeedback(int8_t z) {
  faceEvent(BH_RIPPLE);
  setExpr("normal", "diam");
  setText("Ganti saluran...", z > 0 ? "Saluran berikutnya" : "Saluran sebelumnya");
  setAct(ACT_THINK);
  playTone(z > 0 ? 1320 : 990, 45);
}

// Tunggu tombol kiri & kanan dilepas supaya satu tekanan tidak terhitung dua kali.
void waitZapRelease() {
  uint32_t t = millis();
  while ((gbRead(GBTN_L) == LOW || gbRead(GBTN_R) == LOW) && millis() - t < 3000) delay(10);
  delay(60);
}

bool streamPlay(HTTPClient &http, size_t *played = nullptr, bool music = false) {
  WiFiClient *s = http.getStreamPtr();
  long remaining = http.getSize();          // -1 = panjang tidak diketahui
  const size_t cap = REPLY_CAP;             // genap
  const size_t prefill = min((size_t)SAMPLE_RATE, cap / 2);  // 0,5 detik audio sebelum mulai bunyi
  size_t wr = 0, rd = 0;                    // total byte masuk / sudah diputar
  bool netDone = false, started = false, stopped = false;
  unsigned long lastData = millis();
  int8_t zapHeld = 0;
  uint32_t zapSince = 0;
  int16_t buf[BLOCK];
  padStop = false;
  uint32_t playStart = millis();

  while (true) {
    // 1. Isi buffer dari jaringan selama masih ada ruang
    while (!netDone) {
      size_t space = cap - (wr - rd);
      if (space < 1024) break;
      int av = s->available();
      if (av <= 0) {
        if (remaining == 0 || !http.connected() || millis() - lastData > 8000) netDone = true;
        break;
      }
      size_t pos = wr % cap;
      size_t n = min((size_t)av, min(space, cap - pos));
      if (remaining > 0) n = min(n, (size_t)remaining);
      int r = s->read(reply + pos, n);
      if (r <= 0) break;
      wr += r;
      if (remaining > 0) remaining -= r;
      lastData = millis();
      if (remaining == 0) netDone = true;
    }
    if (netDone) WiFi.setSleep(true);  // unduhan selesai: WiFi hemat daya, arus 3.3V lebih lega

    if (talkPressed() || padStop) { padStop = false; stopped = true; break; }
    if (music && zapReq) { stopped = true; break; }
    if (music && alarmOn) {  // alarm tetap berbunyi walau sedang memutar radio / lagu
      static uint32_t alChk = 0;
      struct tm tm;
      if (millis() - alChk > 1000 && (alChk = millis(), getLocalTime(&tm, 0)) && tm.tm_hour == alarmH &&
          tm.tm_min == alarmM && !(alarmRep == 2 && (tm.tm_wday == 0 || tm.tm_wday == 6))) {
        stopped = true;
        break;
      }
    }  // stik kanan kiri/kanan: ganti saluran (diproses loop)
    if (volAdjActive) replyVolume = VOL_GAIN[constrain((int)volAdjLevel, 1, 10)];  // L2/R2/stik: langsung terdengar
    if (music) {  // tombol kiri/kanan saat musik = saluran / lagu sebelumnya & berikutnya (ditahan 40 ms = sah)
      int8_t z = gbRead(GBTN_L) == LOW ? -1 : (gbRead(GBTN_R) == LOW ? 1 : 0);
      if (z && z == zapHeld) {
        if (millis() - zapSince > 40) { zapReq = z; stopped = true; break; }
      } else {
        zapHeld = z;
        zapSince = millis();
      }
    }

    size_t avail = (wr - rd) & ~(size_t)1;
    if (!started) {
      if (avail >= prefill || netDone) started = true;
      else { delay(1); continue; }
    }
    if (avail == 0) {
      if (netDone) break;
      delay(1);
      continue;
    }

    // 2. Putar satu blok
    size_t bytes = min(avail, (size_t)BLOCK * 2);
    for (size_t j = 0; j < bytes / 2; j++) {
      size_t pos = (rd + j * 2) % cap;
      buf[j] = (int16_t)(reply[pos] | (reply[pos + 1] << 8));
    }
    rd += bytes;
    playScaled(buf, bytes / 2);
  }
  audioLevel = 0;
  playSilence(100);
  wifiFast();
  if (played) *played = rd;
  return stopped;
}

#include "sdmusik.h"   // musik dari kartu microSD (MP3), tanpa internet

// Ambil angka / teks dari JSON sederhana tanpa library, mis. {"ok":true,"vol":5,"tampilan":"TEKS"}
int jsonInt(const String &js, const char *key, int def) {
  int i = js.indexOf(String("\"") + key + "\":");
  return i < 0 ? def : js.substring(i + strlen(key) + 3).toInt();
}

String jsonStr(const String &js, const char *key) {
  int i = js.indexOf(String("\"") + key + "\":\"");
  if (i < 0) return "";
  int a = i + strlen(key) + 4, b = js.indexOf('"', a);
  return b < 0 ? "" : js.substring(a, b);
}

void setVolume(int level);

// Cek server sekaligus melapor status robot. Server membalas perintah tertunda dari halaman
// /pengaturan (misalnya volume atau mode tampilan) dan robot langsung menerapkannya.
extern uint32_t prevStage;
bool checkServer() {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  http.setConnectTimeout(1500);
  http.setTimeout(1500);
  String q = "/health?robot=1&vol=" + String(volLevel) + "&tampilan=" + viewName(viewMode) +
             "&fw=" FW_VERSION "&bat=" + String(batPct) +
             "&tema=" + (lightMode ? "TERANG" : "GELAP") + "&efek=" + (fxOn ? "1" : "0") +
             "&batshow=" + (batShow ? "1" : "0") + "&joget=" + (jogetAuto ? "1" : "0") +
             "&bt=" + (millis() - btSeen < 3000 ? (btConn ? "2" : "1") : "0") + "&btm=" + String(btMode) +
             "&bicara=" + String(talkMode) +
             "&pad=" + (PAD_DIRECT ? (padGet().conn ? "2" : "1") :
                        (bridgeNew() ? (bridgeSlot == 1 ? (padGet().conn ? "2" : "1") : "3") : "0")) +
             "";
  if (!isnan(roomT)) q += "&suhu=" + String(roomT, 1) + "&lembap=" + String((int)roomH);
  q += "&alarm=" + String(alarmOn ? 1 : 0) + "," + String(alarmH) + "," + String(alarmM) + "," + String(alarmRep);
  q += "&rst=" + String((int)esp_reset_reason()) + "&tahap=" + String(prevStage) + "&up=" + String(millis() / 1000);
  if (!http.begin(serverUrl() + q)) return false;
  http.addHeader("Authorization", "Bearer " + tokenCfg);
  int code = http.GET();
  String js = code == 200 ? http.getString() : "";
  http.end();
  if (code != 200) return false;
  int v = jsonInt(js, "vol", -1);
  if (v > 0) setVolume(v);
  String t = jsonStr(js, "tampilan");
  if (t.length() && viewFromName(t) != viewMode) setView(viewFromName(t));
  String srvS = jsonStr(js, "srv");  // pengaturan milik server untuk menu OLED
  if (srvS.length()) {
    int16_t tmp[12];
    int n = 0, from = 0;
    while (n < 12) {
      int c = srvS.indexOf(',', from);
      tmp[n++] = (c < 0 ? srvS.substring(from) : srvS.substring(from, c)).toInt();
      if (c < 0) break;
      from = c + 1;
    }
    if (n == 12) {
      for (int i = 0; i < 12; i++) srvVal[i] = tmp[i];
      srvKnown = true;
    }
  }
  String sy = jsonStr(js, "senyap");  // "22-5" atau "off"
  if (sy.length()) {
    bool on = sy != "off";
    int a = on ? sy.toInt() : quietStart, b = on ? sy.substring(sy.indexOf('-') + 1).toInt() : quietEnd;
    if (on != quietOn || a != quietStart || b != quietEnd) {
      quietOn = on; quietStart = constrain(a, 0, 23); quietEnd = constrain(b, 0, 23);
      prefs.begin("robot", false);
      prefs.putBool("qon", quietOn); prefs.putInt("qs", quietStart); prefs.putInt("qe", quietEnd);
      prefs.end();
    }
  }
  String rm = jsonStr(js, "pengingat");
  if (rm.length()) pendingReminder = urlDecode(rm);
  serverOld = js.indexOf("\"nama\"") < 0;  // server lama tidak mengenal mode terang, efek, game
  String tema = jsonStr(js, "tema");
  if (tema.length()) setTheme(tema == "TERANG");
  int fx = jsonInt(js, "efek", -1);
  if (fx >= 0) setFx(fx == 1);
  setRobotName(jsonStr(js, "nama"));
  String pm = jsonStr(js, "pesan");
  if (pm.length()) pendingMsg = urlDecode(pm);
  String nf = jsonStr(js, "notif");  // notifikasi HP: "app|dari|teks~app|dari|teks" (tiap bagian url-encode)
  if (nf.length() && infoOk) {
    int from = 0;
    while (from < (int)nf.length()) {
      int e = nf.indexOf('~', from);
      String it = nf.substring(from, e < 0 ? nf.length() : e);
      from = e < 0 ? nf.length() : e + 1;
      int p1 = it.indexOf('|'), p2 = it.indexOf('|', p1 + 1);
      if (p1 >= 0 && p2 > p1)
        infoNotifPush(urlDecode(it.substring(0, p1)).c_str(), urlDecode(it.substring(p1 + 1, p2)).c_str(),
                      urlDecode(it.substring(p2 + 1)).c_str());
    }
  }
  int bm = jsonInt(js, "btmode", -1);
  if (bm >= 0 && bm != btMode) setBtMode(bm);
  int bc = jsonInt(js, "bicara", -1);
  if (bc >= 0 && bc != talkMode) setTalkMode(bc, true);
  int jg = jsonInt(js, "joget", -1);
  if (jg >= 0 && (jg == 1) != jogetAuto) {
    jogetAuto = jg == 1;
    prefs.begin("robot", false);
    prefs.putBool("joget", jogetAuto);
    prefs.end();
  }
  int bs = jsonInt(js, "baterai_tampil", -1);
  if (bs >= 0 && (bs == 1) != batShow) {
    batShow = bs == 1;
    batStable = 0;
    if (!batShow) batPct = -1;
    prefs.begin("robot", false);
    prefs.putBool("batshow", batShow);
    prefs.end();
  }
  return true;
}

// Minta server mengucapkan teks lalu putar langsung. false kalau server tidak bisa.
bool speakOnline(const String &teks, bool *stopped = nullptr) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(20000);
  http.useHTTP10(true);
  if (!http.begin(serverUrl() + "/ucap?teks=" + urlEncode(teks))) return false;
  http.addHeader("Authorization", "Bearer " + tokenCfg);
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  bool st = streamPlay(http);
  http.end();
  if (stopped) *stopped = st;
  return true;
}

// ---------------- Suara cadangan (tersimpan di flash, dipakai saat server mati) ----------------
const Phrase PHRASES[] = {
  {"offline", "Maaf, aku sedang tidak tersambung ke server. Coba lagi sebentar ya."},
  {"wifi", "Sambungan WiFi aku terputus. Tunggu sebentar ya."},
  {"timer", "Waktunya sudah habis!"},
  {"baterai", "Baterai aku sudah lemah. Tolong isi daya ya."},
  {"halo", "Halo, aku sudah siap ngobrol!"},
  {"alarm", "Waktunya bangun! Ayo bangun, sudah pagi."},
  {"demo1", "Halo semuanya! Aku Almara, robot teman ngobrol."},
  {"demo2", "Aku bisa berekspresi, bergerak, dan berjoget."},
  {"demo3", "Ayo ngobrol denganku! Tekan tombolnya dan bicara saja."},
};

String phrasePath(const char *key) { return String("/frasa_") + key + ".pcm"; }

bool downloadPhrase(const Phrase &p) {
  HTTPClient http;
  http.setConnectTimeout(3000);
  http.setTimeout(20000);
  http.useHTTP10(true);
  if (!http.begin(serverUrl() + "/ucap?teks=" + urlEncode(p.text))) return false;
  http.addHeader("Authorization", "Bearer " + tokenCfg);
  if (http.GET() != 200) {
    http.end();
    return false;
  }
  String tmp = phrasePath(p.key) + ".tmp";
  File f = FFat.open(tmp, FILE_WRITE);
  if (!f) {
    http.end();
    return false;
  }
  int written = http.writeToStream(&f);
  f.close();
  http.end();
  if (written <= 0) {
    FFat.remove(tmp);
    return false;
  }
  FFat.remove(phrasePath(p.key));
  return FFat.rename(tmp, phrasePath(p.key));
}

void cachePhrases() {
  if (!fsOk || !serverOk) return;
  int missing = 0;
  for (const Phrase &p : PHRASES) missing += !FFat.exists(phrasePath(p.key));
  if (!missing) return;
  screen("Menyiapkan suara", "Menyimpan suara cadangan supaya robot tetap bisa bicara saat server mati...");
  for (const Phrase &p : PHRASES)
    if (!FFat.exists(phrasePath(p.key))) downloadPhrase(p);
}

// Putar suara cadangan. Kalau belum tersimpan, cukup bunyi beep.
bool playPhrase(const char *key) {
  File f = fsOk ? FFat.open(phrasePath(key), FILE_READ) : File();
  if (!f) {
    errBeep();
    return false;
  }
  int16_t buf[BLOCK];
  while (true) {
    if (talkPressed()) break;
    int r = f.read((uint8_t *)buf, sizeof(buf));
    if (r < 2) break;
    playScaled(buf, r / 2);
  }
  f.close();
  audioLevel = 0;
  playSilence(80);
  return true;
}

// ---------------- Timer & pengingat ----------------
void addTimer(uint32_t sec, const String &label) {
  for (Timer &t : timers) {
    if (!t.on) {
      t.on = true;
      t.due = millis() + sec * 1000UL;
      strlcpy(t.label, label.c_str(), sizeof(t.label));
      return;
    }
  }
  // penuh: timpa yang paling cepat selesai
  timers[0].due = millis() + sec * 1000UL;
  strlcpy(timers[0].label, label.c_str(), sizeof(timers[0].label));
}

void cancelTimers() {
  for (Timer &t : timers) t.on = false;
}

String nextTimerText() {
  long best = -1;
  for (Timer &t : timers) {
    if (!t.on) continue;
    long left = (long)(t.due - millis());
    if (left < 0) left = 0;
    if (best < 0 || left < best) best = left;
  }
  if (best < 0) return "";
  long s = (best + 999) / 1000;
  char buf[10];
  if (s >= 3600) snprintf(buf, sizeof(buf), "%ld:%02ld", s / 3600, (s / 60) % 60);
  else snprintf(buf, sizeof(buf), "%ld:%02ld", s / 60, s % 60);
  return buf;
}

void ringTimer(Timer &t) {
  if (gameActive) { gameReq = 2; delay(60); }  // alarm lebih penting dari game
  t.on = false;
  String label = t.label;
  String shown = label.length() ? "Waktunya: " + label : "Waktunya sudah habis!";
  setText("TIMER SELESAI", shown);
  setExpr("kaget", "diam");
  setAct(ACT_TALK);
  // Berbunyi sampai tombol ditekan, maksimal 3 kali
  for (int round = 0; round < 3; round++) {
    faceEvent(BH_SHAKE);
    for (int i = 0; i < 3 && !talkPressed(); i++) { playTone(1500, 120); playSilence(80); }
    if (talkPressed()) break;
    bool stopped = false;
    String say = label.length() ? "Waktunya sudah habis. Saatnya " + label + "." : "Waktunya sudah habis!";
    if (!speakOnline(say, &stopped)) playPhrase("timer");
    if (stopped || talkPressed()) break;
    for (int i = 0; i < 40 && !talkPressed(); i++) delay(50);
    if (talkPressed()) break;
  }
  waitRelease();
  lastText = shown;
  showIdle();
}

// ---------------- Alarm harian ----------------
void saveAlarm() {
  prefs.begin("robot", false);
  prefs.putBool("alon", alarmOn);
  prefs.putChar("alh", alarmH);
  prefs.putChar("alm", alarmM);
  prefs.putChar("alr", alarmRep);
  prefs.putChar("alt", alarmTone);
  prefs.end();
}

String alarmText() {
  char b[24];
  static const char *const R[3] = {"sekali", "setiap hari", "Senin-Jumat"};
  snprintf(b, sizeof(b), "%02d.%02d %s", alarmH, alarmM, R[constrain((int)alarmRep, 0, 2)]);
  return b;
}

// Satu putaran suara alarm. stop() dicek di sela nada; kembalikan false kalau dihentikan.
// round = putaran ke berapa (makin lama makin panjang / ramai).
template <typename F>
bool alarmSound(int kind, int round, F stop) {
  auto T = [&](float f, int ms) -> bool {
    if (stop()) return false;
    if (f > 0) playTone(f, ms); else playSilence(ms);
    return true;
  };
  auto sweep = [&](float f0, float f1, int ms, int steps) -> bool {
    for (int i = 0; i < steps; i++)
      if (!T(f0 + (f1 - f0) * i / (steps - 1), ms / steps)) return false;
    return true;
  };
  int rep = min(1 + round, 4);
  for (int r = 0; r < rep; r++) {
    switch (kind) {
      case 0:  // Bip
        for (int i = 0; i < 4; i++) if (!T(i % 2 ? 1760 : 1500, 110) || !T(0, 70)) return false;
        break;
      case 1:  // Weker: dering cepat seperti jam weker
        for (int i = 0; i < 16; i++) if (!T(i % 2 ? 2300 : 2000, 35)) return false;
        if (!T(0, 200)) return false;
        break;
      case 2:  // Lonceng: dong-ding turun
        for (float f : {1568.f, 1319.f, 1047.f, 784.f}) if (!T(f, 260) || !T(0, 60)) return false;
        break;
      case 3:  // Melodi: do mi sol do naik
        for (float f : {523.f, 659.f, 784.f, 1047.f, 784.f, 1047.f}) if (!T(f, 150) || !T(0, 30)) return false;
        break;
      case 4:  // Sirene: naik turun
        if (!sweep(700, 1600, 500, 25) || !sweep(1600, 700, 500, 25)) return false;
        break;
      case 5:  // Burung: cicit pendek
        for (int i = 0; i < 3; i++) if (!sweep(2600, 3600, 90, 6) || !sweep(3400, 2800, 70, 5) || !T(0, 90)) return false;
        if (!T(0, 200)) return false;
        break;
      case 6:  // Ayam: kukuruyuk
        if (!T(600, 120) || !T(0, 40) || !T(700, 120) || !T(0, 40) || !sweep(700, 1000, 300, 12) ||
            !sweep(1000, 650, 450, 15) || !T(0, 250)) return false;
        break;
      case 7:  // Piano: arpeggio lembut
        for (float f : {523.f, 659.f, 784.f, 988.f, 1175.f, 988.f, 784.f, 659.f}) if (!T(f, 120)) return false;
        if (!T(0, 120)) return false;
        break;
      case 8:  // Arcade: koin & naik level
        if (!T(988, 70) || !T(1319, 220) || !T(0, 80)) return false;
        for (float f : {784.f, 1047.f, 1319.f, 1568.f, 2093.f}) if (!T(f, 70)) return false;
        if (!T(0, 150)) return false;
        break;
      case 9:  // Lembut: dua nada pelan, cocok kalau mudah kaget
        if (!T(659, 400) || !T(0, 150) || !T(523, 500) || !T(0, 400)) return false;
        break;
      case 10:  // Kereta: ting-tong stasiun
        for (float f : {659.f, 523.f, 587.f, 392.f}) if (!T(f, 300) || !T(0, 40)) return false;
        if (!T(0, 200)) return false;
        break;
      default:  // Ceria: lagu pendek
        for (float f : {784.f, 784.f, 880.f, 784.f, 1047.f, 988.f}) if (!T(f, f == 988.f ? 320 : 160) || !T(0, 40)) return false;
        break;
    }
  }
  return true;
}

// Berbunyi makin keras sampai dimatikan (maks 5 menit). Tombol bicara / A = matikan, B = tunda 5 menit.
// Kembalikan true kalau ditunda.
bool ringAlarm() {
  if (gameActive) { gameReq = 2; delay(60); }
  if (setActive) setActive = false;
  alarmRinging = true;
  faceEvent(BH_CONFETTI);
  padStop = false;
  padAEdge = false;
  char hm[8];
  snprintf(hm, sizeof(hm), "%02d.%02d", alarmH, alarmM);
  setText(String("ALARM ") + hm, "Tombol / A = matikan\nB = tunda 5 menit");
  setExpr("kaget", "diam");
  setAct(ACT_TALK);
  bool snooze = false, done = false;
  uint32_t t0 = millis();
  auto pressed = [&]() -> bool {
    if (talkPressed() || padAEdge) { padAEdge = false; done = true; return true; }
    if (padStop) { padStop = false; snooze = true; return true; }
    return false;
  };
  for (int round = 0; !done && !snooze && millis() - t0 < 300000; round++) {
    faceEvent(BH_SHAKE);
    alarmSound(alarmTone, round, pressed);
    if (done || snooze) break;
    if (round % 3 == 1) {  // sesekali bicara
      bool stopped = false;
      struct tm tm;
      String say = "Waktunya bangun! Sekarang jam " + String(alarmH) + (alarmM ? " lewat " + String(alarmM) + " menit." : ".");
      if (getLocalTime(&tm, 0) && tm.tm_hour < 11) say = "Selamat pagi! " + say;
      if (!speakOnline(say, &stopped)) playPhrase("alarm");
      if (stopped) { done = true; break; }
    }
    for (int i = 0; i < 30 && !pressed(); i++) delay(50);
  }
  alarmRinging = false;
  waitRelease();
  if (snooze) {
    setText("Alarm ditunda", "Bunyi lagi 5 menit lagi");
    playTone(880, 80); playSilence(40); playTone(660, 120);
  } else {
    setText("Alarm mati", done ? "Selamat beraktivitas!" : "Alarm berhenti sendiri");
    setExpr("senang", "");
    playTone(1046, 60); playSilence(30); playTone(1568, 100);
  }
  delay(1500);
  lastText = snooze ? "Alarm ditunda 5 menit." : "Alarm " + String(hm) + " selesai.";
  showIdle();
  return snooze;
}

void checkAlarm() {
  static uint32_t lastCheck = 0, snoozeAt = 0;
  static int lastKey = -1;
  uint32_t now = millis();
  if (snoozeAt && (int32_t)(now - snoozeAt) >= 0) {
    snoozeAt = 0;
    if (ringAlarm()) snoozeAt = millis() + 300000;
    return;
  }
  if (!alarmOn || now - lastCheck < 1000) return;
  lastCheck = now;
  struct tm tm;
  if (!getLocalTime(&tm, 0)) return;  // jam belum tersinkron
  int key = tm.tm_yday * 1440 + tm.tm_hour * 60 + tm.tm_min;
  if (tm.tm_hour != alarmH || tm.tm_min != alarmM || key == lastKey) return;
  if (alarmRep == 2 && (tm.tm_wday == 0 || tm.tm_wday == 6)) return;  // Sabtu & Minggu libur
  lastKey = key;
  if (alarmRep == 0) { alarmOn = false; saveAlarm(); }
  if (ringAlarm()) snoozeAt = millis() + 300000;
}

void checkTimers() {
  for (Timer &t : timers)
    if (t.on && (long)(millis() - t.due) >= 0) ringTimer(t);
}

// ---------------- Baterai ----------------
// Pakai USB tanpa baterai: GPIO 1 tidak tersambung dan membaca nilai acak. Baterai asli memberi
// bacaan stabil di rentang 1 sel Li-ion (2.8-4.5V), jadi indikator baru muncul setelah 3 bacaan stabil
// berturut-turut (30 detik). Bisa juga disembunyikan permanen dari halaman /pengaturan.
void readBattery() {
  if (!batShow) {
    batPct = -1;
    return;
  }
  float mn = 99, mx = 0, sum = 0;
  for (int i = 0; i < 8; i++) {
    float vi = analogReadMilliVolts(BAT_PIN) / 1000.0f * BAT_RATIO;
    mn = fminf(mn, vi);
    mx = fmaxf(mx, vi);
    sum += vi;
    delay(2);
  }
  float v = sum / 8;
  if (v < 2.8f || v > 4.5f || mx - mn > 0.15f) {  // tidak terpasang / pin mengambang
    batStable = 0;
    batPct = -1;
    batSmooth = 0;
    return;
  }
  if (++batStable < 3) return;
  batSmooth = batSmooth > 0 ? batSmooth * 0.7f + v * 0.3f : v;  // haluskan supaya persen tidak melompat
  int pct = batPercentFromVolt(batSmooth);
  if (batPct >= 0 && pct > batPct + 1) pct = batPct + 1;       // tidak naik tiba-tiba (kecuali sedang diisi)
  {  // persen naik = sedang diisi: tampilkan petir (paling sering sekali per 2 menit)
    static int prevP = -1; static uint32_t boltAt = 0;
    if (prevP >= 0 && pct > prevP && millis() - boltAt > 120000UL) { boltAt = millis(); react(BH_BOLT); }
    prevP = pct;
  }
  batPct = pct;
  if (batPct < 20) batLowCount++; else batLowCount = 0;
  static uint8_t critCount = 0;  // kunci roda hanya setelah 3 bacaan diam berturut-turut di bawah 5%
  critCount = batPct < 5 ? critCount + 1 : 0;
  if (critCount >= 3) batCritical = true;
  if (batPct >= 12) batCritical = false;  // histeresis: lepas kunci setelah diisi
  if (batLowCount >= 3 && !batWarned) {  // peringatan hanya setelah 3 bacaan rendah berturut-turut
    batWarned = true;
    setExpr("sedih", "diam");
    setText("Baterai lemah", "Baterai " + String(batPct) + "%. Tolong isi daya ya.");
    setAct(ACT_TALK);
    if (!speakOnline("Baterai aku tinggal " + String(batPct) + " persen. Tolong isi daya ya.")) playPhrase("baterai");
    showIdle();
  }
  if (batPct > 30) batWarned = false;
}

// ---------------- Sensor suhu ruangan ----------------
void readRoom() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h) && t > -20 && t < 70) {  // abaikan pembacaan gagal
    roomT = t;
    roomH = h;
  }
}

String roomText() {
  if (isnan(roomT)) return "";
  return String(roomT, 1) + "C " + String((int)roomH) + "%";
}

// Kalau ruangan panas, robot sesekali (maks. tiap 2 jam) mengingatkan sendiri.
void hotReminder() {
  if (!hotRemindOn || isnan(roomT) || roomT < HOT_C || !serverOk || quietNow) return;  // jam tenang: tidak menyapa duluan
  if (lastHotRemind && millis() - lastHotRemind < HOT_REMIND_MS) return;
  if (millis() - lastActivity < 20000) return;  // jangan memotong obrolan
  lastHotRemind = millis() | 1;
  String say = "Ruangan sekarang " + String((int)roundf(roomT)) +
               " derajat, cukup gerah ya. Jangan lupa minum air putih.";
  setExpr("sedih", "diam");
  setText("Ruangan panas", say);
  setAct(ACT_TALK);
  speakOnline(say);
  lastText = say;
  showIdle();
}

// ---------------- Volume ----------------
void setVolume(int level) {
  volLevel = constrain(level, 1, 10);
  replyVolume = VOL_GAIN[constrain(volLevel, 1, 10)];
  prefs.begin("robot", false);
  prefs.putInt("vol", volLevel);
  prefs.end();
}

// ---------------- Update firmware online dari server HP ----------------
// Dibuat tahan WiFi tidak stabil:
//  - unduhan dipotong-potong dan DILANJUTKAN (HTTP Range) dari byte terakhir kalau sambungan putus / macet,
//    jadi tidak mengulang dari 0 dan tidak gagal hanya karena satu kali putus;
//  - WiFi disambungkan ulang otomatis di tengah jalan;
//  - Bluetooth gamepad dimatikan selama update (WiFi & Bluetooth berbagi satu radio: ini penyebab utama lambat/putus);
//  - baterai lemah ditolak, karena lonjakan arus WiFi + flash saat baterai drop bisa mereset ESP32 di tengah update.
// Mengembalikan: 1 = terpasang (robot restart), 0 = sudah terbaru / tidak ada file, -1 = gagal.
// install = false: hanya cek ketersediaan (dipakai saat menyala).
#define OTA_STALL_MS 8000        // tidak ada data selama ini = sambungan dianggap macet, sambung ulang
#define OTA_MAX_FAILS 40         // gagal berturut-turut TANPA kemajuan sama sekali sebelum ganti putaran
#define OTA_MAX_MS (20UL * 60 * 1000)  // batas waktu tiap putaran
#define OTA_ROUNDS 3             // putaran penuh (WiFi disambung ulang total di antaranya)
#define OTA_MIN_BAT 8            // di bawah ini: tunggu dicas dulu (maks 90 dtk), lalu tetap lanjut

void runUpdateMode();            // (dibawah) hotspot Almara-Update berisi halaman upload firmware
volatile bool fwNoFile = false;  // server belum punya firmware.bin

static bool otaWifiUp(bool hard = false) {
  if (!hard && WiFi.status() == WL_CONNECTED) return true;
  if (hard) { WiFi.disconnect(false, false); delay(400); WiFi.begin(); }   // pakai WiFi tersimpan
  else WiFi.reconnect();
  for (int i = 0; i < 80 && WiFi.status() != WL_CONNECTED; i++) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

static void otaPrepHttp(HTTPClient &http, const String &md5) {
  http.setConnectTimeout(5000);
  http.setTimeout(OTA_STALL_MS);
  http.setReuse(false);
  http.useHTTP10(true);
  http.addHeader("Authorization", "Bearer " + tokenCfg);
  if (md5.length()) http.addHeader("X-Want-MD5", md5);  // server menolak (409) kalau file firmware berganti di tengah unduhan
}

static void otaEnd(bool ok) {  // kembalikan keadaan normal
  (void)ok;
  otaBusy = false;
  wifiFast();
}

// Jalan terakhir yang selalu berhasil: robot membuka hotspot Almara-Update, firmware diunggah dari HP.
[[noreturn]] static void otaViaHotspot(const String &alasan) {
  otaEnd(false);
  screen("UPDATE FIRMWARE", alasan + "\n\nAku lanjutkan lewat hotspot\nAlmara-Update...");
  okBeep();
  delay(2500);
  runUpdateMode();  // tidak kembali
  for (;;) delay(1000);
}

// Tanya server (dengan beberapa kali coba). Mengembalikan kode HTTP, atau -1 kalau tidak terjangkau.
static int otaAsk(String &md5, int &total) {
  int code = -1;
  for (int i = 0; i < 5; i++) {
    if (!otaWifiUp(i >= 3)) { delay(500); continue; }
    HTTPClient http;
    http.setConnectTimeout(3500);
    http.setTimeout(10000);
    http.useHTTP10(true);
    if (!http.begin(serverUrl() + "/firmware?cek=1")) { delay(500); continue; }
    http.addHeader("Authorization", "Bearer " + tokenCfg);
    http.addHeader("X-FW-MD5", fwMd5);
    const char *keys[] = {"X-MD5", "X-Size"};
    http.collectHeaders(keys, 2);
    code = http.GET();
    if (code == 200) { md5 = http.header("X-MD5"); total = http.header("X-Size").toInt(); }
    http.end();
    if (code > 0) return code;
    delay(800);
  }
  return -1;
}

// Mengembalikan: 1 = terpasang (robot restart), 0 = sudah terbaru / belum ada file, -1 = server tak terjangkau (hanya saat cek awal).
// install = false: hanya cek ketersediaan (dipakai saat menyala).
int otaFromServer(bool install) {
  if (!install && WiFi.status() != WL_CONNECTED) return -1;

  // 1) Tanya server: ada firmware baru? ukuran & MD5-nya?
  String md5;
  int total = 0;
  fwNoFile = false;
  int code = install ? otaAsk(md5, total) : -1;
  if (!install) {
    HTTPClient http;
    http.setConnectTimeout(3000);
    http.setTimeout(10000);
    http.useHTTP10(true);
    if (!http.begin(serverUrl() + "/firmware?cek=1")) return -1;
    http.addHeader("Authorization", "Bearer " + tokenCfg);
    http.addHeader("X-FW-MD5", fwMd5);
    const char *keys[] = {"X-MD5", "X-Size"};
    http.collectHeaders(keys, 2);
    code = http.GET();
    if (code == 200) { md5 = http.header("X-MD5"); total = http.header("X-Size").toInt(); }
    http.end();
  }
  if (code == 204) { fwAvailable = false; return 0; }
  if (code == 404) { fwNoFile = true; fwAvailable = false; return 0; }
  if (code != 200) {
    if (install) { otaBusy = true; otaViaHotspot(code > 0 ? "Server belum siap (" + String(code) + ")." : "Server belum terjangkau."); }
    return -1;
  }
  fwAvailable = true;
  if (!install) return 0;
  otaBusy = true;
  if (md5.length() != 32 || total <= 0) otaViaHotspot("File dari server belum dikenali.");

  // Baterai sangat rendah: beri waktu untuk colok cas, lalu tetap lanjut (firmware lama aman kalau tiba-tiba mati).
  if (batPct >= 0 && batPct < OTA_MIN_BAT) {
    errBeep();
    uint32_t w0 = millis();
    while (batPct >= 0 && batPct < OTA_MIN_BAT && millis() - w0 < 90000) {
      screen("UPDATE FIRMWARE", "Baterai " + String(batPct) + "%.\nColok charger ya...\n" + String((90000 - (millis() - w0)) / 1000) + " dtk");
      delay(1000);
    }
  }

  // 2) Siapkan: roda diam, gamepad Bluetooth putus, WiFi tanpa hemat daya
  screen("UPDATE FIRMWARE", "Menyiapkan...\n\nJangan cabut daya!");
  delay(600);  // beri waktu task gerak memutus gamepad
  wifiFast();

  static uint8_t buf[4096];
  for (int round = 1; round <= OTA_ROUNDS; round++) {
    if (!Update.begin(total)) {
      Update.abort();
      if (round < OTA_ROUNDS) { delay(500); continue; }
      otaViaHotspot("Ruang update belum siap.");
    }
    Update.setMD5(md5.c_str());  // file rusak di jalan = ditolak

    // 3) Unduh dengan resume
    size_t done = 0;
    int fails = 0, attempt = 0, lastPct = -1;
    bool writeErr = false, changed = false;
    uint32_t t0 = millis();
    String why = "";

    while (done < (size_t)total && fails < OTA_MAX_FAILS && !writeErr && !changed && millis() - t0 < OTA_MAX_MS) {
      attempt++;
      size_t before = done;
      if (!otaWifiUp(fails >= 3)) { fails++; why = "WiFi putus"; continue; }

      HTTPClient http;
      otaPrepHttp(http, md5);
      if (!http.begin(serverUrl() + "/firmware")) { fails++; why = "alamat server"; delay(500); continue; }
      if (done) http.addHeader("Range", "bytes=" + String((unsigned)done) + "-");
      int c2 = http.GET();
      int want = total - (int)done;
      if (c2 == 409) { http.end(); changed = true; why = "firmware di server berganti"; break; }
      if (!((c2 == 200 && done == 0) || c2 == 206) || http.getSize() != want) {
        Serial.printf("OTA: percobaan %d kode %d ukuran %d (perlu %d)\n", attempt, c2, http.getSize(), want);
        http.end();
        fails++;
        why = c2 > 0 ? "server balas " + String(c2) : "server tidak terjangkau";
        delay(1000);
        continue;
      }

      WiFiClient *st = http.getStreamPtr();
      uint32_t lastData = millis();
      while (done < (size_t)total) {
        int av = st->available();
        if (av <= 0) {
          if (!st->connected() || millis() - lastData > OTA_STALL_MS) { why = "sambungan macet"; break; }
          delay(1);
          continue;
        }
        size_t n = min((size_t)av, min(sizeof(buf), (size_t)total - done));
        int r = st->readBytes(buf, n);
        if (r <= 0) continue;
        if (Update.write(buf, r) != (size_t)r) { writeErr = true; break; }
        done += r;
        lastData = millis();
        int pct = (int)((uint64_t)done * 100 / total);
        if (pct != lastPct) {
          lastPct = pct;
          if (pct % 2 == 0)
            screen("UPDATE FIRMWARE", "Memasang firmware baru... " + String(pct) + "%" +
                   (attempt > 1 ? "\nSambung ulang ke-" + String(attempt - 1) : String("")) + "\n\nJangan cabut daya!");
        }
      }
      http.end();
      if (done > before) fails = 0;       // ada kemajuan: hitungan mulai dari nol
      else if (done < (size_t)total) fails++;
      if (done < (size_t)total && !writeErr) {
        Serial.printf("OTA: putus di %u/%d (%s), lanjut dari sana\n", (unsigned)done, total, why.c_str());
        delay(300);
      }
    }

    // 4) Selesaikan & periksa MD5
    if (done == (size_t)total && !writeErr && Update.end(true)) {
      screen("UPDATE BERHASIL", "Memulai ulang...");
      okBeep();
      delay(800);
      ESP.restart();
      return 1;
    }
    Serial.printf("OTA putaran %d belum selesai: %s\n", round, writeErr ? Update.errorString() : why.c_str());
    Update.abort();
    if (changed) {  // file berganti di server: ambil keterangan terbaru lalu ulangi dari awal
      int c3 = otaAsk(md5, total);
      if (c3 != 200 || md5.length() != 32 || total <= 0) otaViaHotspot("Server belum siap.");
    }
    if (round < OTA_ROUNDS) {
      screen("UPDATE FIRMWARE", "Mencoba lagi dari awal...\n(putaran " + String(round + 1) + " dari " + String(OTA_ROUNDS) + ")\n\nJangan cabut daya!");
      otaWifiUp(true);
      delay(500);
    }
  }
  otaViaHotspot("Sambungan ke server kurang stabil.");
}

// ---------------- Percakapan ----------------
void showNetworkStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    lastText = "WiFi belum tersambung.\nTahan tombol 10 dtk untuk setup WiFi.";
  } else {
    lastText = "WiFi: " + WiFi.SSID() + "\n" + serverUrl() + "\nServer: " +
               (serverOk ? "OK" : "TIDAK AKTIF\n(jalankan start.sh)") + "\nFirmware v" FW_VERSION +
               (isnan(roomT) ? "\nSensor suhu: belum terbaca" : "\nRuangan: " + roomText()) +
               (fwAvailable ? "\nAda firmware baru! Ucapkan \"perbarui firmware\"." : "");
  }
  setText("Status", lastText);
  setAct(ACT_INFO, 5000);
}

// Server tidak bisa dipakai: robot tetap hidup, menjawab dengan suara cadangan.
void offlineReply(const char *phraseKey, const String &msg) {
  serverOk = false;
  setStatus(nextTimerText().c_str());
  setExpr("sedih", "diam");
  setText("Tidak tersambung", msg);
  setAct(ACT_TALK);
  playPhrase(phraseKey);
  showError(msg);
  if (sdReady && !sdList.empty() && !gameActive) {  // tanpa internet: otomatis putar musik dari kartu SD
    delay(1500);
    toast(TI_OK, "Mode offline", "musik dari kartu SD", 2500);
    sdCommand("");
    showIdle();
  }
}

size_t buildTextBody(const String &teks, const char *boundary) {
  String all = String("--") + boundary + "\r\nContent-Disposition: form-data; name=\"teks\"\r\n\r\n" + teks +
               "\r\n--" + boundary + "--\r\n";
  memcpy(body, all.c_str(), all.length());
  return all.length();
}

// Kirim rekaman (n sampel) atau perintah teks (cmd) ke server lalu putar jawabannya.
void runUpdateMode();  // didefinisikan di bawah (dekat setup WiFi)

bool followUp = false;  // dengarkan lagi otomatis (jawaban untuk pertanyaan robot)

void askServer(size_t n, const char *cmd = nullptr) {
  if (WiFi.status() != WL_CONNECTED) {
    screen("WiFi putus", "Mencoba menyambung ulang...");
    WiFi.reconnect();
    for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) delay(200);
    if (WiFi.status() != WL_CONNECTED) {
      offlineReply("wifi", "WiFi tidak tersambung.\nTahan tombol 10 dtk untuk setup WiFi.");
      return;
    }
  }

  const char *boundary = "----robotngobrol7MA4YWxkTrZu0gW";
  size_t len = cmd ? buildTextBody(cmd, boundary) : buildBody(n, boundary);
  String url = serverUrl() + "/chat";
  setText(cmd && cmd[0] == '_' ? "Ganti saluran..." : "Berpikir...", "");
  setAct(ACT_THINK);

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(60000);  // lagu penuh / radio butuh waktu persiapan di HP
  if (!http.begin(url)) {
    errBeep();
    showError("Alamat server salah:\n" + url);
    return;
  }
  http.useHTTP10(true);  // tanpa "chunked": data audio bisa langsung diputar
  http.addHeader("Content-Type", String("multipart/form-data; boundary=") + boundary);
  http.addHeader("Authorization", "Bearer " + tokenCfg);
  http.addHeader("X-Robot", "1");
  if (!isnan(roomT))  // suhu & kelembapan ruangan untuk AI
    http.addHeader("X-Sensor", "suhu=" + String(roomT, 1) + ";lembap=" + String(roomH, 0));
  if (sdSession) http.addHeader("X-SD-Sesi", "1");  // ada lagu SD yang bisa dilanjutkan
  if (musicId)  // sampai mana musik terakhir terdengar, untuk "lanjutkan lagunya"
    http.addHeader("X-Posisi", String(musicId) + ":" + String(musicSec, 1));
  const char *keys[] = {"X-Teks", "X-Ucapan", "X-Ekspresi", "X-Gerak", "X-Tampilan", "X-Volume", "X-Timer",
                        "X-Firmware", "X-Musik", "X-Prefix", "X-Tema", "X-Efek", "X-Game", "X-Fokus", "X-BT",
                        "X-Kontrol", "X-Bicara", "X-Lanjut", "X-Menu", "X-Alarm",
                        "X-Judul", "X-Jenis", "X-Durasi", "X-Seek", "X-SD"};
  http.collectHeaders(keys, 27);

  unsigned long t0 = millis();
  int code = http.POST(body, len);
  if (code <= 0) {
    http.end();
    offlineReply("offline", "Server tidak bisa dihubungi:\n" + serverUrl() + "\nCek Termux & hotspot.");
    return;
  }
  serverOk = true;
  if (code != 200) {
    String msg = code == 401 ? "Token salah. Samakan ROBOT_TOKEN di start.sh dan di setup WiFi robot."
                             : "Error " + String(code) + ":\n" + http.getString().substring(0, 120);
    http.end();
    errBeep();
    showError(msg);
    return;
  }

  String teks = urlDecode(http.header("X-Teks"));
  String ucapan = urlDecode(http.header("X-Ucapan"));
  long audioBytes = http.getSize();
  float secs = (millis() - t0) / 1000.0f;
  Serial.printf("Terdengar: %s\nJawaban: %s\n%.1f dtk\n", teks.c_str(), ucapan.c_str(), secs);

  if (teks.length() == 0 || audioBytes == 0) {
    http.end();
    errBeep();
    setExpr("bingung", "diam");
    setText("Tidak terdengar", "Coba bicara lagi.");
    setAct(ACT_TALK);
    delay(1800);
    lastText = "Tidak terdengar. Coba bicara lagi.";
    showIdle();
    return;
  }

  // Perintah untuk robot dari server
  String tampilan = http.header("X-Tampilan");
  if (tampilan.length()) setView(viewFromName(tampilan));
  String tema = http.header("X-Tema");
  if (tema.length()) setTheme(tema == "TERANG");
  String btH = http.header("X-BT");
  if (btH.length()) setBtMode(btH.toInt());
  String efek = http.header("X-Efek");
  if (efek.length()) setFx(efek == "1");
  String fokus = http.header("X-Fokus");
  if (fokus == "stop") {
    stopFocus();
  } else if (fokus.indexOf(':') > 0) {
    focusWork = constrain(fokus.toInt(), 1, 120);
    focusBreak = constrain(fokus.substring(fokus.indexOf(':') + 1).toInt(), 1, 30);
    startFocus(false);
  }
  String vol = http.header("X-Volume");
  bool wantVolAdj = vol == "atur";
  if (wantVolAdj) vol = "";
  if (vol == "+") setVolume(volLevel + 2);
  else if (vol == "-") setVolume(volLevel - 2);
  else if (vol.length()) setVolume(vol.toInt());
  String tmr = http.header("X-Timer");
  if (tmr == "cancel") {
    cancelTimers();
  } else if (tmr.startsWith("set:")) {
    int c2 = tmr.indexOf(':', 4);
    uint32_t sec = tmr.substring(4, c2 < 0 ? tmr.length() : c2).toInt();
    String label = c2 < 0 ? "" : urlDecode(tmr.substring(c2 + 1));
    if (sec > 0) addTimer(sec, label);
  }
  String alm = http.header("X-Alarm");  // "set:jam:menit:ulang" atau "off"
  if (alm == "off") {
    alarmOn = false;
    saveAlarm();
  } else if (alm.startsWith("set:")) {
    int a = alm.indexOf(':', 4), b = alm.indexOf(':', a + 1);
    if (a > 0) {
      alarmH = constrain(alm.substring(4, a).toInt(), 0, 23);
      alarmM = constrain(alm.substring(a + 1, b < 0 ? alm.length() : b).toInt(), 0, 59);
      alarmRep = b < 0 ? 0 : constrain(alm.substring(b + 1).toInt(), 0, 2);
      alarmOn = true;
      saveAlarm();
    }
  }
  if (vol.length()) ucapan += "\n(Volume " + String(volLevel) + "/10)";
  String mus = http.header("X-Musik");
  long prefix = http.header("X-Prefix").toInt();
  String bic = http.header("X-Bicara");
  if (bic == "klik") setTalkMode(1, true);
  else if (bic == "tahan") setTalkMode(0, true);
  else if (bic == "ganti") setTalkMode(!talkMode, true);
  String ktr = http.header("X-Kontrol");
  if (ktr == "on") {
    if (PAD_DIRECT) toast(TI_PAD, "Mode kontrol", padGet().conn ? "aktif" : "Nyalakan pad", 2500, !padGet().conn);
    else if (!bridgeNew()) toast(TI_X, "Jembatan ESP32", "belum ada", 2500);
    requestSlot(1);
  } else if (ktr == "off" && !PAD_DIRECT) {
    if (!btMode) setBtMode(1);
    lastMusicEnd = millis();  // tetap di mode speaker 90 detik
    requestSlot(0);
  }
  if (mus.length() && btMode) requestSlot(0);  // lagu / radio: pindah ke speaker Bluetooth
  bool lanjut = http.header("X-Lanjut") == "1";

  lastText = "> " + teks + "\n" + ucapan;
  setText("Robot (" + String(secs, 1) + " dtk):", lastText);
  setExpr(http.header("X-Ekspresi"), http.header("X-Gerak"));
  setStatus(nextTimerText().c_str());
  setAct(ACT_TALK);
  bool wantFirmware = http.header("X-Firmware") == "cek";
  bool wantUpdateMode = http.header("X-Firmware") == "ap";
  size_t played = 0;
  if (mus.length()) {
    musicPlaying = true;
    // Panel musik "ala Spotify" (infoMusicStart) sengaja TIDAK dipakai lagi: dulu menimpa layar info
    // dengan piringan hitam + judul + bilah progres. Sekarang cukup efek di wajah (BH_DISCO/BH_EQ) saja,
    // layar info otomatis jatuh balik ke mode teks biasa (lihat infoTask() -> mode IM_MUSIC tidak pernah aktif).
    faceEvent((esp_random() & 1) ? BH_DISCO : BH_EQ);
  }
  String sdAct = urlDecode(http.header("X-SD"));
  bool stopped = streamPlay(http, &played, mus.length() > 0);
  http.end();
  if (mus.length()) { musicPlaying = false; lastMusicEnd = millis(); infoMusicEnd(); }

  if (sdAct.length() && !stopped) {  // perintah musik dari kartu SD
    setExpr("senang", "diam");
    sdCommand(sdAct == "mulai" ? String("") : sdAct);
    showIdle();
    return;
  }

  if (mus.length()) {  // catat posisi musik untuk "lanjutkan lagunya"
    musicId = mus.toInt();
    musicSec = played > (size_t)prefix ? (played - prefix) / 32000.0f : 0;
  }
  if (stopped) {
    if (zapReq) {  // tombol kiri/kanan: langsung pindah saluran / lagu (dikirim oleh loop)
      int8_t z = zapReq;
      zapReq = 0;
      zapFeedback(z);
      pendingCmd = z > 0 ? "__next" : "__prev";
      waitZapRelease();
      return;
    }
    waitRelease();  // jangan langsung mulai merekam
    if (mus.length()) {
      // Tekan dua kali cepat saat musik = lagu berikutnya
      uint32_t t = millis();
      while (millis() - t < 450 && !talkPressed()) delay(5);
      if (talkPressed()) {
        waitRelease();
        pendingCmd = "lagu berikutnya";
        return;
      }
      lastText = "Musik dijeda.\nBilang \"lanjutkan lagunya\" untuk melanjutkan.";
    } else {
      lastText = "Dihentikan.";
    }
  }
  if (wantVolAdj && !stopped) {  // tampilkan bilah volume di tepi kanan
    volAdjLevel = volLevel;
    volAdjDone = false;
    volAdjUntil = millis() + 10000;
    volAdjCloseAt = 0;
    volAdjMoveAt = 0;
    volAdjOpenAt = millis();
    volAdjActive = true;
  }
  if (wantUpdateMode) {
    if (stopped) {  // tombol ditekan saat Almara menjelaskan = batal
      lastText = "Mode update dibatalkan.";
    } else {
      runUpdateMode();  // tidak kembali: robot menyala ulang di akhir
    }
  }
  if (wantFirmware) {
    int r = otaFromServer(true);  // kalau berhasil, robot restart di dalam fungsi ini
    String say = r == 0 ? (fwNoFile ? "Belum ada file firmware titik bin di folder robot-server. Taruh dulu file barunya di sana, lalu minta aku memperbarui lagi."
                                    : "Firmware aku sudah yang terbaru, versi " FW_VERSION ".")
                        : "Pembaruan firmware belum selesai. Coba minta lagi sebentar lagi ya.";
    lastText = say;
    setText("Firmware v" FW_VERSION, say);
    setAct(ACT_TALK);
    if (!speakOnline(say)) errBeep();
  }
  showIdle();
  if (http.header("X-Game") == "menu" && !stopped) gameReq = 1;
  if (http.header("X-Menu") == "pengaturan" && !stopped) setOpenReq = true;
  // Robot bertanya & mode klik: langsung mendengarkan jawaban tanpa menekan tombol
  if (lanjut && talkMode == 1 && !stopped && !mus.length() && !wantVolAdj) followUp = true;
}

// ---------------- Joget otomatis saat mendengar musik di ruangan ----------------
// Musik = suara keras hampir terus-menerus (>80% waktu) DAN punya hentakan berirama (5-30 per 3 detik).
// Orang mengobrol banyak jedanya; bunyi sekali (pintu, tepuk) tidak berirama.
void stopDance() {
  if (!dancing) return;
  dancing = false;
  setExpr("normal", "diam");
  audioLevel = 0;
}

void danceMonitor() {
  static int32_t blk[BLOCK];
  static float noise = -60, smooth = -90;
  static uint32_t winStart = 0, lastOnset = 0;
  static int frames = 0, loud = 0, onsets = 0;
  size_t n = readMic24(blk);
  double mean = 0, sum = 0;
  for (size_t i = 0; i < n; i++) mean += blk[i];
  mean /= n ? n : 1;
  for (size_t i = 0; i < n; i++) { double d = blk[i] - mean; sum += d * d; }
  float db = n ? 20.0f * log10f(sqrtf(sum / n) / 8388608.0f + 1e-9f) : -120;
  if (db < noise) noise = noise * 0.7f + db * 0.3f;  // bising ruangan: turun cepat, naik sangat pelan
  else noise += 0.0005f * (db - noise);
  bool isLoud = db > noise + 10;
  uint32_t now = millis();
  if (isLoud && db > smooth + 6 && now - lastOnset > 120) { onsets++; lastOnset = now; }  // hentakan
  smooth = smooth * 0.8f + db * 0.2f;
  frames++;
  loud += isLoud;
  if (dancing) {
    audioLevel = constrain((db - noise - 5) / 30.0f, 0.0f, 1.0f);  // goyang mengikuti musik
    lastActivity = now;
  }
  if (!winStart) winStart = now;
  if (now - winStart < 3000) return;
  float ratio = frames ? (float)loud / frames : 0;
  bool music = ratio > 0.8f && onsets >= 5 && onsets <= 30;
  if (music && !dancing) {
    dancing = true;
    setExpr("senang", "joget");
  } else if (dancing && ratio < 0.5f) {
    stopDance();
  }
  winStart = now;
  frames = loud = onsets = 0;
}

// ---------------- Setup WiFi ----------------
void loadConfig() {
  prefs.begin("robot", true);
  serverCfg = prefs.getString("server", "auto");
  tokenCfg = prefs.getString("token", "");
  prefs.end();
  pServer.setValue(serverCfg.c_str(), 63);
  pToken.setValue("", 64);  // token tidak pernah ditampilkan di halaman setup yang terbuka
}

void saveConfig() {
  String s = pServer.getValue(), t = pToken.getValue();
  s.trim();
  t.trim();
  if (t.length() == 0) t = tokenCfg;  // kolom kosong = token lama tetap dipakai
  if (s.length() == 0) s = "auto";
  if (s == serverCfg && t == tokenCfg) return;
  serverCfg = s;
  tokenCfg = t;
  prefs.begin("robot", false);
  prefs.putString("server", serverCfg);
  prefs.putString("token", tokenCfg);
  prefs.end();
}

void onPortalStart(WiFiManager *) {
  screen("SETUP WIFI", "Sambungkan HP ke:\n" AP_NAME "\n(tanpa password)\nLalu buka:\n192.168.4.1\nTutup otomatis 3 mnt");
}

void onUpdateModeStart(WiFiManager *) {
  screen("MODE UPDATE FIRMWARE", String("1. HP ke WiFi:\n   " UPDATE_AP_NAME "\n") +
         "   (tanpa password)\n" +
         "2. Buka 192.168.4.1\n3. Update -> pilih firmware.bin\nTutup otomatis 5 mnt");
}

// "jalankan mode update firmware": putus dari WiFi, buka hotspot berisi halaman Update. Setelah update
// berhasil, robot menyala ulang sendiri dengan firmware baru. Kalau waktu habis, menyala ulang seperti biasa.
void runUpdateMode() {
  playTone(880, 80); playSilence(40); playTone(1320, 120);
  WiFi.disconnect(false, false);  // putus, tapi data WiFi tetap tersimpan
  delay(300);
  wm.setAPCallback(onUpdateModeStart);
  wm.setConfigPortalTimeout(UPDATE_MODE_SEC);
  wm.startConfigPortal(UPDATE_AP_NAME);  // tanpa password: langsung sambung
  saveConfig();  // kalau sempat mengubah server/token di halaman itu
  screen("Mode update selesai", "Menyambung ulang ke WiFi...");
  delay(1200);
  ESP.restart();
}

void runPortal() {
  wm.setAPCallback(onPortalStart);
  wm.setConfigPortalTimeout(180);
  wm.startConfigPortal(AP_NAME);
  saveConfig();
  wifiFast();
  serverOk = checkServer();
  cachePhrases();
  showNetworkStatus();
}

// ---- Bicara sendiri & ajak ngobrol (tanpa disentuh) ----
// Tiap ~35-75 detik saat robot diam, robot minta server (AI) membuat ucapan iseng / fakta / lelucon / semangat / ajakan
// ngobrol, lalu mengucapkannya. Kalau berupa pertanyaan, mic langsung dibuka supaya kamu bisa menjawab tanpa menyentuh apa pun.
#define SPONTAN_MIN_MS 35000UL
#define SPONTAN_RAND_MS 40000UL
void spontanTick() {
  static uint32_t nextAt = 0;
  uint32_t now = millis();
  if (!nextAt) nextAt = now + 40000 + esp_random() % 30000;   // setelah menyala: ~40-70 detik
  if (!spontanOn || (int32_t)(now - nextAt) < 0) return;
  bool busy = !serverOk || WiFi.status() != WL_CONNECTED || quietNow || gameActive || setActive || focusOn || musicPlaying ||
              sdActive || volAdjActive || listeningNow || alarmRinging || otaBusy || batCritical || followUp ||
              disp.act != ACT_IDLE || pendingMsg.length() || pendingReminder.length() || pendingCmd.length();
  if (busy || now - lastActivity < 25000) return;   // jangan memotong kegiatan / baru selesai ngobrol

  // pilih jenis ucapan (40% ajakan ngobrol)
  uint32_t r = esp_random() % 100;
  const char *jenis = r < 40 ? "ajak" : r < 60 ? "komentar" : r < 75 ? "fakta" : r < 85 ? "lelucon" : r < 95 ? "semangat" : "sensor";
  String url = serverUrl() + "/spontan?jenis=" + jenis;
  if (!isnan(roomT) && !isnan(roomH)) url += "&suhu=" + String(roomT, 1) + "&lembab=" + String((int)roomH);
  faceEvent(BH_CURIOUS);   // tanda "mau ngomong": mata melirik penasaran selama menunggu AI

  String body;
  {
    HTTPClient http;
    http.setConnectTimeout(2500);
    http.setTimeout(12000);   // AI butuh beberapa detik
    if (http.begin(url)) {
      http.addHeader("Authorization", "Bearer " + tokenCfg);
      if (http.GET() == 200) body = http.getString();
      http.end();
    }
  }
  nextAt = millis() + SPONTAN_MIN_MS + esp_random() % SPONTAN_RAND_MS;
  int a = body.indexOf('|'), b = a >= 0 ? body.indexOf('|', a + 1) : -1;
  if (a < 0 || b < 0) return;   // server tidak menjawab / jam tenang: coba lagi nanti
  String ex = body.substring(0, a), say = body.substring(b + 1);
  bool ajak = body.charAt(a + 1) == '1';
  say.trim();
  if (say.length() < 3) return;
  // pastikan tidak ada kegiatan baru selama menunggu
  if (disp.act != ACT_IDLE || gameActive || setActive || listeningNow || talkPressed()) return;

  stopDance();
  playTone(1568, 45); playSilence(30); playTone(2093, 70); playSilence(60);   // "ting-ting!" perhatian
  setExpr(ex, "diam");
  lastText = say;
  setText(robotName, say);
  setAct(ACT_TALK);
  bool stopped = false;
  bool ok = speakOnline(say, &stopped);
  if (!ok) { playTone(1319, 80); playSilence(40); playTone(1568, 120); }
  showIdle();
  if (ok && !stopped && ajak) {   // robot mengajak ngobrol: dengarkan jawabanmu tanpa disentuh
    inviteListen = true;
    followUp = true;
  }
  nextAt = millis() + SPONTAN_MIN_MS + esp_random() % SPONTAN_RAND_MS;
}

// Terapkan setelan yang dipilih di menu OLED (dipanggil loop, bukan task tampilan: menyimpan ke flash & bunyi).
// Kirim satu pengaturan milik server (menu OLED) ke /api/pengaturan.
void postServerSetting(int id, int v) {
  static const char *const KEY[12] = {"keluaran", "mesin_suara", "suara", "ai", "tempo", "nada",
                                      "keras_suara", "batas_speaker", "keras_musik", "senyap", "senyap_mulai",
                                      "senyap_selesai"};
  static const char *const OUTV[3] = {"robot", "hp", "keduanya"};
  static const char *const ENGV[2] = {"edge", "gemini"};
  static const char *const VOIV[2] = {"id-ID-GadisNeural", "id-ID-ArdiNeural"};
  static const char *const AIV[2] = {"groq", "gemini"};
  int i = id - S_OUT;
  if (i < 0 || i > 11) return;
  if (!srvKnown || WiFi.status() != WL_CONNECTED) {
    toast(TI_X, "Server", "belum tersambung", 2500);
    return;
  }
  String val;
  switch (i) {
    case 0: val = String("\"") + OUTV[constrain(v, 0, 2)] + "\""; break;
    case 1: val = String("\"") + ENGV[constrain(v, 0, 1)] + "\""; break;
    case 2: val = String("\"") + VOIV[constrain(v, 0, 1)] + "\""; break;
    case 3: val = String("\"") + AIV[constrain(v, 0, 1)] + "\""; break;
    case 7: val = String(v / 100.0f, 2); break;
    case 9: val = v ? "\"1\"" : "\"0\""; break;
    default: val = String(v); break;
  }
  HTTPClient http;
  http.setConnectTimeout(1500);
  http.setTimeout(2500);
  if (!http.begin(serverUrl() + "/api/pengaturan")) return;
  http.addHeader("Authorization", "Bearer " + tokenCfg);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(String("{\"") + KEY[i] + "\":" + val + "}");
  http.end();
  if (code == 200) {
    srvVal[i] = v;
    toast(TI_OK, SETS[id].label, "tersimpan", 1500);
  } else {
    toast(TI_X, SETS[id].label, code == 401 ? "token salah" : "gagal", 2500);
  }
}

// ---------------- Mode demo (tanpa internet) ----------------
// Gamepad SELECT: wajah berganti ekspresi, roda bergerak, suara dari cadangan di memori.
// Berhenti dengan tombol bicara atau B.
bool demoWait(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (talkPressed() || padStop) { padStop = false; return false; }
    delay(20);
  }
  return true;
}

void demoSay(const char *key, const char *text) {
  setText("Mode demo", text);
  setAct(ACT_TALK);
  if (!playPhrase(key)) { playTone(1046, 80); playSilence(40); playTone(1318, 120); }
  setAct(ACT_IDLE);
}

void runDemo() {
  static const char *const EX[] = {"senang", "kaget", "malu", "kagum", "tertawa", "cinta", "bingung", "normal"};
  padStop = false;
  lastActivity = millis();
  setExpr("senang", "");
  demoSay("demo1", "Halo! Aku Almara.");
  bool ok = demoWait(400);
  if (ok) demoSay("demo2", "Ekspresi & joget");
  for (int i = 0; ok && i < 8; i++) {
    setExpr(EX[i], "");
    ok = demoWait(1600);
  }
  if (ok) {
    setExpr("senang", "joget");
    ok = demoWait(4500);
  }
  setExpr("senang", "");
  if (ok) demoSay("demo3", "Tekan tombol & bicara!");
  lastActivity = millis();
  showIdle();
}

void saveSpk() {
  prefs.begin("robot", false);
  prefs.putUChar("sphp", spHp);
  prefs.putUChar("splp", spLp);
  prefs.putUChar("spgt", spGate);
  prefs.putUChar("spcl", (uint8_t)spCeil);
  prefs.putUChar("qvol", (uint8_t)quietVolLevel);
  prefs.putUChar("bpvol", (uint8_t)beepLevel);
  prefs.putUChar("bright", oledBright);
  prefs.end();
}

void applySettingReq() {
  int id = setApplyId;
  if (id < 0) return;
  setApplyId = -1;
  int v = setApplyVal;
  auto saveBool = [](const char *key, bool val) { prefs.begin("robot", false); prefs.putBool(key, val); prefs.end(); };
  switch (id) {
    case S_VOL: setVolume(v); playTone(1046, 80); break;
    case S_VIEW: setView(v); break;
    case S_THEME: setTheme(v); break;
    case S_FX: setFx(v); break;
    case S_TALK: setTalkMode(v, false); break;
    case S_OUT: case S_ENGINE: case S_VOICE: case S_AI: case S_RATE: case S_PITCH:
    case S_TTSDB: case S_LIMIT: case S_MUSDB: case S_QUIET: case S_QSTART: case S_QEND:
      postServerSetting(id, v);
      break;
    case S_JOGET: jogetAuto = v; saveBool("joget", jogetAuto); break;
    case S_BAT: batShow = v; batStable = 0; if (!batShow) batPct = -1; saveBool("batshow", batShow); break;
    case S_ALON: alarmOn = v; saveAlarm(); toast(TI_OK, "Alarm", v ? alarmText().c_str() : "mati", 2000); break;
    case S_ALH: alarmH = v; saveAlarm(); break;
    case S_ALM: alarmM = v; saveAlarm(); break;
    case S_ALREP: alarmRep = v; saveAlarm(); break;
    case S_ALTONE: {  // simpan lalu putar contoh sekali
      alarmTone = v;
      saveAlarm();
      bool dummy = false;
      alarmRinging = true;
      alarmSound(alarmTone, 0, [&]() { return talkPressed() || dummy; });
      alarmRinging = false;
      waitRelease();
      break;
    }
    case S_ALTES: ringAlarm(); break;
    case S_QVOL: quietVolLevel = constrain(v, 1, 10); saveSpk(); playTone(1046, 80); break;
    case S_BEEP: beepLevel = constrain(v, 1, 10); saveSpk(); okBeep(); break;
    case S_SPHP: spHp = constrain(v, 0, 6); spFxDirty = true; saveSpk(); speakerTest(false); break;
    case S_SPLP: spLp = constrain(v, 0, 5); spFxDirty = true; saveSpk(); speakerTest(false); break;
    case S_SPGATE: spGate = constrain(v, 0, 3); spFxDirty = true; saveSpk(); speakerTest(false); break;
    case S_SPCEIL: spCeil = constrain(v, 40, 100); spFxDirty = true; saveSpk(); speakerTest(false); break;
    case S_SPTEST: speakerTest(true); break;
    case S_SPRESET:
      spHp = 2; spLp = 0; spGate = 0; spCeil = 100; quietVolLevel = QUIET_VOL_DEFAULT; beepLevel = 5;
      spFxDirty = true; saveSpk();
      toast(TI_OK, "Suara", "disetel ulang", 1800);
      speakerTest(false);
      break;
    case S_BRIGHT: oledBright = constrain(v, 1, 10); saveSpk(); break;  // layar langsung berubah
    case S_HOT: hotRemindOn = v; saveBool("hotr", hotRemindOn); break;
    case S_SPONTAN: spontanOn = v; saveBool("spon", spontanOn); break;
    case S_ABOUT: {
      char a[20], b[22];
      snprintf(a, sizeof(a), "Versi " FW_VERSION);
      snprintf(b, sizeof(b), "RAM %dK PSRAM %dM", (int)(ESP.getFreeHeap() / 1024), (int)(ESP.getFreePsram() / 1048576));
      toast(TI_OK, a, b, 4000);
      break;
    }
    case S_INFO: showNetworkStatus(); break;
    case S_FW: pendingCmd = "perbarui firmware"; break;
    case S_WIFI: runPortal(); break;
    default: break;
  }
}


// ---- Diagnosis restart: penyebab restart terakhir tampil di OLED; 2x gagal menyala = mode aman ----
RTC_NOINIT_ATTR uint32_t bootMagic, bootFails, bootStage;
uint32_t prevStage = 0;
#define STAGE(n) (bootStage = (n))
const char *const STAGE_NAME[] = {"-", "awal menyala", "mic/speaker", "sambung WiFi", "simpan WiFi",
                                  "jam internet", "cek server", "unduh suara", "baca sensor", "cek firmware",
                                  "suara pertama", "jalan <1 menit", "jalan normal"};
const char *resetText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_BROWNOUT: return "DAYA KURANG\n(brownout)\nCek buck 5V,\nkapasitor, kabel.";
    case ESP_RST_PANIC: return "ERROR PROGRAM\n(crash/panic)\nKirim log Serial\nke Claude.";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "PROGRAM MACET\n(watchdog)\nKirim log Serial\nke Claude.";
    default: return nullptr;
  }
}
void bootDiagnose() {
  esp_reset_reason_t r = esp_reset_reason();
  if (bootMagic != 0xA1A4A7A0 || r == ESP_RST_POWERON) { bootMagic = 0xA1A4A7A0; bootFails = 0; bootStage = 0; }
  prevStage = bootStage < 13 ? bootStage : 0;
  STAGE(1);
  const char *why = resetText(r);
  if (why) bootFails++;
  safeMode = bootFails >= 2;
  Serial.printf("[ALMARA] reset reason=%d langkah=%s gagal berturut=%lu mode aman=%d\n", (int)r,
                STAGE_NAME[prevStage], (unsigned long)bootFails, safeMode);
  if (!why) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.print("Restart karena:");
  oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  oled.setCursor(0, 14);
  oled.print(why);
  oled.setCursor(0, 47);
  oled.print("Saat: ");
  oled.print(STAGE_NAME[prevStage]);
  if (safeMode) { oled.setCursor(0, 56); oled.print("MODE AMAN: pad mati"); }
  tftShowMain(oled, false, true);
  delay(4000);
}

// ---------------- OLED ketiga 128x32: efek suara, jawaban, musik ala Spotify ----------------
// Tugas sendiri di inti 0; pengiriman ke layar TFT dijaga mutex (tftMtx) supaya tidak bertabrakan dengan tampilan utama.
void infoTask(void *) {
  static Disp d;
  TickType_t wake = xTaskGetTickCount();
  bool dimmed = false;
  for (;;) {
    xSemaphoreTake(dispMtx, portMAX_DELAY);
    d = disp;
    xSemaphoreGive(dispMtx);
    uint32_t now = millis();
    if (d.act == ACT_INFO && d.infoUntil && now > d.infoUntil) d.act = ACT_IDLE;

    InfoMusic m;
    xSemaphoreTake(infoMtx, portMAX_DELAY);
    if (infoMusic.on && !infoMusic.playing && now - infoMusic.pausedAt > 90000) infoMusic.on = false;  // jeda lama: kembali ke beranda
    m = infoMusic;
    xSemaphoreGive(infoMtx);

    bool quiet = quietNow;
    if (quiet != dimmed) { infoOled.dim(quiet); dimmed = quiet; }

    int mode = IM_IDLE;
    bool recent = d.text[0] && d.textSince && now - d.textSince < 15000;
    if (d.act == ACT_LISTEN) mode = IM_LISTEN;
    else if (d.act == ACT_THINK) mode = IM_THINK;
    else if (m.on && (m.playing || !recent)) mode = IM_MUSIC;  // musik menang atas teks; saat dijeda, teks 15 dtk dulu
    else if ((d.act == ACT_TALK || d.act == ACT_INFO) && d.text[0]) mode = IM_TEXT;
    else if (recent) mode = IM_TEXT;

    InfoNotif nt;  // notifikasi HP menimpa semua layar kecuali saat mendengar / berpikir
    if (mode != IM_LISTEN && mode != IM_THINK && infoNotifCurrent(now, &nt)) mode = IM_NOTIF;

    if (mode == IM_NOTIF) {
      infoDrawNotif(now, nt);
    } else if (mode == IM_LISTEN) {
      infoDrawListen(now, audioLevel);
    } else if (mode == IM_THINK) {
      infoDrawThink(now);
    } else if (mode == IM_MUSIC) {
      infoDrawMusic(now, m);
    } else if (mode == IM_TEXT) {
      infoDrawText(now, d.text, d.textSince, audioLevel, d.act == ACT_TALK);
    } else {
      struct tm tmNow;
      bool timeOk = getLocalTime(&tmNow, 0);
      infoDrawHome(now, timeOk, tmNow.tm_hour, tmNow.tm_min, roomT, roomH, d.bat, d.online,
                   timeOk ? tmNow.tm_wday : -1, tmNow.tm_mday, tmNow.tm_mon,
                   (int32_t)(now - lastActivity) > 0 ? now - lastActivity : 0);
    }
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(25));  // ~40 fps (I2C 1 MHz: satu frame ~6 ms)
  }
}

// Playlist bisa berganti lagu di tengah aliran yang sama: tanya server judul yang sedang jalan (tiap 6 detik).
void nowTask(void *) {
  uint32_t last = 0;
  bool wasPlaying = false;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    bool playing = musicPlaying;
    if (playing && !wasPlaying) last = millis();  // lagu baru mulai: tunggu dulu 6 detik
    wasPlaying = playing;
    if (!playing || !serverOk || WiFi.status() != WL_CONNECTED) continue;
    if (millis() - last < 6000) continue;
    last = millis();
    HTTPClient h;
    h.setConnectTimeout(1500);
    h.setTimeout(1500);
    if (!h.begin(serverUrl() + "/now")) continue;
    h.addHeader("Authorization", "Bearer " + tokenCfg);
    if (h.GET() == 200) {
      String s = h.getString();  // "id|jenis|durasi|judul"
      int a = s.indexOf('|'), b = s.indexOf('|', a + 1), c = s.indexOf('|', b + 1);
      if (a > 0 && b > a && c > b) {
        infoMusicTitle(s.substring(0, a).toInt(), s.substring(c + 1).c_str(), s.substring(b + 1, c).toInt());
      }
    }
    h.end();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(SPK_SD, OUTPUT); digitalWrite(SPK_SD, LOW);  // speaker mati sampai ada suara
  pinMode(BTN_PIN, INPUT_PULLUP);
  // tombol fisik 11/12/13 sudah dihapus (diganti remote BLE); jangan di-pinMode karena pin 11/12 dipakai ulang
  pinMode(TALK_PIN, TALK_ACTIVE_HIGH ? INPUT_PULLDOWN : INPUT_PULLUP);
  analogSetPinAttenuation(BAT_PIN, ADC_11db);
  dht.begin();
  sdInit();  // kartu microSD (opsional): musik tanpa internet

  oled.begin();
  tftBegin();   // layar TFT (SPI): SCK=45, MOSI=13, DC=42, CS=47, RES=38, BLK=11
  bootDiagnose();
  dispMtx = xSemaphoreCreateMutex();
  prefs.begin("robot", true);
  disp.face = prefs.getBool("face", true);
  viewMode = prefs.getUChar("view", disp.face ? 0 : 1);
  disp.face = viewMode == 0;
  disp.mix = viewMode == 2;
  quietOn = prefs.getBool("qon", true);
  btMode = 0;  // speaker Bluetooth lewat ESP32 jembatan sudah tidak dipakai
  quietStart = prefs.getInt("qs", 22);
  quietEnd = prefs.getInt("qe", 5);
  volLevel = prefs.getInt("vol", 8);
  talkMode = prefs.getUChar("tkm", 0);
  alarmOn = prefs.getBool("alon", false);
  alarmH = constrain(prefs.getChar("alh", 6), 0, 23);
  alarmM = constrain(prefs.getChar("alm", 0), 0, 59);
  alarmRep = constrain(prefs.getChar("alr", 1), 0, 2);
  alarmTone = constrain(prefs.getChar("alt", 0), 0, N_ALTONE - 1);
  prefs.end();
  faceMode = disp.face;
  prefs.begin("robot", true);
  lightMode = disp.light = prefs.getBool("light", false);
  fxOn = disp.fx = prefs.getBool("fx", true);
  robotName = prefs.getString("name", "Almara");
  if (robotName == "Rara") robotName = "Almara";  // nama lama -> nama baru
  jogetAuto = prefs.getBool("joget", true);
  prefs.getBytes("hs", gameHigh, sizeof(gameHigh));
  batShow = prefs.getBool("batshow", true);
  spHp = constrain(prefs.getUChar("sphp", 2), 0, 6);
  spLp = constrain(prefs.getUChar("splp", 0), 0, 5);
  spGate = constrain(prefs.getUChar("spgt", 0), 0, 3);
  spCeil = constrain((int)prefs.getUChar("spcl", 100), 40, 100);
  quietVolLevel = constrain((int)prefs.getUChar("qvol", QUIET_VOL_DEFAULT), 1, 10);
  beepLevel = constrain((int)prefs.getUChar("bpvol", 5), 1, 10);
  oledBright = constrain(prefs.getUChar("bright", 10), 1, 10);
  hotRemindOn = prefs.getBool("hotr", true);
  spontanOn = prefs.getBool("spon", true);
  spFxDirty = true;
  prefs.end();
  strlcpy(disp.name, robotName.c_str(), sizeof(disp.name));
  replyVolume = VOL_GAIN[constrain(volLevel, 1, 10)];
  xTaskCreatePinnedToCore(displayTask, "tampilan", 8192, nullptr, 1, nullptr, 0);
  if (infoBegin()) {  // OLED 128x32 ketiga (kalau tidak terpasang, robot tetap jalan seperti biasa)
    xTaskCreatePinnedToCore(infoTask, "tampilan2", 6144, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(nowTask, "judul", 6144, nullptr, 1, nullptr, 1);
  } else {
    Serial.println("[TFT] layar ST7789 tidak dapat dimulai, dilewati.");
  }
  // LCD 16x2 tidak dipakai lagi: semua tampilan kembali penuh di OLED
  screen("Robot ngobrol v" FW_VERSION, "Menyiapkan...");

  recRaw = (int32_t *)ps_malloc(MAX_SAMPLES * sizeof(int32_t));
  rec16 = (int16_t *)ps_malloc(MAX_SAMPLES * sizeof(int16_t));
  body = (uint8_t *)ps_malloc(BODY_CAP);
  reply = (uint8_t *)ps_malloc(REPLY_CAP);
  if (!recRaw || !rec16 || !body || !reply) {
    screen("ERROR", "PSRAM tidak ada.\nFlash ulang file BIN.");
    while (true) delay(1000);
  }

  STAGE(2);
  mic.setPins(MIC_SCK, MIC_WS, -1, MIC_SD);
  bool micOk = mic.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
  // GPIO 17/18 bebas: jembatan ESP32 biasa (UART) tidak dipakai lagi.
  if (!safeMode) blePadBegin();  // mode aman: BLE remote tidak dinyalakan
  xTaskCreatePinnedToCore(padTask, "gamepad", 8192, nullptr, 2, nullptr, 0);
  spk.setPins(SPK_BCLK, SPK_LRC, SPK_DIN);
  bool spkOk = spk.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  if (!micOk || !spkOk) {
    screen("ERROR", String("Mic: ") + (micOk ? "OK" : "GAGAL") + "\nSpeaker: " + (spkOk ? "OK" : "GAGAL"));
    while (true) delay(1000);
  }

  fsOk = FFat.begin(true);  // partisi penyimpanan suara cadangan (diformat otomatis kali pertama)

  loadConfig();
  wm.addParameter(&pServer);
  wm.addParameter(&pToken);
  wm.setSaveParamsCallback(saveConfig);  // token tersimpan saat "Save" ditekan, sebelum menyambung WiFi
  wm.setAPCallback(onPortalStart);
  wm.setTitle("Almara");
  wm.setConnectTimeout(15);
  wm.setConfigPortalTimeout(180);

  screen("Menyambung WiFi...", safeMode ? "Mode aman" : "");
  STAGE(3);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_15dBm);  // lonjakan arus saat menyambung lebih kecil
  wm.autoConnect(AP_NAME);  // buka portal kalau belum ada WiFi tersimpan
  STAGE(4);
  saveConfig();
  wifiFast();
  STAGE(5);
  configTzTime("WIB-7", "pool.ntp.org", "time.google.com");  // jam dari internet (WIB)

  STAGE(6);
  serverOk = checkServer();
  STAGE(7);
  cachePhrases();
  STAGE(8);
  readBattery();
  delay(1000);  // DHT22 butuh waktu sejenak setelah menyala
  readRoom();
  fwMd5 = ESP.getSketchMD5();
  STAGE(9);
  if (serverOk) otaFromServer(false);  // hanya cek, tidak memasang otomatis
  setStatus("");
  showNetworkStatus();
  if (tokenCfg.length() == 0) {  // tanpa token server menolak semua permintaan (401)
    screen("TOKEN KOSONG", "Tahan tombol bicara\n10 dtk -> WiFi\nRobot-Setup ->\nisi ROBOT_TOKEN");
    errBeep();
    delay(5000);
  }
  STAGE(10);
  if (!speakOnline("Halo, aku sudah siap ngobrol!")) playPhrase("halo");
  STAGE(11);
  bootFails = 0;  // berhasil menyala sampai siap
}

void loop() {
  static uint32_t lastBat = 0, lastCheck = 0, lastStatus = 0, lastRoom = 0;
  uint32_t now = millis();

  if (bootStage == 11 && now > 60000) STAGE(12);
  checkTimers();
  checkAlarm();
  slotHousekeeping();
  applySettingReq();
  if (setActive) {  // menu pengaturan terbuka: tombol bicara = tutup, robot tidak mendengar
    if (talkPressed()) {
      setActive = false;
      waitRelease();
      playTone(660, 50); playSilence(20); playTone(440, 80);
    }
    while (sfxTail != sfxHead) {
      Sfx t = sfxQ[sfxTail];
      sfxTail = (sfxTail + 1) % 16;
      playTone(t.f, t.ms);
    }
    delay(10);
    return;
  }
  // ---- Bilah "atur volume": terapkan langsung, simpan saat OK / 10 detik tanpa tekan ----
  if (volAdjActive) {
    if (volAdjLevel != volLevel) {
      volLevel = volAdjLevel;
      replyVolume = VOL_GAIN[constrain(volLevel, 1, 10)];  // langsung terdengar, disimpan saat selesai
      playTone(1046, 70);                  // contoh kekerasan
    }
    if (talkPressed()) {  // tombol bicara = tutup
      waitRelease();
      if (!volAdjCloseAt) volAdjCloseAt = millis() | 1;
    }
    if (volAdjDone || (int32_t)(millis() - volAdjUntil) > 3000) {
      volAdjActive = false;
      volAdjDone = false;
      setVolume(volLevel);
      okBeep();
      lastText = "Volume " + String(volLevel * 10) + "%";
      showIdle();  // pop-up sudah hilang sendiri, langsung kembali ke wajah
    }
    delay(10);  // jangan memonopoli prosesor
    return;
  }

  // ---- Pengingat berulang dari server ----
  if (pendingReminder.length()) {
    if (gameActive) { gameReq = 2; delay(80); }
    stopDance();
    String m = pendingReminder;
    pendingReminder = "";
    for (int i = 0; i < 2; i++) { playTone(1568, 110); playSilence(70); playTone(1175, 140); playSilence(180); }
    setExpr("senang", "diam");
    lastText = "Pengingat:\n" + m;
    setText("Pengingat", m);
    setAct(ACT_TALK);
    if (!speakOnline(m)) { playTone(1568, 300); }
    showIdle();
    return;
  }

  if (pendingCmd.length() && !gameActive) {  // perpindahan saluran/lagu dari tombol (tanpa rekursi)
    String c = pendingCmd;
    pendingCmd = "";
    askServer(0, c.c_str());
    return;
  }
  if (zapReq && !gameActive) {  // tekan singkat kiri/kanan saat diam: nyalakan saluran/lagu lain
    int8_t z = zapReq;
    zapReq = 0;
    if (musicId) {
      zapFeedback(z);
      askServer(0, z > 0 ? "__next" : "__prev");
    }
    return;
  }

  while (sfxTail != sfxHead) {  // bunyi dari game
    Sfx t = sfxQ[sfxTail];
    sfxTail = (sfxTail + 1) % 16;
    playTone(t.f, t.ms);
  }
  if (quickReq) {
    uint8_t q = quickReq;
    quickReq = 0;
    if (q == 1 || q == 2) {
      if (q == 1) setTheme(!lightMode);
      else setFx(!fxOn);
      playTone(q == 1 && lightMode ? 1320 : 880, 60);
    } else if (q == 4) {  // gamepad A: dielus
      faceEvent(BH_PET);
      playTone(1200, 60); playSilence(30); playTone(1600, 90);
    } else if (q == 6) {  // gamepad SELECT: mode demo tanpa internet
      runDemo();
    }
  }
  if (serverOld && !serverOldShown && !gameActive) {
    serverOldShown = true;
    showError("Server di HP masih versi lama.\nSalin server.py terbaru ke ~/robot-server lalu jalankan ulang start.sh.");
  }
  if (gameHighDirty) {
    gameHighDirty = false;
    prefs.begin("robot", false);
    prefs.putBytes("hs", gameHigh, sizeof(gameHigh));
    prefs.end();
  }
  if (gameActive && gameWantMic) {  // game suara: baca mic terus-menerus
    static int32_t blk[BLOCK];
    static float noise = -60;
    size_t n = readMic24(blk);
    double sum = 0, mean = 0;
    for (size_t i = 0; i < n; i++) mean += blk[i];
    mean /= n ? n : 1;
    for (size_t i = 0; i < n; i++) { double d = blk[i] - mean; sum += d * d; }
    float db = n ? 20.0f * log10f(sqrtf(sum / n) / 8388608.0f + 1e-9f) : -120;
    if (db < noise) noise = db; else noise += 0.002f * (db - noise);  // ikuti bising ruangan pelan-pelan
    float lvl = constrain((db - noise - 8) / 30.0f, 0.0f, 1.0f);       // 8 dB di atas bising = mulai naik
    gameMicLevel = gameMicLevel * 0.6f + lvl * 0.4f;
  } else {
    gameMicLevel = 0;
  }
  if (gameActive) {  // saat bermain, tombol bicara = keluar dari game (LB gamepad tidak ikut)
    if (hwTalkPressed()) {
      gameReq = 2;
      waitRelease();
      playTone(660, 50); playSilence(20); playTone(440, 80);
    }
  } else if (talkPressed() || followUp) {
    bool autoL = followUp;
    bool invited = inviteListen && autoL;   // robot yang mengajak ngobrol: tunggu jawaban lebih lama, batal tanpa bunyi bip
    inviteListen = false;
    tapWaitMs = invited ? 9000 : TAP_WAIT_MS;
    followUp = false;
    stopDance();
    bool wantSetup = false, noSpeech = false;
    size_t count = record(&wantSetup, &noSpeech, autoL);
    if (wantSetup) {
      waitRelease();
      runPortal();
      return;
    }
    size_t n = processRecording(count);
    static uint32_t lastTapAt = 0;  // ketuk 2x cepat saat diam = musik dari kartu SD (tanpa suara / internet)
    if (noSpeech || !n) {
      uint32_t tnow = millis();
      bool dbl = lastTapAt && tnow - lastTapAt < 1500;
      lastTapAt = dbl ? 0 : tnow;
      if (dbl && !autoL) {
        showIdle();
        sdCommand("");
        return;
      }
    }
    if (noSpeech || (!n && (talkMode == 1 || autoL))) {  // mode klik: tidak ada suara = batal, bukan dielus
      showIdle();
      if (!invited) { playTone(660, 50); playSilence(20); playTone(440, 70); }   // ajakan tak dijawab: diam saja
      return;
    }
    if (!n) {  // tekan sebentar saja: dielus, bukan perintah
      showIdle();
      faceEvent(BH_PET);
      playTone(1200, 60); playSilence(30); playTone(1600, 90); playSilence(60);
      return;
    }
    askServer(n);  // robot hanya mendengar lagi kalau tombol ditekan lagi
    return;
  }

  // ---- Teman belajar: ganti fase fokus -> istirahat -> selesai ----
  if (focusOn && (long)(millis() - focusEnd) >= 0 && !gameActive) {
    stopDance();
    String say;
    if (!focusRest) {
      focusSessions++;
      startFocus(true);
      say = "Waktunya istirahat " + String(focusBreak) + " menit! Kamu sudah fokus " + String(focusWork) +
            " menit, hebat banget!";
    } else {
      stopFocus();
      say = "Istirahat selesai! Hari ini kamu sudah " + String(focusSessions) +
            " sesi belajar. Mau lanjut? Bilang temani aku belajar lagi ya.";
    }
    okBeep();
    setExpr("senang", "diam");
    lastText = say;
    setText(focusOn ? "Istirahat" : "Sesi selesai", say);
    setAct(ACT_TALK);
    if (!speakOnline(say)) { playTone(1046, 150); playSilence(60); playTone(1319, 200); }
    showIdle();
    return;
  }

  // ---- Pesan dari HP (diketik di halaman /pengaturan) ----
  if (pendingMsg.length() && !gameActive) {
    stopDance();
    String m = pendingMsg;
    pendingMsg = "";
    playTone(1319, 90); playSilence(40); playTone(988, 160); playSilence(60);  // ding-dong
    setExpr("senang", "diam");
    lastText = "Pesan dari HP:\n" + m;
    setText("Pesan dari HP", m);
    setAct(ACT_TALK);
    if (!speakOnline(m)) errBeep();
    showIdle();
    return;
  }

  // ---- Joget sendiri kalau mendengar musik ----
  if (jogetAuto && !gameActive && !focusOn && !quietNow && !volAdjActive) danceMonitor();
  else stopDance();

  quietNow = computeQuiet();
  btPoll();
  if (now - lastStatus > 500) {  // perbarui hitung mundur timer & ikon
    lastStatus = now;
    setStatus(nextTimerText().c_str());
  }
  if (now - lastRoom > 5000) {  // DHT22 maksimal dibaca tiap 2 detik
    lastRoom = now;
    readRoom();
    hotReminder();
  }
  if (reactReq) { Behav rb = (Behav)reactReq; reactReq = 0; if (disp.act == ACT_IDLE) faceEvent(rb); }   // reaksi wajah
  spontanTick();   // robot bicara / mengajak ngobrol sendiri
  if (now - lastBat > 10000) {
    lastBat = now;
    readBattery();
  }
  if (now - lastCheck > (serverOk ? 10000UL : 15000UL)) {  // cek server + ambil perintah dari /pengaturan
    lastCheck = now;
    bool wasOk = serverOk;
    serverOk = checkServer();
    if (serverOk && !wasOk) cachePhrases();  // server hidup lagi: lengkapi suara cadangan
  }
  delay(10);
}
