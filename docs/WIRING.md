# FUMI Wiring

The reference FUMI build uses the Waveshare ESP32-S3 Touch LCD 1.46 as the main device. The display, touch, microphone, and audio path are connected through the development board.

```text
USB / Power
    |
    v
Waveshare ESP32-S3 Touch LCD 1.46
    |
    +--> Round LCD + touch
    +--> Microphone
    +--> Audio output
    +--> Power / battery peripherals
    +--> Wi-Fi
```

For exact pin-level details, use the official Waveshare schematic and the upstream driver source fetched by the setup scripts. This repository intentionally avoids duplicating potentially stale pin mappings.
