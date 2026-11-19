// Musik dari kartu microSD (tanpa internet) untuk Almara.
// Modul SD built-in di modul TFT 1,8" (berbagi bus FSPI dengan layar):
// SD_CS=GPIO9, SD_SCK=GPIO45, SD_MOSI=GPIO41, SD_MISO=GPIO3
// Kartu FAT32, lagu MP3 di folder /musik (kalau folder tidak ada, dipakai folder paling atas).
//
// Kendali sensor sentuh saat lagu diputar:
//   ketuk 1x  = jeda / lanjut        ketuk 2x = lagu berikutnya
//   ketuk 3x  = lagu sebelumnya      ketuk 4x = berhenti
//   TAHAN >0,6 dtk = musik berhenti sementara dan robot mendengarkan perintah suara
//   (ucapan: "lanjutkan lagunya", "lagu berikutnya", "lagu sebelumnya", "hentikan musik",
//    "putar lagu <judul> dari kartu memori", "putar musik dari kartu secara acak")
// Memulai tanpa suara / tanpa internet: ketuk sensor 2x cepat saat robot diam.
#pragma once
#include <SD.h>
#include <SPI.h>
#include <vector>
#include <algorithm>

#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#define SD_CS   9
#define SD_SCK  45  // sama dengan TFT_SCK
#define SD_MOSI 41  // sama dengan TFT_MOSI (berubah dari 48)
#define SD_MISO 3   // MISO hanya dipakai SD (layar tidak butuh)
#define SD_DIR  "/musik"
#define SD_MAX_LAGU 400

static SPIClass sdSpi(FSPI);  // ganti HSPI->FSPI: berbagi bus dengan layar TFT
static bool sdReady = false;
static std::vector<String> sdList;      // path lengkap tiap lagu
static int sdIdx = 0;                   // lagu terakhir / yang sedang dimainkan
static uint32_t sdResumeByte = 0;       // posisi (byte) tempat lagu terhenti oleh perintah suara
static bool sdSession = false;          // ada lagu yang bisa dilanjutkan
static bool sdShuffle = false;

enum SdRes : uint8_t { SDR_CONT = 0, SDR_END, SDR_NEXT, SDR_PREV, SDR_STOP, SDR_TALK, SDR_ALARM, SDR_FAIL };

static bool sdEndsMp3(const String &n) {
  String l = n;
  l.toLowerCase();
  return l.endsWith(".mp3");
}

static String sdTitle(const String &path) {
  int s = path.lastIndexOf('/');
  String n = s >= 0 ? path.substring(s + 1) : path;
  int d = n.lastIndexOf('.');
  if (d > 0) n = n.substring(0, d);
  n.replace('_', ' ');
  return n;
}

static void sdScan() {
  sdList.clear();
  const char *dirs[] = {SD_DIR, "/"};
  for (const char *dp : dirs) {
    File dir = SD.open(dp);
    if (!dir || !dir.isDirectory()) { if (dir) dir.close(); continue; }
    for (File f = dir.openNextFile(); f && (int)sdList.size() < SD_MAX_LAGU; f = dir.openNextFile()) {
      if (!f.isDirectory() && f.size() > 8000) {
        String p = f.path();
        String nm = p.substring(p.lastIndexOf('/') + 1);
        if (!nm.startsWith(".") && sdEndsMp3(nm)) sdList.push_back(p);
      }
      f.close();
    }
    dir.close();
    if (!sdList.empty()) break;  // /musik ada isinya: cukup
  }
  std::sort(sdList.begin(), sdList.end(), [](const String &a, const String &b) { return a.compareTo(b) < 0; });
}

// Dipanggil saat menyala dan setiap kali musik SD diminta (kartu boleh dicabut / dipasang kapan saja).
static bool sdInit() {
  if (sdReady) SD.end();
  sdReady = false;
  sdSpi.end();
  sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (SD.begin(SD_CS, sdSpi, 20000000)) {
    sdReady = true;
    sdScan();
    if (sdIdx >= (int)sdList.size()) sdIdx = 0;
  }
  Serial.printf("[SD] kartu %s, %d lagu\n", sdReady ? "terbaca" : "TIDAK terbaca", (int)sdList.size());
  return sdReady;
}

// ---------- pencocokan judul ----------
static String sdNorm(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c >= 'A' && c <= 'Z') c += 32;
    o += ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) ? c : ' ';
  }
  return o;
}

static int sdFind(const String &query) {
  String q = sdNorm(query);
  static const char *stop[] = {"lagu", "putar", "musik", "dari", "yang", "judul", "tolong", "dong", "ya", "saja",
                               "kartu", "memori", "memory", "sd", "card", "offline", "mainkan", "setel", "berjudul"};
  int best = -1, bestScore = 0;
  for (size_t i = 0; i < sdList.size(); i++) {
    String t = sdNorm(sdTitle(sdList[i]));
    int score = 0, from = 0;
    while (from < (int)q.length()) {
      int sp = q.indexOf(' ', from);
      String w = sp < 0 ? q.substring(from) : q.substring(from, sp);
      from = sp < 0 ? q.length() : sp + 1;
      if (w.length() < 3) continue;
      bool st = false;
      for (const char *x : stop) if (w == x) { st = true; break; }
      if (st) continue;
      if (t.indexOf(w) >= 0) score += w.length();
    }
    if (score > bestScore) { bestScore = score; best = (int)i; }
  }
  return best;
}

static int sdNextIdx(int i) {
  int n = (int)sdList.size();
  if (n <= 1) return 0;
  if (sdShuffle) { int j; do { j = esp_random() % n; } while (j == i); return j; }
  return (i + 1) % n;
}
static int sdPrevIdx(int i) {
  int n = (int)sdList.size();
  return n <= 1 ? 0 : (i + n - 1) % n;
}

// ---------- gestur sensor sentuh ----------
struct SdGesture {
  bool wasPr = false;
  uint32_t pressAt = 0, lastRel = 0;
  uint8_t taps = 0;
};

// Mengembalikan: SDR_CONT = lanjut, SDR_NEXT/PREV/STOP/TALK. *toggle = true kalau diminta jeda/lanjut.
static SdRes sdPoll(SdGesture &g, bool *toggle) {
  uint32_t now = millis();
  bool pr = hwTalkPressed();
  if (pr && !g.wasPr) { g.wasPr = true; g.pressAt = now; }
  if (pr && g.wasPr && now - g.pressAt > 600) return SDR_TALK;  // tahan = bicara
  if (!pr && g.wasPr) {
    g.wasPr = false;
    if (now - g.pressAt > 30) { g.taps++; g.lastRel = now; }
  }
  if (g.taps && !pr && now - g.lastRel > 420) {
    uint8_t t = g.taps;
    g.taps = 0;
    if (t == 1) *toggle = true;
    else if (t == 2) { faceEvent(BH_RIPPLE); return SDR_NEXT; }
    else if (t == 3) { faceEvent(BH_RIPPLE); return SDR_PREV; }
    else return SDR_STOP;
  }
  if (padStop) { padStop = false; return SDR_STOP; }
  if (zapReq) { int8_t z = zapReq; zapReq = 0; return z > 0 ? SDR_NEXT : SDR_PREV; }
  if (volAdjActive) replyVolume = VOL_GAIN[constrain((int)volAdjLevel, 1, 10)];
  if (alarmOn) {
    static uint32_t alChk = 0;
    struct tm tm;
    if (now - alChk > 1000 && (alChk = now, getLocalTime(&tm, 0)) && tm.tm_hour == alarmH && tm.tm_min == alarmM &&
        !(alarmRep == 2 && (tm.tm_wday == 0 || tm.tm_wday == 6)))
      return SDR_ALARM;
  }
  return SDR_CONT;
}

// ---------- memutar satu lagu ----------
static void sdShowPlaying(const String &title) {
  setExpr("senang", "diam");
  setText(title, "Ketuk: 1x jeda  2x lanjut lagu  3x mundur  4x stop");
  setAct(ACT_TALK);
}

static SdRes sdPlayTrack(int idx, uint32_t startByte, uint32_t *endByte) {
  *endByte = 0;
  if (idx < 0 || idx >= (int)sdList.size()) return SDR_FAIL;
  File f = SD.open(sdList[idx]);
  if (!f) return SDR_FAIL;
  const size_t INBUF = 16 * 1024;
  uint8_t *in = (uint8_t *)malloc(INBUF);
  int16_t *pcm = (int16_t *)malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(int16_t));
  mp3dec_t *dec = (mp3dec_t *)malloc(sizeof(mp3dec_t));
  if (!in || !pcm || !dec) { free(in); free(pcm); free(dec); f.close(); return SDR_FAIL; }
  mp3dec_init(dec);
  if (startByte) f.seek(startByte);

  String title = sdTitle(sdList[idx]);
  size_t fsize = f.size();
  size_t have = 0, off = 0;
  bool eof = false, started = false, paused = false;
  uint32_t playedMs = 0;
  uint32_t durSec = 0;
  int16_t out[BLOCK];
  size_t on = 0;
  uint32_t rsPos = 0;
  int32_t rsAcc = 0;
  int rsCnt = 0;
  SdGesture g;
  SdRes res = SDR_END;
  int badFrames = 0;

  auto bytePos = [&]() -> uint32_t { return (uint32_t)(f.position() - (have - off)); };
  auto doPoll = [&]() -> SdRes {
    bool tg = false;
    SdRes r = sdPoll(g, &tg);
    if (tg) {
      paused = !paused;
      faceEvent(paused ? BH_ZZZ : BH_CHECK);
      if (paused) {
        infoMusicEnd();
        setExpr("normal", "diam");
        setText("Dijeda", title);
        setAct(ACT_IDLE);
        audioLevel = 0;
      } else {
        infoMusicStart(false, title.c_str(), durSec, playedMs / 1000, 0, 0x5D00 + idx);
        sdShowPlaying(title);
      }
    }
    return r;
  };
  auto flush = [&]() -> SdRes {
    playScaled(out, on);
    on = 0;
    return doPoll();
  };

  while (true) {
    if (!eof && have - off < 8192) {  // isi buffer baca
      if (off) { memmove(in, in + off, have - off); have -= off; off = 0; }
      int r = f.read(in + have, INBUF - have);
      if (r <= 0) eof = true; else have += r;
    }
    if (have - off == 0 && eof) { res = SDR_END; break; }

    mp3dec_frame_info_t info;
    int samples = mp3dec_decode_frame(dec, in + off, (int)(have - off), pcm, &info);
    if (info.frame_bytes <= 0) {  // data tidak cukup / sampah di akhir
      if (eof) { res = SDR_END; break; }
      off = have;  // buang dan baca lagi
      if (++badFrames > 50) { res = SDR_FAIL; break; }
      continue;
    }
    off += info.frame_bytes;
    if (samples <= 0) { if (++badFrames > 400) { res = SDR_FAIL; break; } continue; }  // ID3 / data rusak dilewati
    badFrames = 0;

    if (!started) {
      started = true;
      uint32_t kbps = info.bitrate_kbps > 0 ? info.bitrate_kbps : 128;
      durSec = (uint32_t)((uint64_t)fsize * 8 / (kbps * 1000UL));
      musicPlaying = true;
      infoMusicStart(false, title.c_str(), durSec, startByte ? (uint32_t)((uint64_t)startByte * 8 / (kbps * 1000UL)) : 0,
                     0, 0x5D00 + idx);
      sdShowPlaying(title);
    }

    int ch = info.channels < 1 ? 1 : info.channels;
    uint32_t hz = info.hz > 0 ? info.hz : 44100;
    for (int i = 0; i < samples; i++) {
      int32_t s = ch == 2 ? ((int32_t)pcm[2 * i] + pcm[2 * i + 1]) / 2 : pcm[i];
      rsAcc += s;
      rsCnt++;
      rsPos += SAMPLE_RATE;
      if (rsPos >= hz) {  // satu sampel keluar = rata-rata sampel sumber di sela-selanya (penurun laju sederhana)
        rsPos -= hz;
        out[on++] = (int16_t)(rsAcc / rsCnt);
        rsAcc = 0;
        rsCnt = 0;
        if (on == BLOCK) {
          SdRes r = flush();
          if (r != SDR_CONT) { res = r; goto done; }
          while (paused) {  // dijeda: tunggu ketukan berikutnya
            delay(8);
            r = doPoll();
            if (r != SDR_CONT) { res = r; goto done; }
          }
        }
      }
    }
    playedMs += (uint32_t)((uint64_t)samples * 1000 / hz);
    {
      SdRes r = doPoll();
      if (r != SDR_CONT) { res = r; break; }
      while (paused) {
        delay(8);
        r = doPoll();
        if (r != SDR_CONT) { res = r; goto done; }
      }
    }
  }
done:
  *endByte = bytePos();
  free(in);
  free(pcm);
  free(dec);
  f.close();
  audioLevel = 0;
  return res;
}

// ---------- sesi musik SD ----------
// Mengembalikan alasan berhenti (SDR_TALK = tahan sensor: biarkan loop utama langsung merekam).
static SdRes sdRun(int idx, uint32_t startByte) {
  sdActive = true;
  faceEvent((esp_random() & 1) ? BH_DISCO : BH_EQ);
  musicPlaying = true;
  SdRes res = SDR_STOP;
  int fails = 0;
  while (true) {
    uint32_t endB = 0;
    sdIdx = idx;
    SdRes r = sdPlayTrack(idx, startByte, &endB);
    startByte = 0;
    if (r == SDR_FAIL) {
      if (++fails >= (int)sdList.size()) { res = SDR_FAIL; sdSession = false; break; }
      idx = sdNextIdx(idx);
      continue;
    }
    fails = 0;
    if (r == SDR_END || r == SDR_NEXT) idx = sdNextIdx(idx);
    else if (r == SDR_PREV) idx = sdPrevIdx(idx);
    else if (r == SDR_STOP) { sdSession = false; res = r; break; }
    else {  // TALK / ALARM: simpan posisi supaya "lanjutkan lagunya" bisa meneruskan
      sdIdx = idx;
      sdResumeByte = endB;
      sdSession = true;
      res = r;
      break;
    }
    playSilence(60);
  }
  sdActive = false;
  musicPlaying = false;
  lastMusicEnd = millis();
  infoMusicEnd();
  return res;
}

// act: "putar:<judul>", "acak", "next", "prev", "lanjut", "stop", "" (mulai / lanjutkan daftar)
static SdRes sdCommand(const String &actIn) {
  String act = actIn;
  act.trim();
  if (act == "stop") {
    sdSession = false;
    toast(TI_OK, "Musik SD", "dihentikan", 1800);
    return SDR_STOP;
  }
  if (!sdInit()) {
    toast(TI_X, "Kartu SD", "tidak terbaca", 3000);
    errBeep();
    return SDR_FAIL;
  }
  if (sdList.empty()) {
    toast(TI_X, "Folder /musik", "kosong (MP3)", 3000);
    errBeep();
    return SDR_FAIL;
  }
  int idx = sdIdx;
  uint32_t startB = 0;
  sdShuffle = false;
  if (act == "acak") {
    sdShuffle = true;
    idx = esp_random() % sdList.size();
  } else if (act == "next") {
    idx = sdNextIdx(sdIdx);
  } else if (act == "prev") {
    idx = sdPrevIdx(sdIdx);
  } else if (act == "lanjut" || act.length() == 0) {
    if (sdSession) startB = sdResumeByte;
  } else if (act.startsWith("putar:")) {
    String q = act.substring(6);
    if (q.indexOf("acak") >= 0) { sdShuffle = true; q.replace("acak", ""); }
    q.trim();
    if (q.length()) {
      int f = sdFind(q);
      if (f < 0) {
        toast(TI_X, "Judul tidak ada", q.c_str(), 3000);
        errBeep();
        return SDR_FAIL;
      }
      idx = f;
    } else if (sdShuffle) {
      idx = esp_random() % sdList.size();
    }
  }
  sdSession = false;
  return sdRun(idx, startB);
}
