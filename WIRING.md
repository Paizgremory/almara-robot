# Wiring Almara v9.6 — Layar TFT 1.8" 128x160 ST7735 + SD built-in

## Layar TFT (modul V1.1, pin kanan)
| Pin Modul | GPIO ESP32-S3 |
|-----------|--------------|
| VCC       | 3.3V         |
| GND       | GND          |
| CS        | GPIO 47      |
| RESET     | GPIO 38      |
| A0 / DC   | GPIO 42      |
| SDA/MOSI  | GPIO 41      |
| SCK       | GPIO 45      |
| LED       | GPIO 11      |

## Slot SD built-in (pin kiri modul, berbagi MOSI+SCK dengan layar)
| Pin Modul | GPIO ESP32-S3 |
|-----------|--------------|
| SD_CS     | GPIO 9       |
| SD_MOSI   | GPIO 41 (sama dengan SDA layar) |
| SD_MISO   | GPIO 3       |
| SD_SCK    | GPIO 45 (sama dengan SCK layar) |

## Komponen lain (tidak berubah)
| Komponen         | Pin                              |
|------------------|----------------------------------|
| Mic INMP441      | SCK=5, WS=4, SD=6                |
| Speaker MAX98357A| BCLK=15, LRC=16, DIN=7, SD=GPIO2|
| Sensor sentuh    | GPIO 10                          |
| Tombol fisik     | (dihapus — kontrol lewat remote BLE) |
| DHT22            | GPIO 8                           |
| Servo tangan     | L=GPIO17, R=GPIO18               |
| Servo paha       | L=GPIO14, R=GPIO21               |
| Servo engkel     | L=GPIO12, R=GPIO40               |

## Catatan
- Modul SD terpisah (yang lama) DILEPAS — tidak dibutuhkan lagi.
- SD dan layar berbagi bus SPI (FSPI). Tidak ada konflik karena CS berbeda.
- Kalau layar warna terbalik/aneh: ganti INITR_BLACKTAB ke INITR_REDTAB di layar_tft.h baris initR().
- Kalau layar terbalik atas-bawah: ganti setRotation(0) ke setRotation(2) di layar_tft.h.
