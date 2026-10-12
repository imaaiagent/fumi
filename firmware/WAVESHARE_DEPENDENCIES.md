# Waveshare dependencies

FUMI is built on the official Waveshare ESP32-S3 Touch LCD 1.46 Arduino example. To preserve the upstream source, copyright notices, and license files, the repository does not silently rewrite or pretend to own the vendor drivers.

## Fetch the exact dependency tree

Windows PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-waveshare.ps1
```

macOS / Linux:

```bash
bash scripts/setup-waveshare.sh
```

The script clones the official upstream repository and copies its `example/Arduino-3.1.1/examples/LVGL_Arduino` source tree into a local generated sketch folder. It then overlays the FUMI firmware as `LVGL_Arduino.ino`. The upstream repository's separate `example/Arduino-3.1.1/libraries` folder is retained under `firmware/vendor/waveshare`; install any libraries required by the example into the Arduino IDE libraries directory.

Upstream source: https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.46

## Files expected from the upstream example

The upstream folder includes the display, touch, LVGL, RTC, microphone, I2S, power-key, SD-card, battery, and related driver `.h/.cpp/.c` files.

The setup script uses the upstream repository at the time the script runs. For reproducible release builds, record the exact upstream commit hash in the release notes and vendor a reviewed snapshot only when license terms permit.
