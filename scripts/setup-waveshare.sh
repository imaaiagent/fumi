#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENDOR="$ROOT/firmware/vendor/waveshare"
SKETCH_DIR="$ROOT/firmware/build/LVGL_Arduino"
SOURCE_DIR="$VENDOR/example/Arduino-3.1.1/examples/LVGL_Arduino"

command -v git >/dev/null 2>&1 || { echo "Git is required."; exit 1; }
rm -rf "$VENDOR"
git clone --depth 1 https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.46.git "$VENDOR"
test -d "$SOURCE_DIR" || { echo "Waveshare Arduino example not found."; exit 1; }
mkdir -p "$SKETCH_DIR"
cp -R "$SOURCE_DIR/." "$SKETCH_DIR/"
cp "$ROOT/firmware/FUMI.ino" "$SKETCH_DIR/LVGL_Arduino.ino"
cp "$ROOT/firmware/fumi_config.h.example" "$SKETCH_DIR/fumi_config.h.example"
if [ ! -f "$SKETCH_DIR/fumi_config.h" ]; then
  cp "$SKETCH_DIR/fumi_config.h.example" "$SKETCH_DIR/fumi_config.h"
  echo "Edit firmware/build/LVGL_Arduino/fumi_config.h before compiling."
fi
echo "FUMI sketch prepared at: $SKETCH_DIR"
