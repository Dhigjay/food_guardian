# Food Guardian

Prototipe IoT berbasis ESP32 yang memantau udara di dalam wadah penyimpanan: suhu, kelembapan, tekanan, dan respons gas relatif. Studi kasus: mangga mentah, matang, dan busuk.

> **Baca ini dulu.** Food Guardian adalah prototipe edukasi dan penelitian awal. Alat ini **tidak** menentukan apakah makanan aman atau layak dimakan. Label *mentah / matang / busuk* adalah klasifikasi eksperimental dengan patokan dari sedikit sampel (mangga berbeda tiap sesi) dan belum diuji pada data terpisah. Jangan memakainya untuk keputusan konsumsi.

---

## Isi

- [Apa yang dilakukan alat ini](#apa-yang-dilakukan-alat-ini)
- [Komponen](#komponen)
- [Rangkaian](#rangkaian)
- [Perangkat lunak](#perangkat-lunak)
- [Memasang dan mengunggah kode](#memasang-dan-mengunggah-kode)
- [Cara memakai](#cara-memakai)
- [Cara kerja klasifikasi](#cara-kerja-klasifikasi)
- [Indikator LED](#indikator-led)
- [Dashboard Blynk](#dashboard-blynk)
- [Data pengujian](#data-pengujian)
- [Keterbatasan](#keterbatasan)
- [Pemecahan masalah](#pemecahan-masalah)
- [Struktur repository](#struktur-repository)
- [Rencana pengembangan](#rencana-pengembangan)

---

## Apa yang dilakukan alat ini

Alat membaca tiga sensor di dalam wadah, merata-ratakan pembacaan setiap 15 detik, lalu menampilkan hasilnya di Serial Monitor dan dashboard Blynk.

Temuan yang didukung data sejauh ini: isi wadah mengubah udara di dalamnya. Dibanding wadah kosong, wadah berisi mangga menaikkan kelembapan dan respons MQ-135, dan kenaikannya cenderung searah dengan urutan mentah, matang, busuk. Temuan ini masih awal dan belum membuktikan bahwa sensor dapat membedakan tingkat kematangan.

Yang **tidak** dilakukan alat ini: mengukur konsentrasi gas (ppm), mengenali jenis gas, menyatakan kesegaran dalam persen, atau menilai keamanan makanan.

## Komponen

| Komponen | Fungsi |
|---|---|
| ESP32 WROOM (DevKit, dengan extension board S-V-G) | Mikrokontroler dan Wi-Fi |
| DHT22 (modul 3 pin) | Suhu dan kelembapan |
| BME280 (I2C, alamat 0x76) | Suhu, kelembapan, tekanan |
| MQ-135 (modul dengan pin AO) | Respons gas relatif |
| 2 resistor 10 kΩ | Pembagi tegangan keluaran AO MQ-135 |
| 2 resistor 220 Ω | Pembatas arus LED |
| LED merah dan kuning | Indikator status alat |
| Breadboard, kabel jumper | Perakitan |
| Wadah plastik bertutup (dengan lubang ventilasi kecil) | Ruang uji |

## Rangkaian

### Pin

| Perangkat | Pin perangkat | Pin ESP32 | Tegangan |
|---|---|---|---|
| DHT22 | `+` / `-` / `out` | 3V3 / GND / GPIO 4 | 3,3 V |
| BME280 | VIN / GND / SDA / SCL | 3V3 / GND / GPIO 21 / GPIO 22 | 3,3 V |
| MQ-135 | VCC / GND | VIN (5 V) / GND | 5 V |
| MQ-135 | AO | pembagi tegangan, lalu GPIO 34 | lihat di bawah |
| LED merah | anoda lewat 220 Ω / katoda | GPIO 25 / GND | |
| LED kuning | anoda lewat 220 Ω / katoda | GPIO 26 / GND | |
| Tombol baseline | tombol BOOT bawaan | GPIO 0 | |

### Pembagi tegangan MQ-135

Pemanas MQ-135 butuh 5 V, dan pin AO bisa mendekati 5 V, sedangkan input ESP32 maksimal 3,3 V. **Jangan sambungkan AO langsung ke GPIO.**

```
MQ-135 AO ──[ 10 kΩ ]──┬── GPIO 34
                       │
                     [ 10 kΩ ]
                       │
                      GND
```

Dengan dua resistor 10 kΩ, tegangan di pin turun menjadi setengah dari AO. GPIO 34 adalah pin ADC1 yang hanya bisa menjadi input, jadi tetap aman dipakai saat Wi-Fi aktif.

### Peringatan perakitan

- Cabut USB sebelum menyambung kabel.
- Pin V pada header extension board bertegangan 3,3 V atau 5 V tergantung jumper. Pastikan DHT22 dan BME280 berada di 3,3 V.
- Jangan sampai 5 V dari MQ-135 masuk ke jalur 3,3 V.
- LED selalu dengan resistor 220 Ω.
- Jauhkan MQ-135 dari DHT22 dan BME280. Pemanasnya menaikkan suhu di sekitarnya.

## Perangkat lunak

- Arduino IDE
- Board package **esp32 by Espressif Systems**
- Library (lewat Library Manager):
  - DHT sensor library (Adafruit)
  - Adafruit BME280 Library
  - Adafruit Unified Sensor
  - Blynk (Volodymyr Shymanskyy)

## Memasang dan mengunggah kode

1. **Siapkan folder sketch.** File `food_guardian.ino` harus berada di folder bernama `food_guardian`.
2. **Buat `secrets.h`.** Di folder yang sama, salin `secrets.example.h` menjadi `secrets.h`, lalu isi:

   ```cpp
   #define BLYNK_AUTH_TOKEN "token_dari_blynk"
   const char* WIFI_SSID = "nama_wifi_2_4_ghz";
   const char* WIFI_PASS = "password_wifi";
   ```

   File `secrets.h` sudah ada di `.gitignore`, jadi tidak ikut ter-upload. Jangan pernah mem-publish token atau password.
3. **Isi nama template Blynk.** Nilai `BLYNK_TEMPLATE_NAME` di `food_guardian.ino` harus sama persis dengan nama template di Blynk Console.
4. **Atur board.** Tools → Board → *ESP32 Dev Module*. Upload Speed: *115200*.
5. **Unggah.** Klik Upload. Jika muncul `Wrong boot mode detected`, lakukan ini: saat Output menampilkan `Connecting...`, tahan tombol **BOOT**, tekan sekali **EN**, dan tetap tahan BOOT sampai muncul `Writing at ...`.
6. Buka Serial Monitor pada **115200 baud**.

Wi-Fi harus 2,4 GHz. ESP32 tidak mendukung 5 GHz.

## Cara memakai

1. **Nyalakan dengan wadah kosong** dan jangan reset selama sesi. Pemanasan MQ-135 berlangsung 1200 detik (20 menit) sejak boot. Angka ini panduan umum untuk sensor MQ, bukan hasil ukur modul tertentu. Selama itu LED kuning menyala.
2. **Tunggu status `SEMUA SIAP`** dan angka ADC MQ-135 datar.
3. **Kunci baseline** saat wadah masih kosong. Pilih salah satu:
   - tekan tombol **BOOT**,
   - ketik `b` di Serial Monitor,
   - nyalakan switch **V18** di dashboard Blynk.

   Cari pesan `# BASELINE DIKUNCI: ADC=..., RH BME=...`. Jika muncul `# BASELINE DITOLAK`, baca status MQ135 yang tertulis dan tunggu sampai `SIAP`.
4. **Masukkan satu mangga**, tutup wadah, dan catat waktu masuk.
5. **Tunggu pembacaan stabil**, lalu baca `Delta MQ`, `Delta RH`, dan `KLASIFIKASI`. Gunakan jendela waktu yang sama untuk tiap sampel, karena ADC mentah dan matang naik pelan seiring waktu.
6. **Pulihkan sensor** antar sampel: buka wadah sampai `Delta MQ` kembali mendekati nol. Dari data pengujian, pemulihan setelah mangga matang sekitar 10 menit. Setelah mangga busuk bisa lebih lama.
7. **Urutan yang disarankan:** mentah, matang, busuk, supaya pemulihan terpanjang ada di akhir.

Baseline tidak disimpan permanen. Jika board restart, ulangi dari langkah 1.

### Contoh keluaran

Contoh format. Angkanya **ilustrasi, bukan hasil pengukuran**.

```
[waktu 1200 s | rata-rata 15 detik] SEMUA SIAP
Suhu DHT        : 24,20 C
Kelembapan DHT  : 59,60 %
Suhu BME        : 25,28 C
Kelembapan BME  : 45,00 %
Tekanan BME     : 1002,97 hPa
MQ135 ADC       : 2558 (relatif)
MQ135 Pin       : 2238 mV
Status sensor   : BME=SIAP | DHT=SIAP | MQ=SIAP
Delta MQ        : 0 dari baseline
Delta RH        : 0,0 poin dari baseline
KLASIFIKASI     : WADAH KOSONG / TANPA SAMPEL
Blynk           : TERSAMBUNG
(klasifikasi eksperimental, bukan untuk keputusan konsumsi)
```

### Mode CSV untuk merekam data

Ubah `const bool MODE_CSV = false;` menjadi `true`. Setiap 15 detik alat mencetak satu baris CSV (titik desimal). Mode ini lebih cocok untuk data eksperimen karena tidak ada baris yang hilang saat disalin.

```
waktu_s,st_bme,st_dht,st_mq,dht_suhu_C,dht_rh,bme_suhu_C,bme_rh,bme_tekanan_hPa,mq_adc,mq_pin_mV,delta_mq,delta_rh,label
```

Baris yang diawali `#` adalah pesan status, bukan data. Abaikan saat mengimpor (misalnya `comment='#'` di pandas).

## Cara kerja klasifikasi

Patokan memakai **selisih terhadap baseline wadah kosong**, bukan nilai ADC mutlak, karena baseline MQ-135 bergeser antar hari.

- `ΔMQ` = rata-rata ADC 15 detik dikurangi ADC baseline
- `ΔRH` = RH BME280 dikurangi RH baseline (DHT22 tidak dipakai untuk klasifikasi)

| Kondisi | Label |
|---|---|
| ADC ≥ 4050 (sensor jenuh) | BUSUK (sensor jenuh) |
| ΔMQ ≥ +410 | BUSUK |
| ΔMQ ≥ +125 | MATANG |
| ΔMQ < +125 dan ΔRH ≥ +10 poin | MENTAH |
| selain itu | WADAH KOSONG / TANPA SAMPEL |

Jika ΔMQ tinggi tetapi ΔRH tidak naik, hasil ditandai *meragukan*.

Batas +125 dan +410 adalah titik tengah antar rata-rata kelas dari data awal (mentah 1 sesi, matang 2 sesi, busuk 2 sesi). **Angka ini sementara dan belum divalidasi.** Ubah `BATAS_MENTAH_MATANG`, `BATAS_MATANG_BUSUK`, dan `BATAS_RH_ADA_SAMPEL` di kode setelah ada data ulangan. Menyetel batas agar cocok dengan beberapa mangga demo berarti menyesuaikan alat ke sampel itu, bukan membuktikan akurasi.

Status sensor MQ-135:

| Status | Arti |
|---|---|
| `PEMANASAN` | Masih dalam 1200 detik pertama |
| `SIAP` | Pembacaan tenang (selisih ADC maks−min ≤ 30 dalam 8 rata-rata terakhir) |
| `BELUM_STABIL` | Pembacaan masih berubah |
| `JENUH` | ADC ≥ 4050, rentang baca habis |
| `GAGAL` | ADC di bawah 20 |

`SIAP` berarti pembacaan **tenang**, bukan akurat dan bukan sudah kembali ke baseline. Pembacaan bisa tenang di level yang masih tinggi.

## Indikator LED

| Kondisi | Kuning | Merah |
|---|---|---|
| Ada sensor belum siap (pemanasan atau belum stabil) | menyala | mati |
| Semua sensor siap | mati | menyala |
| Ada sensor gagal | berkedip bergantian | berkedip bergantian |

LED hanya menunjukkan status alat, bukan kondisi makanan.

## Dashboard Blynk

Blynk dipakai untuk **pemantauan**. Data eksperimen sebaiknya disimpan dari Serial (mode CSV), karena Blynk bisa kehilangan data saat Wi-Fi putus dan batas penyimpanan paketnya perlu dicek di situs resmi. Jika Wi-Fi atau Blynk gagal, sensor dan klasifikasi tetap berjalan.

| Pin | Nama | Tipe | Isi |
|---|---|---|---|
| V0 | DHT_Temperature | Double | Suhu DHT22 (°C) |
| V1 | DHT_Humidity | Double | RH DHT22 (%) |
| V2 | BME_Temperature | Double | Suhu BME280 (°C) |
| V3 | BME_Humidity | Double | RH BME280 (%) |
| V4 | BME_Pressure | Double | Tekanan (hPa) |
| V5 | MQ135_ADC | Integer | ADC MQ-135 (angka utama) |
| V6 | MQ135_Pin_mV | Double | Tegangan di pin (mV) |
| V8 | Status_BME | String | Status BME280 |
| V9 | Status_DHT | String | Status DHT22 |
| V10 | Status_MQ135 | String | Status MQ-135 |
| V11 | Status_Sistem | String | SEMUA SIAP / BELUM SEMUA SIAP / ADA SENSOR GAGAL |
| V12 | Delta_MQ | Double | ΔMQ |
| V13 | Delta_RH | Double | ΔRH (poin %) |
| V14 | Baseline_ADC | Double | ADC baseline |
| V15 | Baseline_RH | Double | RH baseline |
| V16 | Klasifikasi | String | Label eksperimental |
| V17 | Catatan_Klasifikasi | String | Catatan dan peringatan |
| V18 | Kunci_Baseline | Integer | Switch untuk mengunci baseline |

V7 (estimasi tegangan AO) sudah tidak dikirim karena nilainya pernah melebihi 5 V dan tidak dapat dipercaya.

Letakkan teks statis di dashboard dekat widget V16: *"Klasifikasi eksperimental, bukan untuk keputusan konsumsi."* Jangan menamai datastream apa pun dengan kata "Food", "Fresh", atau "Safe".

## Data pengujian

Ringkasan hasil dari sesi pengukuran aktual. Setiap kelas memakai mangga yang berbeda dan baseline dari sesinya sendiri. Berkas mentah ada di folder `data/`.

| Kondisi | Sesi | ΔMQ (ADC) | Catatan |
|---|---|---|---|
| Wadah kosong | 3 baseline bersih | sekitar 0 | ADC 2535 sampai 2563, variasi sekitar ±10 sampai 20 |
| Mentah | 1 | sekitar −70 | Tidak naik, praktis sama dengan baseline |
| Matang | 2 | sekitar +305 sampai +340 | Konsisten antar sesi |
| Busuk | 2 | sekitar +500 dan ≥ +1500 | Satu sesi jenuh (ADC 4095), satu tidak. Dua sampel tidak setara |

Hal yang perlu diperhatikan saat membaca tabel ini:

- Jumlah sesi sangat kecil, dan tiap kelas memakai mangga berbeda. Selisih antar kelas bercampur dengan selisih antar mangga.
- Kelembapan naik pada ketiga kelas (sekitar +20 sampai +36 poin pada BME280), jadi RH **bukan** pembeda kelas. RH hanya dipakai untuk membedakan mentah dari wadah kosong, dan dasarnya lemah.
- ADC jenuh membuat besar respons busuk tidak terukur.
- Hasil gladi klasifikasi pada tiga mangga demo (mentah, matang, busuk) belum dilampirkan di repository ini. Tambahkan angka `Delta MQ`, `Delta RH`, dan label tiap mangga ke `data/` agar hasilnya dapat diperiksa.

Tidak ada angka akurasi pada proyek ini karena belum ada pengujian pada data yang tidak dipakai menentukan batas.

## Keterbatasan

- MQ-135 bukan sensor khusus pembusukan. Ia merespons banyak uap (alkohol, amonia, VOC, asap), dan dipengaruhi suhu serta kelembapan. Nilainya relatif, bukan ppm.
- Kenaikan ADC tidak dapat dipisahkan dari pengaruh RH dan suhu dengan alat ini.
- Baseline MQ-135 bergeser antar hari (sekitar 2010, 2476, dan 2550 pada sesi berbeda). Baseline harus diukur ulang di tiap sesi, setelah ADC datar.
- Pemulihan sensor setelah paparan butuh puluhan menit. Mengukur berurutan tanpa jeda membuat sisa paparan ikut terbaca.
- Sensor jenuh pada gas pekat, dan jenuh tidak berarti mangga busuk.
- DHT22 dan BME280 berselisih sekitar 10 sampai 18 %RH. Belum ada pembanding terkalibrasi, jadi analisis tren tiap sensor, jangan merata-ratakannya. DHT22 mendekati batas pada RH di atas sekitar 95%.
- Kriteria `SIAP` (batas variasi 30 ADC) adalah tebakan awal, belum divalidasi.
- Waktu dihitung sejak boot, belum jam nyata. Belum ada penyimpanan data otomatis.
- Klasifikasi belum diuji pada sampel di luar yang dipakai menyusun patokan.

## Pemecahan masalah

| Gejala | Kemungkinan penyebab dan tindakan |
|---|---|
| `Wrong boot mode detected (0x17)` saat upload | Tahan BOOT, tekan EN, lepaskan EN, tahan BOOT sampai `Writing at` |
| Upload putus setelah baud naik | Turunkan Upload Speed ke 115200, ganti kabel USB yang pendek dan berkualitas, cabut MQ-135 sementara |
| `secrets.h: No such file or directory` | `secrets.h` belum ada di folder sketch, atau namanya masih `secrets.example.h` |
| Error kompilasi aneh pada konstanta | Periksa tanda baca nyasar di akhir baris |
| Label `BASELINE BELUM DIKUNCI` terus | Baseline tidak otomatis. Tekan BOOT, ketik `b`, atau pakai switch V18, setelah MQ135 `SIAP` |
| `# BASELINE DITOLAK` | Status MQ135 belum `SIAP`. Tunggu, jangan paksa |
| Wadah berisi mangga terbaca `WADAH KOSONG` | Baseline terkunci saat ADC masih tinggi, mangga belum cukup lama di wadah tertutup, atau mangga berbeda dari data patokan. Periksa nilai `Delta MQ` dan `Delta RH` |
| Status `JENUH` | Uap pekat, sensor belum pulih, wadah lembap tertutup rapat, atau sensitivitas modul terlalu tinggi. Jika jenuh juga di wadah kosong, periksa pembagi tegangan |
| `Blynk : PUTUS` | Wi-Fi 5 GHz, SSID atau password salah, token salah, atau nama template tidak cocok |
| DHT22 selalu `GAGAL BACA` | Pin `out` salah atau longgar, polaritas `+`/`−` terbalik |
| `BME280 GAGAL terdeteksi` | SDA/SCL tertukar, VIN bukan 3,3 V, atau alamat bukan 0x76 |
| Board restart sendiri | Daya USB kurang (kabel atau port), terutama saat Wi-Fi menyala |

## Struktur repository

```
food-guardian/
├── firmware/
│   └── food_guardian/
│       ├── food_guardian.ino
│       └── secrets.example.h
├── data/              data pengukuran aktual (CSV atau salinan Serial Monitor)
├── docs/              foto rangkaian, tabel pin, screenshot dashboard
├── README.md
├── .gitignore
└── LICENSE
```

Hanya data pengukuran aktual yang boleh masuk ke `data/`. Data ilustrasi tidak disertakan.

## Rencana pengembangan

- Ulangi pengukuran minimal 3 mangga per kelas, dengan baseline tepat sebelum tiap sesi dan jendela waktu baca yang tetap.
- Catat pengamatan visual terstruktur (foto, skor 1 sampai 5, ukuran, berat, lama di wadah) dengan kriteria yang ditulis sebelum mengukur.
- Pantau satu mangga yang sama dari hari ke hari sampai membusuk.
- Pisahkan data penentu batas dan data uji, lalu laporkan tabel benar-salah.
- Atasi saturasi: turunkan sensitivitas modul atau perluas rentang baca.
- Tambahkan penanda waktu nyata dan penyimpanan otomatis (kartu SD atau Google Sheets).
- Tambahkan status "di atas baseline" agar `SIAP` juga memeriksa kedekatan dengan baseline.
- ESP32-CAM untuk dokumentasi visual (tertunda).

## Lisensi

Kode dirilis di bawah [lisensi MIT](LICENSE). Lisensi ini hanya mencakup kode. Peringatan di bagian atas README tetap berlaku: alat ini tidak untuk menentukan keamanan konsumsi.
