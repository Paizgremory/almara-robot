// Penyesuaian supaya firmware Almara bisa dikompilasi di dua paket papan:
//   - ESP32 Arduino 3.x (bawaan)            : ESP_I2S, ledcAttach(pin)
//   - Arduino 2.x (board lama)              : I2S & LEDC model lama
// Bluepad32 TIDAK dipakai lagi: remote ESP32-C3 kini menyambung langsung ke robot lewat BLE sendiri
// (lihat ble_pad.h), jadi PAD_DIRECT selalu 1 (pad dibaca langsung oleh ESP32-S3, bukan lewat
// jembatan ESP32 UART).
#pragma once
#include <Arduino.h>

#define PAD_DIRECT 1

#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include <ESP_I2S.h>
inline void pwmAttach(uint8_t pin, uint8_t, uint32_t freq, uint8_t bits) { ledcAttach(pin, freq, bits); }
inline void pwmWrite(uint8_t pin, uint32_t duty) { ledcWrite(pin, duty); }
#else
// ---------- Arduino 2.x: I2S lewat driver lama, dibungkus seperti ESP_I2S ----------
#include <driver/i2s.h>
#define I2S_MODE_STD 0
#define I2S_DATA_BIT_WIDTH_16BIT 16
#define I2S_DATA_BIT_WIDTH_32BIT 32
#define I2S_SLOT_MODE_MONO 1
#define I2S_SLOT_MODE_STEREO 2
#define I2S_STD_SLOT_LEFT 1

class I2SClass {
  i2s_port_t port;
  int bck = -1, ws = -1, dout = -1, din = -1;
  bool mono = false;
  int bits = 16;
  int ch = -1;          // mic: kanal yang dipakai (dideteksi otomatis: kiri/kanan yang ada suaranya)
  double e[2] = {0, 0};
  int probe = 0;
  int32_t tmp[512];
  static int nextPort() { static int n = 0; return n++; }

 public:
  I2SClass() : port((i2s_port_t)nextPort()) {}
  void setPins(int b, int w, int dataOut, int dataIn = -1) { bck = b; ws = w; dout = dataOut; din = dataIn; }
  bool begin(int, uint32_t rate, int bitWidth, int slotMode, int = 0) {
    mono = slotMode == I2S_SLOT_MODE_MONO;
    bits = bitWidth;
    i2s_config_t c = {};
    c.mode = (i2s_mode_t)(I2S_MODE_MASTER | (din >= 0 ? I2S_MODE_RX : I2S_MODE_TX));
    c.sample_rate = rate;
    c.bits_per_sample = (i2s_bits_per_sample_t)bitWidth;
    c.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;  // mic: rekam dua kanal, pilih yang ada suaranya
    c.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    c.dma_buf_count = 8;
    c.dma_buf_len = 256;
    c.tx_desc_auto_clear = true;
    if (i2s_driver_install(port, &c, 0, nullptr) != ESP_OK) return false;
    i2s_pin_config_t p = {};
    p.mck_io_num = I2S_PIN_NO_CHANGE;
    p.bck_io_num = bck;
    p.ws_io_num = ws;
    p.data_out_num = dout >= 0 ? dout : I2S_PIN_NO_CHANGE;
    p.data_in_num = din >= 0 ? din : I2S_PIN_NO_CHANGE;
    return i2s_set_pin(port, &p) == ESP_OK;
  }
  // Mic (32 bit mono): baca stereo lalu ambil satu kanal.
  size_t readBytes(char *buf, size_t n) {
    if (!mono || bits != 32) { size_t r = 0; i2s_read(port, buf, n, &r, portMAX_DELAY); return r; }
    size_t want = min(n / 4, sizeof(tmp) / 8) * 8, r = 0;
    i2s_read(port, tmp, want, &r, portMAX_DELAY);
    size_t frames = r / 8;
    if (ch < 0) {  // ~0,3 detik pertama: ukur energi tiap kanal
      for (size_t i = 0; i < frames; i++) {
        e[0] += fabs((double)(tmp[2 * i] >> 8));
        e[1] += fabs((double)(tmp[2 * i + 1] >> 8));
      }
      if (++probe > 20) ch = e[1] > e[0] * 1.5 ? 1 : 0;
    }
    int c = ch < 0 ? 0 : ch;
    int32_t *out = (int32_t *)buf;
    for (size_t i = 0; i < frames; i++) out[i] = tmp[2 * i + c];
    return frames * 4;
  }
  size_t write(const uint8_t *buf, size_t n) { size_t w = 0; i2s_write(port, buf, n, &w, portMAX_DELAY); return w; }
};

// LEDC model lama: kanal dipilih manual, diingat per pin
inline uint8_t &pwmChanOf(uint8_t pin) { static uint8_t m[49] = {0}; return m[pin]; }
inline void pwmAttach(uint8_t pin, uint8_t chan, uint32_t freq, uint8_t bits) {
  ledcSetup(chan, freq, bits);
  ledcAttachPin(pin, chan);
  pwmChanOf(pin) = chan;
}
inline void pwmWrite(uint8_t pin, uint32_t duty) { ledcWrite(pwmChanOf(pin), duty); }
#endif
