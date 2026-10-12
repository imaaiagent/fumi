# FUMI Firmware

This folder contains the public FUMI Arduino firmware.

## Hardware

- Waveshare ESP32-S3 Touch LCD 1.46
- Built around the Waveshare ESP32-S3/LVGL example structure
- I2S microphone input for push-to-talk voice

## Setup

1. Copy `fumi_config.h.example` to `fumi_config.h`.
2. Fill in your Wi-Fi SSID/password and FUMI backend URLs.
3. Keep `fumi_config.h` local. It is ignored by Git.
4. Open `FUMI.ino` in Arduino IDE.
5. Install the Waveshare/LVGL dependencies required by the included headers.
6. Select the board settings documented in the repository README.
7. Build and upload.

The public firmware does **not** contain a Wi-Fi password or model-provider API key.

## Local dependency files

The current sketch references the Waveshare project headers/sources:

- `Display_SPD2010`
- `Audio_PCM5101`
- `RTC_PCF85063`
- `Gyro_QMI8658`
- `LVGL_Driver`
- `MIC_MSM`
- `ESP_I2S`
- `PWR_Key`
- `SD_Card`
- `LVGL_Example`
- `BAT_Driver`
- `Touch_SPD2010` and related display/touch files from the Waveshare project

Keep the complete working Waveshare project structure when building locally.

## Known development baseline

The latest verified development baseline used during the current project work was:

- Arduino IDE 2.3.10
- ESP32 by Espressif Systems 3.3.12
- Board: ESP32S3 Dev Module
- CPU: 240 MHz (WiFi)
- Flash Mode: QIO 80 MHz
- Flash Size: 16 MB
- Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)
- PSRAM: OPI PSRAM
- Arduino Runs On: Core 1
- Events Run On: Core 1
- USB Mode: Hardware CDC and JTAG
- Upload Mode: UART0 / Hardware CDC
- USB CDC On Boot: Disabled

Treat these as the project development baseline, not as a guarantee for every board revision.
