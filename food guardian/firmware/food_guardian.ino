// Food Guardian: klasifikasi eksperimental + Blynk
// Rahasia (token, Wi-Fi) ada di secrets.h (tidak di-upload ke GitHub).
// Salin secrets.example.h menjadi secrets.h lalu isi nilainya.

// ===== Blynk (harus sebelum #include Blynk) =====
#define BLYNK_TEMPLATE_ID   "TMPL6pW0R9y2H"
#define BLYNK_TEMPLATE_NAME "food guardian"   // samakan dengan Blynk Console
#include "secrets.h"                          // BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <DHT.h>

#define PAKAI_BME280 true
#define PAKAI_DHT22  true
#define PAKAI_MQ135  true
#define PAKAI_LED    true

const bool MODE_CSV = false;   // true untuk merekam data

// ===== Pin =====
#define PIN_SDA   21
#define PIN_SCL   22
#define PIN_DHT   4
#define PIN_MQ    34
#define PIN_LED_M 25   // merah = SIAP
#define PIN_LED_K 26   // kuning = pemanasan / belum siap
#define PIN_BTN   0    // tombol BOOT: kunci baseline

// ===== Waktu =====
const unsigned long TICK_MS          = 1000;
const int           TICK_PER_JENDELA = 15;
const unsigned long WARMUP_DETIK     = 1200;  // 20 menit
const unsigned long KEDIP_MS         = 400;
const unsigned long COBA_KONEKSI_MS  = 30000; // coba sambung Blynk tiap 30 detik

// ===== MQ-135 =====
const int   JUMLAH_SAMPEL    = 16;
const int   JENDELA_MQ       = 8;
const float BATAS_VARIASI_MQ = 30.0;
const int   BATAS_GAGAL_DHT  = 2;
const float ADC_JENUH        = 4050;

// ===== PATOKAN SEMENTARA (belum divalidasi, dari 1-2 sesi per kelas) =====
const float BATAS_MENTAH_MATANG = 125.0;  // delta ADC
const float BATAS_MATANG_BUSUK  = 410.0;  // delta ADC
const float BATAS_RH_ADA_SAMPEL = 10.0;   // delta RH BME (poin %)

enum Status { BELUM, PEMANASAN_S, SIAP, BELUM_STABIL, JENUH, GAGAL };

const char* namaStatus(Status s) {
  switch (s) {
    case PEMANASAN_S:  return "PEMANASAN";
    case SIAP:         return "SIAP";
    case BELUM_STABIL: return "BELUM_STABIL";
    case JENUH:        return "JENUH";
    case GAGAL:        return "GAGAL";
    default:           return "BELUM";
  }
}

Adafruit_BME280 bme;
DHT dht(PIN_DHT, DHT22);
bool bmeTerdeteksi = false;

float sDhtT = 0, sDhtH = 0;            int nDht = 0;
float sBmeT = 0, sBmeH = 0, sBmeP = 0; int nBme = 0;
float sMqAdc = 0, sMqMv = 0;           int nMq  = 0;

float aDhtT = NAN, aDhtH = NAN, aBmeT = NAN, aBmeH = NAN, aBmeP = NAN;
float aMqAdc = NAN, aMqMv = NAN;

int   dhtJendelaGagal = 0;
float riwayatMq[JENDELA_MQ];
int   idxMq = 0, jumlahRiwayatMq = 0;

Status stBME = BELUM, stDHT = BELUM, stMQ = BELUM;
Status stBMEsebelum = BELUM, stDHTsebelum = BELUM, stMQsebelum = BELUM;

// Baseline wadah kosong
bool  baselineAda = false;
float baseAdc = NAN, baseRH = NAN;
bool  tombolSebelum = false;

unsigned long tickKe = 0;
unsigned long waktuTickBerikut = 0;
unsigned long terakhirCobaKoneksi = 0;

void cetakNilai(const char* label, float v, int desimal, const char* satuan) {
  if (isnan(v)) { Serial.printf("%-16s: GAGAL BACA\n", label); return; }
  char buf[16];
  dtostrf(v, 0, desimal, buf);
  for (char* p = buf; *p; p++) if (*p == '.') *p = ',';
  Serial.printf("%-16s: %s %s\n", label, buf, satuan);
}

void resetAkumulator() {
  sDhtT = sDhtH = 0; nDht = 0;
  sBmeT = sBmeH = sBmeP = 0; nBme = 0;
  sMqAdc = sMqMv = 0; nMq = 0;
}

bool semuaSiap() {
  bool mqOk = (stMQ == SIAP || stMQ == JENUH);
  return (!PAKAI_BME280 || stBME == SIAP) &&
         (!PAKAI_DHT22  || stDHT == SIAP) &&
         (!PAKAI_MQ135  || mqOk);
}
bool adaGagal() {
  return (PAKAI_BME280 && stBME == GAGAL) ||
         (PAKAI_DHT22  && stDHT == GAGAL) ||
         (PAKAI_MQ135  && stMQ  == GAGAL);
}
const char* teksSistem() {
  return adaGagal() ? "ADA SENSOR GAGAL"
                    : (semuaSiap() ? "SEMUA SIAP" : "BELUM SEMUA SIAP");
}

// ===== Klasifikasi eksperimental =====
const char* klasifikasi(float dMQ, float dRH, bool& ragu) {
  ragu = false;
  if (!baselineAda) return "BASELINE BELUM DIKUNCI";
  if (isnan(aMqAdc)) return "DATA MQ TIDAK ADA";
  if (aMqAdc >= ADC_JENUH) return "BUSUK (sensor jenuh)";
  bool rhNaik = !isnan(dRH) && dRH >= BATAS_RH_ADA_SAMPEL;
  if (dMQ >= BATAS_MATANG_BUSUK)  { ragu = !rhNaik; return "BUSUK"; }
  if (dMQ >= BATAS_MENTAH_MATANG) { ragu = !rhNaik; return "MATANG"; }
  if (rhNaik) return "MENTAH";
  return "WADAH KOSONG / TANPA SAMPEL";
}

// ===== Kunci baseline (dipanggil dari BOOT, Serial 'b', atau switch Blynk V18) =====
void kunciBaseline() {
  if (stMQ == SIAP && !isnan(aMqAdc) && !isnan(aBmeH)) {
    baseAdc = aMqAdc; baseRH = aBmeH; baselineAda = true;
    Serial.printf("# BASELINE DIKUNCI: ADC=%.0f, RH BME=%.2f %% (pastikan wadah KOSONG)\n", baseAdc, baseRH);
  } else {
    Serial.printf("# BASELINE DITOLAK: status MQ135 = %s (harus SIAP), data belum tampil = %s\n",
                  namaStatus(stMQ), isnan(aMqAdc) ? "ya" : "tidak");
  }
}

// Switch di dashboard Blynk (V18)
BLYNK_WRITE(V18) {
  if (param.asInt() == 1) {
    kunciBaseline();
    Blynk.virtualWrite(V18, 0);   // kembalikan switch ke OFF
  }
}

void cekTombol() {
  bool tekan = (digitalRead(PIN_BTN) == LOW);
  if (tekan && !tombolSebelum) kunciBaseline();
  tombolSebelum = tekan;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'b' || c == 'B') kunciBaseline();
  }
}

void bacaSatuTick() {
#if PAKAI_DHT22
  if (tickKe % 2 == 0) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t) && t > -40 && t < 80 && h >= 0 && h <= 100) {
      sDhtT += t; sDhtH += h; nDht++;
    }
  }
#endif
#if PAKAI_BME280
  if (bmeTerdeteksi) {
    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0F;
    if (!isnan(t) && !isnan(h) && !isnan(p) &&
        t > -40 && t < 85 && h >= 0 && h <= 100 && p > 300 && p < 1100) {
      sBmeT += t; sBmeH += h; sBmeP += p; nBme++;
    }
  }
#endif
#if PAKAI_MQ135
  long totalRaw = 0, totalMv = 0;
  for (int i = 0; i < JUMLAH_SAMPEL; i++) {
    totalRaw += analogRead(PIN_MQ);
    totalMv  += analogReadMilliVolts(PIN_MQ);
    delay(5);
  }
  sMqAdc += totalRaw / (float)JUMLAH_SAMPEL;
  sMqMv  += totalMv  / (float)JUMLAH_SAMPEL;
  nMq++;
#endif
}

// ===== Kirim ke Blynk (nilai NaN tidak dikirim) =====
void kirimBlynk(float dMQ, float dRH, const char* label, bool ragu) {
  if (!Blynk.connected()) return;
  if (!isnan(aDhtT))  Blynk.virtualWrite(V0, aDhtT);
  if (!isnan(aDhtH))  Blynk.virtualWrite(V1, aDhtH);
  if (!isnan(aBmeT))  Blynk.virtualWrite(V2, aBmeT);
  if (!isnan(aBmeH))  Blynk.virtualWrite(V3, aBmeH);
  if (!isnan(aBmeP))  Blynk.virtualWrite(V4, aBmeP);
  if (!isnan(aMqAdc)) Blynk.virtualWrite(V5, (int)aMqAdc);
  if (!isnan(aMqMv))  Blynk.virtualWrite(V6, aMqMv);
  Blynk.virtualWrite(V8,  namaStatus(stBME));
  Blynk.virtualWrite(V9,  namaStatus(stDHT));
  Blynk.virtualWrite(V10, namaStatus(stMQ));
  Blynk.virtualWrite(V11, teksSistem());
  if (baselineAda) {
    if (!isnan(dMQ)) Blynk.virtualWrite(V12, dMQ);
    if (!isnan(dRH)) Blynk.virtualWrite(V13, dRH);
    Blynk.virtualWrite(V14, baseAdc);
    Blynk.virtualWrite(V15, baseRH);
  }
  Blynk.virtualWrite(V16, label);
  Blynk.virtualWrite(V17, ragu ? "meragukan: RH tidak naik (eksperimental)"
                               : "klasifikasi eksperimental, bukan untuk keputusan konsumsi");
}

void tampilkan(unsigned long detik) {
  float dMQ = baselineAda ? aMqAdc - baseAdc : NAN;
  float dRH = baselineAda ? aBmeH - baseRH : NAN;
  bool ragu;
  const char* label = klasifikasi(dMQ, dRH, ragu);

  if (MODE_CSV) {
    Serial.printf("%lu,%s,%s,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.0f,%.0f,%.0f,%.2f,%s%s\n",
                  detik, namaStatus(stBME), namaStatus(stDHT), namaStatus(stMQ),
                  aDhtT, aDhtH, aBmeT, aBmeH, aBmeP, aMqAdc, aMqMv,
                  dMQ, dRH, label, ragu ? " (meragukan)" : "");
  } else {
    Serial.printf("[waktu %lu s | rata-rata 15 detik] %s\n", detik, teksSistem());
    cetakNilai("Suhu DHT",       aDhtT,  2, "C");
    cetakNilai("Kelembapan DHT", aDhtH,  2, "%");
    cetakNilai("Suhu BME",       aBmeT,  2, "C");
    cetakNilai("Kelembapan BME", aBmeH,  2, "%");
    cetakNilai("Tekanan BME",    aBmeP,  2, "hPa");
    cetakNilai("MQ135 ADC",      aMqAdc, 0, "(relatif)");
    cetakNilai("MQ135 Pin",      aMqMv,  0, "mV");
    Serial.printf("%-16s: BME=%s | DHT=%s | MQ=%s\n", "Status sensor",
                  namaStatus(stBME), namaStatus(stDHT), namaStatus(stMQ));
    if (baselineAda) {
      cetakNilai("Delta MQ", dMQ, 0, "dari baseline");
      cetakNilai("Delta RH", dRH, 1, "poin dari baseline");
    }
    Serial.printf("%-16s: %s%s\n", "KLASIFIKASI", label, ragu ? " (meragukan: RH tidak naik)" : "");
    Serial.printf("%-16s: %s\n", "Blynk", Blynk.connected() ? "TERSAMBUNG" : "PUTUS");
    if (stMQ == BELUM_STABIL) Serial.println("Catatan         : MQ135 masih berubah, hasil bisa bergeser");
    Serial.println("(klasifikasi eksperimental, bukan untuk keputusan konsumsi)");
    Serial.println("----------------------------------------");
  }

  kirimBlynk(dMQ, dRH, label, ragu);
}

void akhiriJendela() {
  unsigned long detik = millis() / 1000;

  if (nDht > 0) {
    aDhtT = sDhtT / nDht; aDhtH = sDhtH / nDht;
    dhtJendelaGagal = 0; stDHT = SIAP;
  } else {
    aDhtT = NAN; aDhtH = NAN;
    if (++dhtJendelaGagal >= BATAS_GAGAL_DHT) stDHT = GAGAL;
  }

  if (nBme > 0) {
    aBmeT = sBmeT / nBme; aBmeH = sBmeH / nBme; aBmeP = sBmeP / nBme;
    stBME = SIAP;
  } else {
    aBmeT = aBmeH = aBmeP = NAN; stBME = GAGAL;
  }

  if (nMq > 0) {
    aMqAdc = sMqAdc / nMq;
    aMqMv  = sMqMv  / nMq;
    riwayatMq[idxMq] = aMqAdc;
    idxMq = (idxMq + 1) % JENDELA_MQ;
    if (jumlahRiwayatMq < JENDELA_MQ) jumlahRiwayatMq++;

    if (aMqAdc < 20) {
      stMQ = GAGAL;
    } else if (aMqAdc >= ADC_JENUH) {
      stMQ = JENUH;
    } else if (detik < WARMUP_DETIK) {
      stMQ = PEMANASAN_S;
    } else if (jumlahRiwayatMq < JENDELA_MQ) {
      stMQ = BELUM_STABIL;
    } else {
      float mn = riwayatMq[0], mx = riwayatMq[0];
      for (int i = 1; i < JENDELA_MQ; i++) {
        if (riwayatMq[i] < mn) mn = riwayatMq[i];
        if (riwayatMq[i] > mx) mx = riwayatMq[i];
      }
      stMQ = ((mx - mn) <= BATAS_VARIASI_MQ) ? SIAP : BELUM_STABIL;
    }
  }

  if (stBME != stBMEsebelum) { Serial.printf("# [%lus] BME280 -> %s\n", detik, namaStatus(stBME)); stBMEsebelum = stBME; }
  if (stDHT != stDHTsebelum) { Serial.printf("# [%lus] DHT22  -> %s\n", detik, namaStatus(stDHT)); stDHTsebelum = stDHT; }
  if (stMQ  != stMQsebelum)  { Serial.printf("# [%lus] MQ135  -> %s\n", detik, namaStatus(stMQ));  stMQsebelum  = stMQ;  }

  tampilkan(detik);
  resetAkumulator();
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("=== Food Guardian: Klasifikasi Eksperimental + Blynk ===");
  Serial.println("Langkah: wadah kosong -> tunggu SEMUA SIAP -> tekan BOOT / ketik b / switch Blynk -> masukkan mangga");

  pinMode(PIN_BTN, INPUT_PULLUP);
#if PAKAI_LED
  pinMode(PIN_LED_M, OUTPUT);
  pinMode(PIN_LED_K, OUTPUT);
  digitalWrite(PIN_LED_M, HIGH); digitalWrite(PIN_LED_K, HIGH);
  delay(500);
  digitalWrite(PIN_LED_M, LOW);  digitalWrite(PIN_LED_K, LOW);
#endif
#if PAKAI_BME280
  Wire.begin(PIN_SDA, PIN_SCL);
  bmeTerdeteksi = bme.begin(0x76, &Wire);
  Serial.println(bmeTerdeteksi ? "[BME280] terdeteksi di 0x76" : "[BME280] GAGAL terdeteksi");
#endif
#if PAKAI_DHT22
  dht.begin();
#endif
#if PAKAI_MQ135
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_MQ, ADC_11db);
  Serial.printf("[MQ135 ] pemanasan %lu detik dimulai...\n", WARMUP_DETIK);
#endif

  // Wi-Fi dan Blynk: sensor tetap jalan walau koneksi gagal
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Blynk.config(BLYNK_AUTH_TOKEN);
  Serial.println("[Wi-Fi ] menyambung di latar belakang...");

  if (MODE_CSV) {
    Serial.println("waktu_s,st_bme,st_dht,st_mq,dht_suhu_C,dht_rh,bme_suhu_C,bme_rh,bme_tekanan_hPa,mq_adc,mq_pin_mV,delta_mq,delta_rh,label");
  }
  waktuTickBerikut = millis() + TICK_MS;
}

void loop() {
  // Blynk (tidak memblokir bila Wi-Fi belum tersambung)
  if (WiFi.status() == WL_CONNECTED) {
    if (Blynk.connected()) {
      Blynk.run();
    } else if (millis() - terakhirCobaKoneksi > COBA_KONEKSI_MS) {
      terakhirCobaKoneksi = millis();
      Blynk.connect(3000);
    }
  }

  if ((long)(millis() - waktuTickBerikut) >= 0) {
    waktuTickBerikut += TICK_MS;
    tickKe++;
    bacaSatuTick();
    if (tickKe % TICK_PER_JENDELA == 0) akhiriJendela();
  }
  cekTombol();

#if PAKAI_LED
  if (adaGagal()) {
    bool fase = (millis() / KEDIP_MS) % 2;
    digitalWrite(PIN_LED_M, fase ? HIGH : LOW);
    digitalWrite(PIN_LED_K, fase ? LOW : HIGH);
  } else {
    digitalWrite(PIN_LED_M, semuaSiap() ? HIGH : LOW);
    digitalWrite(PIN_LED_K, semuaSiap() ? LOW : HIGH);
  }
#endif
}
