# ⚡ DOKUMENTASI RESMI: VOTOL Smart Dashboard POLYTRON FOX RS V15.1 Pro (BLE Edition)

**Deskripsi Proyek:**
VOTOL Smart Dashboard adalah instrumen kokpit tambahan berteknologi tinggi untuk motor listrik dengan *controller* VOTOL (khususnya Polytron Fox-R). Alat ini membaca data spesifik pabrikan (Suhu, RPM, Voltase, Arus, hingga Status Per-Cell Baterai BMS) langsung dari jalur CAN Bus menggunakan modul MCP2515.

Pada versi **V15.1 (Pro Edition)** ini, alat telah dilengkapi dengan **Bluetooth Low Energy (BLE) Live Streaming** yang memungkinkan data motor ditampilkan secara *real-time* ke layar HP Anda melalui Web Dashboard, tanpa memerlukan aplikasi tambahan atau server khusus.

**Fitur unggulan lainnya meliputi:**
* Modul RTC (DS3231) untuk jam *offline* super akurat. Syncron melalui WiFi (Tethreing HP) dengan ssid dan pass customable.
* **Early Warning System (Buzzer):** Alarm kecepatan tinggi dan indikator gigi mundur.
* Perlindungan logika *Anti-Regen* agar pengereman regeneratif tidak disalahartikan sebagai pengecasan.
* **Sistem Menu On-Device:** Pengaturan alat (Sound, BLE, NTP) langsung dari tombol fisik layar.

---

## 🛠️ 1. Alat dan Bahan Utama

1. **Mikrokontroler:** ESP32-C3 Super Mini (Prosesor ringkas, WiFi & BLE tertanam).
2. **Modul Pembaca CAN:** MCP2515 (Transceiver SPI). 
   *(Catatan Penting: Modul harus menggunakan crystal 8MHz. Jika tertulis 16.000 pada crystal-nya, kode harus disesuaikan ke `MCP_16MHZ`).*
3. **Layar Display:** OLED SSD1306 ukuran 0.91 inch (128x32 pixel), antarmuka I2C.
4. **Modul Waktu (RTC):** DS3231 (Akurat dengan baterai kancing), antarmuka I2C.
5. **Tombol Navigasi:** 1 buah *tactile switch* (Push Button).
6. **Buzzer Aktif 3V/5V:** Untuk *feedback* suara dan peringatan keselamatan.
7. **Jumper Cap (120 Ohm):** Wajib dipasang pada terminal resistor modul MCP2515 untuk mencegah *noise* data pada CAN Bus.
8. **Kabel Jumper & Timah Solder:** Secukupnya.

---

## ⚡ 2. Skema Wiring (Jalur Kabel)

Berikut adalah referensi *Pinout* dari ESP32-C3 Super Mini:

![ESP32-C3 Super Mini Pinout](https://mischianti.org/wp-content/uploads/2025/07/ESP32-C3-Super-Mini-pinout-low.jpg)

Pemasangan pin dioptimalkan untuk ESP32-C3 Super Mini sebagai berikut:

| Komponen | Pin Modul | Sambung ke ESP32-C3 | Keterangan Tambahan |
| --- | --- | --- | --- |
| **OLED & RTC** | **SDA** | **GPIO 8** | Digabung (Paralel) untuk layar OLED & RTC DS3231. |
| *(Jalur I2C)* | **SCL** | **GPIO 9** | Digabung (Paralel) untuk layar OLED & RTC DS3231. |
| | VCC | 5V | Tegangan operasi Modul RTC dan Oled. |
| | GND | GND | Gabung ke Ground utama ESP32. |
| **MCP2515** | **CS** | **GPIO 7** | Jalur Chip Select SPI. |
| *(Jalur SPI)* | **SI / MOSI** | **GPIO 6** | Jalur Data Masuk (SPI). |
| | **SO / MISO** | **GPIO 5** | Jalur Data Keluar (SPI). |
| | **SCK** | **GPIO 4** | Jalur Clock SPI. |
| | **INT** | **GPIO 10** | Jalur Interrupt (Penting untuk kecepatan data tanpa *lag*). |
| | **VCC** | **5V (VBUS)** | **WAJIB 5V!** IC Transceiver TJA1050 tidak membaca motor jika diberi 3.3V. |
| | GND | GND | Gabung ke Ground utama dan Ground Motor. |
| **Buzzer** | **Positif (+)** | **GPIO 2** | Fitur suara & alarm. |
| | Negatif (-) | GND | |
| **CAN Motor** | **CAN H** | Terminal **H** | Kabel CAN High dari Motor (Biasanya Oranye/Kuning). |
| | **CAN L** | Terminal **L** | Kabel CAN Low dari Motor (Biasanya Hijau/Coklat). |
| **Tombol Nav.** | Kaki 1 | **GPIO 3** | Input tombol navigasi halaman & setting. |
| | Kaki 2 | GND | |

---

## 📱 3. Antarmuka Layar (UI) & Navigasi Pintar

Perangkat ini didesain agar informatif namun tidak memecah konsentrasi pengendara.

* **Tekan Pendek (< 3 detik):** Mengganti halaman (Halaman Jam, Suhu, BMS Data, Power, System Info).
* **Tekan Panjang (5 detik):** Masuk ke **MENU SETTINGS**.

### ⚙️ Menu Settings (On-Device)
Di dalam mode *Settings*, tekan pendek untuk menggeser kursor (`>`), tekan tahan 3 detik untuk mengeksekusi pilihan:
1. **SOUND (ON/OFF):** Menyalakan/mematikan fitur Buzzer (Alarm kecepatan >85km/h, gigi mundur, dan *feedback* tombol).
2. **BLE OUT (ON/OFF):** Menyalakan pemancar Bluetooth. *(Catatan: Saat BLE ON, navigasi halaman OLED akan dikunci di Halaman Jam untuk membebaskan 100% memori prosesor agar streaming Bluetooth ke HP tidak lag).*
3. **SYNC NTP:** Menyinkronkan jam alat menggunakan jaringan WiFi (Tethering HP).

**⭐ Fitur Pintar Otomatis (Smart Overlays):**
* **Auto-Sleep:** Layar Info (Suhu, Watt) akan kembali otomatis ke Halaman Jam jika tidak ada penekanan tombol selama 30 detik.
* **Pop-up Mode:** Muncul teks layar penuh selama 3 detik saat memindahkan gigi (PARK, DRIVE, SPORT, REVERSE).
* **High-Speed Override:** Jika dipacu di atas **70 km/h**, angka kecepatan akan mendominasi layar secara otomatis demi keselamatan.
* **Early Warning System** Jika dipacu di atas **85 km/h** maka buzzer akan berbunyi, buzzer juga akan berbunyi saat tombol mudur/reverse ditekan (Pengaturan Sound bisa disilent di Pengaturan).
* **Smart Charging Display:** Saat dicolok *charger* (Ori / Fast Charger), layar bergantian menampilkan Ampere Masuk dan Persentase SOC. Kebal terhadap arus *Regen Braking*.
* **BLE Stream** Saat mode ini di aktifkan maka sistem akan otomatis fokus melayani ble mode saja, layar yang ditampilkan hanya layar Jam, layar mode, speed override, dan settings saja. ini berguna agar sistem bt tidak crash dengan antena wifi dan menghemat penggunaan ram saat render informasi dari mcp.

---

## 🚀 4. Cara Menggunakan Web Dashboard (Streaming ke HP)

Fitur paling mutakhir dari V15.1 adalah **VOTOL Web Dashboard**. Anda bisa memantau puluhan data telemetri (termasuk status per-sel baterai) layaknya *scanner* pabrikan, langsung dari *browser* HP Anda tanpa perlu instalasi aplikasi khusus!

**Langkah Penggunaan:**
1. Nyalakan motor, tekan tahan tombol 5 detik untuk masuk ke menu Settings OLED.
2. Geser ke opsi `BLE OUT`, lalu tekan tahan 3 detik untuk mengubahnya menjadi **ON**. Alat akan *Restart* untuk memuat modul Bluetooth.
3. Di HP Android Anda, nyalakan **Bluetooth** dan **Lokasi (GPS)**. *(GPS diwajibkan oleh Google Chrome untuk memindai perangkat BLE)*.
4. Buka file **`dashboard.html`** yang ada di repositori ini menggunakan aplikasi **Google Chrome** di HP Anda. atau bisa menggunakan halaman https://atzarhika.github.io/jamFoxRS-MCP-OledBLE/
5. Ketuk tombol **CONNECT** berwarna hijau di layar HP.
6. Akan muncul *pop-up* izin Chrome dari bawah layar. Pilih perangkat bernama **"Votol_BLE"** lalu ketuk **Pasangkan / Pair**.
7. Selamat! Layar HP Anda sekarang berubah menjadi kokpit digital *real-time*.

---

## 🔧 5. Pemecahan Masalah Umum (Troubleshooting)

1. **Alat Bootloop / Layar Stuck (Terjebak di Splash Screen):**
   * Ini terjadi jika memori ESP32 bentrok. Gunakan **SAFE MODE**: Cabut alat / matikan motor -> Tekan dan tahan terus tombol fisik -> Colok alat / nyalakan motor sambil tombol tetap ditahan sampai muncul tulisan `SAFE MODE: BLE OFF`.
2. **Compile Error saat Upload Kode:**
   * Wajib ubah pengaturan di Arduino IDE: Menu `Tools` -> `Partition Scheme` -> Pilih **"Huge APP (3MB No OTA / 1MB SPIFFS)"**.
3. **Perangkat BLE tidak muncul di Google Chrome HP:**
   * Pastikan GPS/Lokasi HP sudah diaktifkan. Chrome Android menolak melakukan *scan* Bluetooth murni jika hak akses lokasi dimatikan.
4. **Data Sensor Menunjukkan Angka "0":**
   * Cek kabel CAN H dan CAN L. Putar kunci kontak OFF, tukar posisinya di modul MCP2515, lalu ON-kan lagi. Pastikan *Jumper Resistor* 120 Ohm terpasang.

---

## 🛜 6. Cara Setting SSID dan Password (Tanpa Coding)

Untuk mengganti SSID dan Password WiFi (untuk keperluan *Sync NTP*), Anda **tidak perlu mengutak-atik kodingan atau melakukan flash ulang**. Fitur pengaturan pintar akan menyimpannya langsung ke memori permanen (EEPROM/Preferences) ESP32.

**Langkah 1: Persiapan**
1. Colokkan alat ESP32 Anda ke laptop menggunakan kabel USB Type-C.
2. Buka aplikasi **Arduino IDE**.
3. Pastikan port (COM) ESP32 Anda sudah terbaca di Arduino IDE.

**Langkah 2: Menggunakan Serial Monitor**
1. Buka **Serial Monitor** (ikon kaca pembesar di pojok kanan atas Arduino IDE, atau tekan `Ctrl + Shift + M`).
2. Perhatikan bagian pojok kanan bawah jendela Serial Monitor, pastikan *Baudrate* diatur pada **115200 baud**.

**Langkah 3: Memasukkan Perintah Baru**
Di kolom teks Serial Monitor, ketikkan perintah dengan format berikut (tanpa spasi setelah koma):
`WIFI,NamaSSIDBaru,PasswordBaru`

*(Contoh: Jika nama hotspot HP Anda adalah `Poco X3` dan passwordnya `rahasia1234`, ketik: `WIFI,Poco X3,rahasia1234`)*

**Langkah 4: Eksekusi**
1. Tekan tombol **Enter** di keyboard Anda.
2. Alat ESP32 Anda akan langsung merespons, menyimpan data tersebut, dan melakukan **Restart** otomatis.
3. Cek di **Halaman 5 (System Info)** pada layar OLED untuk memastikan data sudah terganti.

---
## Anda juga bisa ***mengganti tulisan Splash Screen***

dengan perintah: 
`SPLASH,NamaAnda` maksimal 10 huruf).*

---
> ### ⚠️ PENTING: Khusus Pengguna ESP32-C3 Super Mini
> ESP32-C3 Super Mini menggunakan jalur USB langsung (USB CDC) dari dalam chip-nya. Jika Serial Monitor kosong/tidak merespons, lakukan 2 perbaikan ini:
> 
> **1. Aktifkan "USB CDC" (Wajib Flash Ulang Sekali):**
> * Di Arduino IDE, klik menu **Tools**.
> * Cari menu **USB CDC On Boot** dan ubah menjadi **Enabled**.
> * Klik **Upload** untuk memasukkan ulang kode ke ESP32.
> 
> **2. Setting Teks Serial Monitor:**
> * Buka Serial Monitor, di sebelah opsi baudrate, ubah opsi *No line ending* menjadi **Newline** (atau *Both NL & CR*). Jika tidak diset ke Newline, ESP32 tidak akan tahu kalau Anda sudah menekan tombol Enter.


---
### ⚠️ DISCLAIMER!
Kami **TIDAK** bertanggung jawab atas segala kerusakan, malfungsi, atau cedera yang mungkin terjadi pada kendaraan, controller, baterai, atau komponen lainnya.

### BIG THANKS TO
- @zexry619 (https://github.com/zexry619)
- @yudhaime (https://github.com/yudhaime)

---