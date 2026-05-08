# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Upload

No build system — Arduino IDE or arduino-cli only.

```bash
# Compile and upload (watch appears as /dev/ttyACM0 via native USB)
arduino-cli compile --upload -p /dev/ttyACM0 \
  --fqbn esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=default_8MB \
  rpg/
```

Board settings: **ESP32S3 Dev Module**, Flash 8MB (`FlashSize=8M`), Partition 8MB with SPIFFS (`PartitionScheme=default_8MB`), Watchy >= 1.4.11, arduino-esp32 >= 3.0.2.
Port: `/dev/ttyACM0` (ESP32-S3 native USB CDC, not UART → not ttyUSB0).

No linter, no test suite — verify by flashing.

## Repository Structure

Each watch face is a self-contained Arduino sketch in its own folder. The sketch folder name must match the `.ino` filename (Arduino requirement).

```
<watchface>/
  <watchface>.ino      ← entry point: includes, watchySettings struct, setup()/loop()
  <WatchFace>.h        ← class extending Watchy, all rendering logic
  settings.h           ← gitignored credentials/config (copy from settings.h.example)
  settings.h.example   ← safe template
```

## Architecture

**Watchy class model** — each face subclasses `Watchy` and overrides `drawWatchFace()`. The base class handles deep sleep, RTC wake, button dispatch, and NTP sync. Only `drawWatchFace()` needs implementing.

**`settings.h` is gitignored** — never committed. Contains WiFi credentials, OWM API key, player name. Always edit `settings.h.example` for any config changes meant to be shared.

**NVS persistence** (`Preferences`, namespace `"rpg"`) — survives deep sleep and reboot:
- `chg` / `chgEnd` — charge state and unplug timestamp (uptime calc)
- `stkDay/Mon/Yr/Cnt/Base` — step streak date and counter
- `geoLat` / `geoLon` — IP-geolocated coordinates (fetched once, cached)

**Weather flow** — `getWeatherData()` (Watchy built-in) fetches OWM every `WEATHER_UPDATE_INTERVAL` minutes; other wakes return cached `RTC_DATA_ATTR weatherData currentWeather`. WiFi is killed after each fetch. Use `weather.weatherConditionCode` (int, OWM codes 2xx–80x) not `weather.weatherDescription` (stringify artifacts).

**WiFi** — `preConnectWiFi()` tries each `WIFI_NETWORKS` entry with `WiFi.begin(ssid, pass)` + 8s timeout. Saves to NVS so Watchy's own `connectWiFi()` works on subsequent wakes. `autoDetectLocation()` hits `ip-api.com/json` once to get lat/lon, caches in NVS, updates `settings.lat/lon` before `getWeatherData()`.

**Step streak** — BMA423 `sensor.getCounter()` is cumulative (never auto-resets). `updateStreak()` stores a daily baseline in NVS and computes delta. On day rollover: delta ≥ `STEPS_GOAL` increments `_streak`, else resets to 0.

**Key Watchy API facts:**
- `currentTime` — RTC time struct (`.Hour`, `.Minute`, `.Day`, `.Month`, `.Wday`, `.Year` offset from 1970)
- `sensor.getCounter()` — cumulative steps since last manual reset
- `getBatteryVoltage()` — raw float; LiPo curve: 4.2V=100%, 3.3V=0%
- `connectWiFi()` is not virtual — cannot override, only pre-empt
- `gmtOffset` auto-updated from OWM `timezone` field on each successful fetch (DST handled)
- Display: 200×200 px e-paper, `GxEPD_BLACK` / `GxEPD_WHITE`, monospace fonts
