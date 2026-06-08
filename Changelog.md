# **CHANGELOG \- VOTOL SMART DASHBOARD & IMMOBILIZER**

Dokumen ini mencatat riwayat pembaruan, perbaikan bug, peningkatan sistem keamanan, serta optimasi performa pada perangkat Votol Smart Dashboard & Immobilizer (ESP32 & Web UI).

## **\[V15.48-beta\] \- Pembaruan Masa Tenggang Shutdown, Self-Healing Scan Latar Belakang & Failsafe Ganda**

### **Ditambahkan (Added)**

* **Penyempurnaan 1: Masa Tenggang Shutdown 10 Detik (Grace Period Countdown):**  
  * Dibandingkan langsung memutus relay utama (SSR GPIO20) saat gantungan kunci (*Tag*) BLE menjauh, sistem kini memberikan waktu tunggu 10 detik penuh sebelum kelistrikan utama dipadamkan.  
  * Selama masa tenggang, GPIO20 tetap dipaksa bernilai *HIGH* (kelistrikan menyala stabil tanpa ada kedipan jatuh daya). Layar OLED akan memunculkan peringatan darurat kedip-kedip cepat hitung mundur: "KEYLESS ALERT \- LOCKING SYSTEM IN \[X\]S".  
  * Jika Tag kembali terdeteksi sebelum hitung mundur selesai, alarm warning akan langsung dibatalkan secara otomatis, layar kembali normal, dan motor tetap menyala tanpa interupsi daya.  
* **Penyempurnaan 2: Continuous Scan Latar Belakang dengan Self-Healing (Loop 3 Detik):**  
  * Mengubah metode pemindaian tak terbatas (*infinite scan*) yang rawan macet (*silent scan death*) akibat tabrakan modul radio dengan stream data telemetri.  
  * Proses pemindaian kini berjalan secara asinkron dalam durasi 3 detik sekali. Begitu selesai, memori pindaian langsung dibersihkan otomatis (clearResults()) demi mencegah kebocoran memori RAM, lalu pemindaian otomatis dimulai kembali secara mandiri.  
* **Penyempurnaan 3: Failsafe Watchdog Pemindai (6 Detik):**  
  * Menambahkan pengawas internal (*watchdog protection failsafe*). Jika status pemindaian terdeteksi menggantung/membeku melebihi 6 detik akibat interupsi GATT Server HP, sistem akan memaksa pemindai berhenti (stop()) dan mengatur ulang statusnya agar loop pemindaian dapat langsung berjalan kembali secara normal.  
* **Penyempurnaan 4: Sistem Motion Safety Lock:**  
  * Perlindungan berkendara mutlak yang melarang sistem memutus relay SSR utama (GPIO20) atau mematikan OLED saat motor terdeteksi sedang melaju atau dinamo berputar (speed\_kmh \> 0 || rpm \> 0\) berdasarkan data dinamis CAN Bus.  
  * Jika Anda sedang berkendara dan Tag jatuh ke jalan, kelistrikan motor tetap menyala stabil demi menjamin keselamatan jiwa Anda.  
* **Penyempurnaan 5: OLED Auto-Hibernate Terintegrasi:**  
  * Layar OLED akan mati total fisik secara menyeluruh (SSD1306\_DISPLAYOFF) saat relay utama padam (relayState \== false) ketika motor terparkir aman. Otomatis menyala kembali (SSD1306\_DISPLAYON) saat Tag mendekat.  
* **Penyempurnaan 6: Popup Cruise Control 3 Detik:**  
  * Deteksi perubahan status dinamis (*rising-edge*) pada sinyal Cruise Control (cruiseActive) di parser CAN. Saat cruise diaktifkan, layar OLED memunculkan popup *"CRUISE ACTIVE \- SPEED LOCKED"* selama 3 detik lengkap dengan buzzer beep feedback fisik.  
* **Penyempurnaan 7: Warning Standar Samping Berkedip Penuh:**  
  * Popup berkedip cepat *"STAND DOWN\! \- LIFT STAND TO RIDE"* yang mengunci layar utama OLED secara konstan selama standar samping masih turun (standActive \== 1), mencegah potensi bahaya mengemudi dengan standar samping yang menggantung.

### **Dihapus (Removed)**

* **Active Ping Handshake:** Menghapus metode koneksi langsung jabat tangan (*active ping connection*) ke alamat MAC Tag yang terbukti menyebabkan tabrakan frekuensi antena (*RF antenna sharing conflict*) dengan stream telemetri BLE Server ke HP.

## **\[V15.45-beta\] \- Integrasi Pencarian BLE Sisi Smartphone & Sinkronisasi Dua Arah**

### **Ditambahkan (Added)**

* **Arsitektur Web UI Modular (Pemisahan Berkas Berstandar Bersih):**  
  * Mengembalikan sistem Web UI ke dalam bentuk modular terpisah demi kestabilan, keterbacaan, dan kemudahan pemeliharaan:  
    * index.html: Shell induk pemuat navigasi, status bar kustom, jam RTC, dan tombol koneksi.  
    * dash.html: Panel spidometer bundar, RPM, pengukur suhu 3 titik, indikator lencana transisi, dan GPX Record.  
    * battery.html: Penampil diagram detail kesehatan baterai dan monitoring 23 sel baterai individual.  
    * trip.html: Jarak tempuh, efisiensi Wh/km, estimasi range, dan grafik diagnostik Chart.js.  
    * settings.html: Pengaturan splashname, sinkronisasi jam, batas alarm kecepatan, kalibrasi ban, dan kendali keyless.  
    * logs.html: Aliran baris data JSON CAN Bus secara live.  
    * app.js: Otak logika pusat pengontrol navigasi, penangkap notifikasi BLE, pemindaian, dan render grafik.  
    * style.css: File CSS global yang menyajikan tema gelap (*dark mode*) modern yang responsif.  
* **Pencarian BLE di Sisi Smartphone (Metode A):**  
  * Memindahkan beban scanning Tag BLE sepenuhnya ke ponsel (melalui Web Bluetooth API pada Chrome) saat menekan tombol "PINDAI PERANGKAT DARI HP" di pengaturan, guna menjamin kebebasan radio ESP32 dari beban pindaian berlebih.  
* **Input MAC Manual (Metode Failsafe B):**  
  * Menyediakan kolom input teks di settings.html untuk mengetik alamat MAC Tag BLE secara langsung dan mengirimkannya ke NVS Preferences ESP32 layaknya splashscreen kustom, sangat berguna sebagai failsafe jika Tag menggunakan sistem proteksi privasi MAC.  
* **Ekstraksi Logika Bitwise Transisi Votol (Sniffing ID 0x0A010810):**  
  * Mendeteksi status transisi tombol fisik dengan membaca Low-Nibble (4-bit bawah) pada Byte\[1\]: Cruise Control (Bit 2 \- 0x04), Tuas Rem (Bit 1 \- 0x02), dan Standar Samping (Bit 3 \- 0x08).

### **Diperbaiki (Fixed)**

* **Kompilasi Sukses Arsitektur Modular ESP32:**  
  * Menggunakan kata kunci extern pada seluruh variabel global di Config.h dan mengalokasikan memori sebenarnya di file .ino utama untuk mencegah error *multiple definitions* linker.  
  * Mengimplementasikan kata kunci inline pada semua definisi fungsi di dalam berkas header (.h) pelengkap agar aman saat dilakukan *cross-include* oleh compiler.