# Testing

FUMI has separate backend, firmware, and hardware validation layers.

## Backend

From `backend/`:

```bash
npm install
npm run check
npm test
```

The smoke test covers the health endpoint and device-token authentication behavior.

## Firmware

Compile the firmware against the documented Arduino, ESP32, LVGL, and Waveshare configuration before marking a release as hardware-verified.

## Hardware checklist

- [ ] display initializes
- [ ] touch works
- [ ] animation/blinking works
- [ ] Wi-Fi connects
- [ ] `/brain` succeeds
- [ ] speaker/audio output works
- [ ] microphone capture works
- [ ] `/voice` succeeds
- [ ] power cycle works
- [ ] logs contain no credentials or tokens

Only mark a feature as verified when it has actually been tested.
