// robot_remote_c3.ino — remote untuk Almara, pakai ESP32-C3 Super Mini.
// Joystick analog (kaki: maju/mundur/kiri/kanan) + 1 tombol bicara + 2 tombol tangan + 4 tombol game/menu.
// Kirim ke robot lewat BLUETOOTH (BLE) — C3 jadi CLIENT yang mencari & menyambung ke robot (SERVER),
// jadi tidak perlu tahu MAC robot atau SSID WiFi rumah sama sekali (beda dari versi ESP-NOW lama).
// Catatan: ESP32-C3 Super Mini hanya punya BLE (tidak ada Bluetooth Classic/SPP).
//
// Wiring (semua tombol: satu kaki ke GPIO, satu kaki ke GND, pakai pull-up internal, LOW = ditekan):
//   Joystick VRx -> GPIO 0 (ADC)     Joystick VRy -> GPIO 1 (ADC)     Joystick VCC->3V3, GND->GND
//   Tombol bicara     -> GPIO 2
//   Tangan kiri       -> GPIO 3
//   Tangan kanan      -> GPIO 4
//   Game/menu A       -> GPIO 5
//   Game/menu B       -> GPIO 6
//   Game/menu X       -> GPIO 7
//   Game/menu Y       -> GPIO 10
// (GPIO 8/9 sengaja tidak dipakai: strap boot pada banyak board C3 Super Mini.)
//
// Butuh library "NimBLE-Arduino" (Tools > Manage Libraries, cari "NimBLE-Arduino" oleh h2zero) —
// SAMA PERSIS seperti yang dipakai robot di ble_pad.h. UUID & nama di bawah HARUS SAMA dengan
// BLEPAD_SVC_UUID / BLEPAD_CHR_UUID / BLEPAD_DEV_NAME di ble_pad.h.

#include <NimBLEDevice.h>

#define BLEPAD_SVC_UUID  "7a1e2b10-0001-4a9f-9d2c-5c6f0a1b2c3d"
#define BLEPAD_CHR_UUID  "7a1e2b10-0002-4a9f-9d2c-5c6f0a1b2c3d"
#define BLEPAD_DEV_NAME  "Almara"

#define PIN_VRX 0
#define PIN_VRY 1
#define PIN_TALK 2
#define PIN_HANDL 3
#define PIN_HANDR 4
#define PIN_A 5
#define PIN_B 6
#define PIN_X 7
#define PIN_Y 10

#define PADC_TALK  0x01
#define PADC_HANDR 0x02
#define PADC_HANDL 0x04
#define PADC_A     0x08
#define PADC_B     0x10
#define PADC_X     0x20
#define PADC_Y     0x40

typedef struct __attribute__((packed)) {
  int16_t lx, ly;
  uint8_t btn;
  uint8_t seq;
} BlePadPkt;

static uint16_t adcMidX = 2048, adcMidY = 2048;  // kalibrasi tengah joystick, diisi otomatis saat nyala
static NimBLEClient *bleClient = nullptr;
static NimBLERemoteCharacteristic *bleChr = nullptr;
static bool bleScanning = false;

static inline bool pr(uint8_t pin) { return digitalRead(pin) == LOW; }

// Dipanggil tiap kali hasil scan BLE ketemu satu perangkat; sambung kalau itu robotnya.
class ScanCb : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *d) override {
    if (!d->isAdvertisingService(NimBLEUUID(BLEPAD_SVC_UUID))) return;
    NimBLEDevice::getScan()->stop();
    if (bleClient && bleClient->isConnected()) bleClient->disconnect();
    bleClient = NimBLEDevice::createClient();
    if (!bleClient->connect(d)) {
      Serial.println("[REMOTE] gagal connect, scan ulang");
      bleChr = nullptr;
      return;
    }
    NimBLERemoteService *svc = bleClient->getService(BLEPAD_SVC_UUID);
    bleChr = svc ? svc->getCharacteristic(BLEPAD_CHR_UUID) : nullptr;
    Serial.println(bleChr ? "[REMOTE] tersambung ke robot" : "[REMOTE] service/characteristic tidak ketemu");
  }
};
static ScanCb scanCb;

static void bleStartScan() {
  if (bleScanning) return;
  bleScanning = true;
  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&scanCb);
  scan->setInterval(45);
  scan->setWindow(45);
  scan->setActiveScan(true);
  scan->start(0, false);  // 0 = terus-menerus sampai ketemu/berhenti sendiri
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TALK, INPUT_PULLUP);
  pinMode(PIN_HANDL, INPUT_PULLUP);
  pinMode(PIN_HANDR, INPUT_PULLUP);
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_X, INPUT_PULLUP);
  pinMode(PIN_Y, INPUT_PULLUP);

  // kalibrasi tengah joystick: rata-rata 30 bacaan saat nyala (jangan sentuh joystick saat ini)
  uint32_t sx = 0, sy = 0;
  for (int i = 0; i < 30; i++) { sx += analogRead(PIN_VRX); sy += analogRead(PIN_VRY); delay(5); }
  adcMidX = sx / 30; adcMidY = sy / 30;

  NimBLEDevice::init("");
  bleStartScan();
  Serial.println("[REMOTE] siap, mencari robot lewat BLE...");
}

void loop() {
  static uint8_t seq = 0;
  bool connected = bleClient && bleClient->isConnected() && bleChr;
  bleScanning = connected ? false : bleScanning;   // izinkan scan ulang kalau putus
  if (!connected) {
    bleStartScan();
    delay(200);
    return;
  }

  BlePadPkt pkt;
  int rawX = analogRead(PIN_VRX) - adcMidX;   // -2048..2047 relatif tengah
  int rawY = analogRead(PIN_VRY) - adcMidY;
  pkt.lx = constrain(rawX / 2, -512, 512);    // skala ke -512..512 (sama rentang dgn kode gamepad lama)
  pkt.ly = constrain(rawY / 2, -512, 512);
  if (abs(pkt.lx) < 40) pkt.lx = 0;           // zona mati tengah joystick (hindari gerak sendiri)
  if (abs(pkt.ly) < 40) pkt.ly = 0;

  pkt.btn = 0;
  if (pr(PIN_TALK)) pkt.btn |= PADC_TALK;
  if (pr(PIN_HANDL)) pkt.btn |= PADC_HANDL;
  if (pr(PIN_HANDR)) pkt.btn |= PADC_HANDR;
  if (pr(PIN_A)) pkt.btn |= PADC_A;
  if (pr(PIN_B)) pkt.btn |= PADC_B;
  if (pr(PIN_X)) pkt.btn |= PADC_X;
  if (pr(PIN_Y)) pkt.btn |= PADC_Y;
  pkt.seq = seq++;

  bleChr->writeValue((uint8_t *)&pkt, sizeof(pkt), false);  // write no-response: cepat, tidak nunggu ACK
  delay(20);   // ~50x/detik, cukup responsif untuk joystick & tombol
}
