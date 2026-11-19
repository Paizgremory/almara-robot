# Wajah full-color baru (wajah_warna.h)

Modul wajah baru, warna penuh (bukan 1-bit tint seperti wajah.h lama), dibuat untuk TFT ST7735
1,8" 128x160 yang dipasang **landscape** (160x128 efektif). Tampilan: mata hijau rounded-square,
kerlip putih ganda di tiap mata, pipi coklat (blush), senyum kecil, kerlip "+" putih/oranye di
sekitar wajah, dan bingkai hijau di tepi layar — mengikuti referensi yang diminta.

## Coba langsung (tanpa utak-atik firmware_robot/)

Folder `demo_wajah_warna/` ini sudah lengkap (ada `.ino` + `wajah_warna.h` sendiri). Tinggal:

1. Buka `demo_wajah_warna.ino` di Arduino IDE / PlatformIO.
2. Pilih board ESP32-S3 kamu.
3. Flash. Pinout sudah sama persis dengan `WIRING.md` project ini, jadi tidak perlu ubah apa pun.

Sketch demo ini akan memutar siklus 4 state (IDLE -> LISTEN -> THINK -> TALK) tiap 12 detik supaya
kamu bisa lihat semua animasi tanpa perlu nyambung ke mic/speaker asli dulu.

## Menyambungkan ke firmware_robot/ (robot_ngobrol.ino)

`wajah_warna.h` juga sudah disalin ke dalam `firmware_robot/` (satu folder dengan
`robot_ngobrol.ino`), supaya gampang dipakai di situ. Tapi perlu dicatat:

- File `wajah.h` asli (yang dulu dipanggil lewat `#include "wajah.h"` di `robot_ngobrol.ino`)
  **tidak ikut** ter-upload, jadi saya tidak tahu persis `enum Act`, struct animasi, dan parameter
  lain yang dipakai di `drawFace(...)` yang asli.
- Karena itu `wajah_warna.h` dibuat dengan API sendiri yang sederhana, bukan pengganti 1:1 untuk
  `drawFace(...)`:

  ```cpp
  #include "wajah_warna.h"

  void setup() {
    faceInit();
  }

  void loop() {
    faceSetState(FACE_IDLE);     // atau FACE_LISTEN / FACE_THINK / FACE_TALK
    faceSetAudioLevel(0.0f);     // 0..1, dipakai saat FACE_TALK & FACE_LISTEN
    faceSetTime(13, 22, true);   // jam, menit, timeOk
    faceUpdate();                // panggil tiap loop; redraw otomatis ~30fps
  }
  ```

- Untuk menyambung ke `robot_ngobrol.ino` yang sebenarnya, ganti pemanggilan
  `drawFace(oled, anim, d.act, ...)` di sekitar baris 1223 dengan pemanggilan `faceSetState()` /
  `faceSetAudioLevel()` / `faceSetTime()` / `faceUpdate()` di atas, sesuaikan dengan variabel
  `d.act`, `audioLevel`, `tmNow` yang sudah ada di situ. Kalau mau saya sambungkan otomatis,
  upload saja `wajah.h` yang hilang itu, nanti saya kerjakan penuh.

## Kalau mau ubah tampilan lagi

Semua ukuran/posisi (mata, pipi, senyum, kerlip, bingkai) ada di fungsi `faceUpdate()` dalam
`wajah_warna.h`, dikomentari dalam Bahasa Indonesia biar gampang diutak-atik sendiri juga.
