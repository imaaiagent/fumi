# FUMI Hardware

FUMI currently targets the **Waveshare ESP32-S3 Touch LCD 1.46** development board.

## Reference hardware

- ESP32-S3
- 1.46-inch round touch display
- LVGL-based UI
- onboard microphone
- audio output path
- Wi-Fi
- board-level power/battery peripherals exposed through the Waveshare driver stack

## Physical controls

The reference board has:

- **POWER** — normal power control
- **BOOT** — ESP32 download/programming mode

The BOOT button is not the normal reset control.

## Driver source

Low-level peripheral and pin definitions come from the upstream Waveshare driver tree. Use the setup scripts in `scripts/` rather than copying pin definitions from another ESP32-S3 board.

## Photos

Maintainer-provided prototype photos are in `docs/assets/`.
