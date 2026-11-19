// ble_pad.h — pengganti Bluepad32 DAN espnow_pad.h: robot jadi server BLE sendiri (BUKAN gamepad
// Bluepad32 X3, BUKAN ESP-NOW), remote ESP32-C3 jadi client BLE yang menyambung ke robot lewat
// Bluetooth. Mengisi struct `Pad` yang SAMA dipakai kode lama (padNow / padGet()) — jadi semua
// kode menu & game yang sudah ada (cur[0..6] dari p.btn/p.dpad, dst.) TIDAK perlu diubah.
//
// Kenapa robot = SERVER, remote = CLIENT: robot tidak perlu tahu alamat BLE remote lebih dulu
// (tidak seperti ESP-NOW yang butuh MAC ditulis manual di firmware remote). Remote yang mencari
// & menyambung ke robot lewat nama/UUID service di bawah — robot cukup menyala & menunggu.
//
// Butuh library "NimBLE-Arduino" (Tools > Manage Libraries, cari "NimBLE-Arduino" oleh h2zero).
// ESP32-C3 Super Mini hanya punya BLE (tidak ada Bluetooth Classic), jadi BLE adalah satu-satunya
// pilihan "Bluetooth" yang bisa dipakai di kedua sisi.
//
// Cara pasang di robot_ngobrol.ino:
//   1) #include "ble_pad.h"   // taruh menggantikan #include "espnow_pad.h" / Bluepad32
//   2) di setup(): blePadBegin();     // boleh dipanggil kapan saja, tidak perlu WiFi/router
//   3) di padTask(): blePadTick();    // gantikan padDirectTick(millis()) / espnowPadTick()
//   4) tombol tangan kiri/kanan otomatis memanggil gerakTanganKiri()/gerakTanganKanan() dari
//      gerak.h — pastikan "#include "gerak.h"" ada SEBELUM file ini.
//
// Tombol remote dipetakan ke bit gamepad lama, supaya menu/game jalan apa adanya:
//   Joystick  -> p.lx / p.ly / p.dpad   Bicara -> PB_LB   4 tombol game/menu -> PB_A/B/X/Y
//   Tangan kanan -> PADC_HANDR (trigger gerak.h saja)    Tangan kiri -> PADC_HANDL
#pragma once
#include <NimBLEDevice.h>

// UUID acak tetap (jangan diubah — remote mencari service ini). Boleh diganti kalau mau beberapa
// robot+remote hidup berdekatan supaya tidak saling nyambung (ganti di KEDUA file: ini & robot_remote_c3.ino).
#define BLEPAD_SVC_UUID  "7a1e2b10-0001-4a9f-9d2c-5c6f0a1b2c3d"
#define BLEPAD_CHR_UUID  "7a1e2b10-0002-4a9f-9d2c-5c6f0a1b2c3d"
#define BLEPAD_DEV_NAME  "Almara"   // nama BLE robot, dicari oleh remote

#define PADC_TALK   0x01
#define PADC_HANDR  0x02
#define PADC_HANDL  0x04
#define PADC_A      0x08
#define PADC_B      0x10
#define PADC_X      0x20
#define PADC_Y      0x40

typedef struct __attribute__((packed)) {
  int16_t lx, ly;   // -512..512, arah joystick kaki (maju/mundur/kiri/kanan)
  uint8_t btn;      // bit PADC_* di atas
  uint8_t seq;
} BlePadPkt;

static volatile BlePadPkt blePadLast;
static volatile uint32_t blePadAt = 0;
static volatile bool blePadConnected = false;

class BlePadChrCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    std::string v = c->getValue();
    if (v.size() != sizeof(BlePadPkt)) return;
    memcpy((void *)&blePadLast, v.data(), sizeof(BlePadPkt));
    blePadAt = millis();
  }
};
class BlePadSrvCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, NimBLEConnInfo &) override { blePadConnected = true; react(BH_WOW); }
  void onDisconnect(NimBLEServer *s, NimBLEConnInfo &, int) override {
    blePadConnected = false;
    s->getAdvertising()->start();  // siap disambung remote lagi
  }
};
static BlePadChrCb blePadChrCb;
static BlePadSrvCb blePadSrvCb;

static void blePadBegin() {
  NimBLEDevice::init(BLEPAD_DEV_NAME);
  NimBLEServer *srv = NimBLEDevice::createServer();
  srv->setCallbacks(&blePadSrvCb);
  NimBLEService *svc = srv->createService(BLEPAD_SVC_UUID);
  NimBLECharacteristic *chr = svc->createCharacteristic(
      BLEPAD_CHR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  chr->setCallbacks(&blePadChrCb);
  svc->start();
  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(BLEPAD_SVC_UUID);
  adv->setName(BLEPAD_DEV_NAME);
  adv->start();
  Serial.println("[BLE] siap, menunggu remote...");
}

// Panggil tiap 10ms (gantikan padDirectTick/espnowPadTick lama) dari padTask().
static void blePadTick() {
  uint32_t now = millis();
  Pad p;
  p.at = now;
  if (blePadConnected && now - blePadAt < 400) {  // data masih baru = remote tersambung & aktif
    BlePadPkt pkt;
    memcpy(&pkt, (const void *)&blePadLast, sizeof(pkt));
    p.conn = true;
    p.lx = pkt.lx; p.ly = pkt.ly; p.rx = 0; p.ry = 0;
    if (pkt.btn & PADC_TALK) p.btn |= PB_LB;
    if (pkt.btn & PADC_A) p.btn |= PB_A;
    if (pkt.btn & PADC_B) p.btn |= PB_B;
    if (pkt.btn & PADC_X) p.btn |= PB_X;
    if (pkt.btn & PADC_Y) p.btn |= PB_Y;
    p.dpad = (pkt.ly < -200 ? PD_UP : 0) | (pkt.ly > 200 ? PD_DOWN : 0) |
             (pkt.lx > 200 ? PD_RIGHT : 0) | (pkt.lx < -200 ? PD_LEFT : 0);

    static uint8_t prevBtn = 0;  // tangan: pemicu sekali tiap tombol DITEKAN, bukan ditahan
    if ((pkt.btn & PADC_HANDR) && !(prevBtn & PADC_HANDR)) gerakTanganKanan();
    if ((pkt.btn & PADC_HANDL) && !(prevBtn & PADC_HANDL)) gerakTanganKiri();
    prevBtn = pkt.btn;
  }
  taskENTER_CRITICAL(&padMux);
  padNow = p;
  taskEXIT_CRITICAL(&padMux);
}
