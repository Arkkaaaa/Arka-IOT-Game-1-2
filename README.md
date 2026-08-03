# Game 1–2 Firmware

Firmware ESP32-C3 untuk Mode 1 **Peras Jeruk** dan Mode 2 **Tangkap Wayang**. Gaya genggaman dibaca melalui load cell 5 kg dan HX711, lalu dikirim ke backend melalui WebSocket Secure dengan capability `FSR_10HZ` setiap 100 ms.

## Isi folder

- `sketch.ino` — firmware utama.
- `diagram.json` — rangkaian Wokwi ESP32-C3 dan HX711.
- `libraries.txt` — daftar library Arduino/Wokwi.

## Perangkat dan pin

| Komponen | Pin ESP32-C3 |
| --- | --- |
| HX711 DT | GPIO4 |
| HX711 SCK | GPIO5 |
| HX711 VCC | 5V |
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

Wi-Fi dan device secret sudah di-hardcode di `sketch.ino` melalui `kWifiSsid`, `kWifiPassword`, dan `kDeviceSecretBase64`. Tidak ada input provisioning melalui Serial Monitor.

Backend harus memakai nilai Base64 yang sama:

```env
DEVICE_SECRET_BASE64=<NILAI_kDeviceSecretBase64>
```

Jika secret diganti, ubah nilai di firmware dan env backend bersamaan, lalu flash ulang perangkat.

## Menjalankan di Wokwi

1. Buka folder ini sebagai proyek Wokwi atau buat proyek ESP32-C3 baru.
2. Gunakan `sketch.ino`, `diagram.json`, dan `libraries.txt` dari folder ini.
3. Jalankan simulasi.
4. Ubah nilai beban pada komponen HX711 untuk mensimulasikan genggaman.

## Kalibrasi

Firmware menggunakan faktor kalibrasi `0.42` dan skala penuh 5.000 gram. Nilai HX711 dikonversi ke rentang FSR `0–4095`. Sesuaikan `kCalibrationFactor` setelah kalibrasi perangkat fisik.

## Indikator Serial

- `ARKA_GAME12_HX711_READY` — HX711 siap.
- `ARKA_GAME12_NETWORK_READY` — Wi-Fi dan waktu sistem siap.
- `ARKA_GAME12_AUTHENTICATED` — autentikasi WSS berhasil.
- `ARKA_GAME12_HX711_FAULT` — HX711 tidak terdeteksi.
