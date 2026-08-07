# Game 1–2 Firmware

Firmware ESP32-C3 untuk Mode 1 **Peras Buah** dan Mode 2 **Go-No-Go**. Gaya genggaman dibaca melalui load cell dan HX711, lalu dikirim ke backend melalui WebSocket Secure setiap 100 ms dengan capability `FSR_10HZ` dan `FSR_TARED_ON_SETUP_BIND`.

## Isi folder

- `sketch.ino` — firmware utama.
- `diagram.json` — rangkaian Wokwi ESP32-C3 dan HX711.
- `libraries.txt` — daftar library Arduino/Wokwi.

## Perangkat dan pin

| Komponen | Pin ESP32-C3 |
| --- | --- |
| Sensor baterai | GPIO1 |
| Buzzer | GPIO2 |
| HX711 DT | GPIO3 |
| HX711 SCK | GPIO4 |
| HX711 VCC | 3V3 |
| HX711 GND | GND |

## Kebutuhan

- Board package ESP32 dengan board **ESP32-C3 DevKitM-1**.
- ArduinoJson 7.4.2 atau lebih baru.
- WebSockets by Markus Sattler 2.6.1 atau lebih baru.
- HX711 Arduino Library by Bogdan Necula.

## Konfigurasi jaringan

Wi-Fi Wokwi sudah ditetapkan di firmware:

```text
SSID: Wokwi-GUEST
Password: kosong
```

Backend WSS yang digunakan adalah `api.arrka.my.id` pada path `/ws/device` dengan subprotocol `arka-device-v1`.

## Device secret

Wi-Fi dan device secret dikonfigurasi langsung di `sketch.ino` melalui `kWifiSsid`, `kWifiPassword`, dan `kDeviceSecretBase64`, sama seperti proyek Game 3.

Sebelum build, ganti placeholder `REPLACE_WITH_DEVICE_SECRET_BASE64` secara lokal dengan nilai Base64 yang sama seperti backend:

```env
DEVICE_SECRET_BASE64=<NILAI_kDeviceSecretBase64>
```

Jangan commit nilai secret aktif. Setelah secret diganti, flash ulang perangkat.

## Menjalankan di Wokwi

1. Buka folder ini sebagai proyek Wokwi atau buat proyek ESP32-C3 baru.
2. Gunakan `sketch.ino`, `diagram.json`, dan `libraries.txt` dari folder ini.
3. Jalankan simulasi.
4. Ubah nilai beban pada komponen HX711 untuk mensimulasikan genggaman.
5. Atur potensiometer baterai untuk menguji status baterai dan peringatan buzzer.

## Kalibrasi

Firmware menggunakan faktor kalibrasi `0.42` dan skala permainan 120.000 gram. Nilai HX711 dikonversi ke rentang FSR `0–4095`. Firmware melakukan tare saat boot dan setiap `setup.bind`, tetapi tidak mengulang tare pada `session.bind`. Pastikan sensor tidak diberi beban ketika board dinyalakan atau persiapan permainan dimulai. Sesuaikan tanda serta nilai `kCalibrationFactor` setelah kalibrasi perangkat fisik.

## Indikator Serial

- `ARKA_GAME12_HX711_READY` — HX711 siap.
- `ARKA_GAME12_HX711_SETUP_TARED` — tare persiapan selesai.
- `ARKA_GAME12_HX711_RECOVERED` — sensor kembali siap setelah gangguan.
- `ARKA_GAME12_NETWORK_READY` — Wi-Fi dan waktu sistem siap.
- `ARKA_GAME12_AUTHENTICATED` — autentikasi WSS berhasil.
- `ARKA_GAME12_HX711_FAULT` — HX711 tidak terdeteksi.
- `ARKA_GAME12_LEDGER_RAM_ONLY` — Preferences/NVS tidak tersedia; firmware tetap berjalan memakai ledger RAM.
- `ARKA_GAME12_LEDGER_RESET` — ledger lama korup dan berhasil dibersihkan.
