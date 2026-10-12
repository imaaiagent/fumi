$ErrorActionPreference = "Stop"

# Fetch the official Waveshare example source tree, then create a buildable
# Arduino sketch folder with FUMI replacing the example .ino.
$repoUrl = "https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.46.git"
$root = Split-Path -Parent $PSScriptRoot
$vendor = Join-Path $root "firmware\vendor\waveshare"
$sketchDir = Join-Path $root "firmware\build\LVGL_Arduino"
$sourceDir = Join-Path $vendor "example\Arduino-3.1.1\examples\LVGL_Arduino"

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
  throw "Git is required. Install Git for Windows, then run this script again."
}

if (Test-Path $vendor) {
  Remove-Item $vendor -Recurse -Force
}
git clone --depth 1 $repoUrl $vendor
if ($LASTEXITCODE -ne 0) { throw "Could not clone the official Waveshare repository." }
if (-not (Test-Path $sourceDir)) {
  throw "Expected Waveshare Arduino example was not found at: $sourceDir"
}

New-Item -ItemType Directory -Force -Path $sketchDir | Out-Null
Copy-Item (Join-Path $sourceDir "*") $sketchDir -Recurse -Force
Copy-Item (Join-Path $root "firmware\FUMI.ino") (Join-Path $sketchDir "LVGL_Arduino.ino") -Force
Copy-Item (Join-Path $root "firmware\fumi_config.h.example") (Join-Path $sketchDir "fumi_config.h.example") -Force

$configPath = Join-Path $sketchDir "fumi_config.h"
if (-not (Test-Path $configPath)) {
  Copy-Item (Join-Path $sketchDir "fumi_config.h.example") $configPath
  Write-Host "Created fumi_config.h. Edit it with your Wi-Fi and backend values before compiling." -ForegroundColor Yellow
}
Write-Host ""
Write-Host "FUMI sketch prepared at: $sketchDir" -ForegroundColor Green
Write-Host "Open LVGL_Arduino.ino in Arduino IDE. Keep the original Waveshare library/dependency notices." -ForegroundColor Green
