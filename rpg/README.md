# RPG Face

An 8-bit RPG HUD watch face for Watchy v3.0.

```
FRITSCH               LVL 7
HP [████████████████]
XP [████████░░░░░░░░]
- - - - - - - - - - -
        14:32
    THU 30.APR.26
- - - - - - - - - - -
CLOUDY          17C
[+] FRI BUFF
- - - - - - - - - - -
NET:OK        UP:4H32M
```

## Layout

| Row | Left | Right |
|-----|------|-------|
| Name | Player name | Streak level |
| HP | Label | Battery % bar |
| XP | Label | Today's steps bar |
| Time | — | Centered clock |
| Date | — | Centered date |
| Weather | Condition | Temperature |
| Status | RPG status message | — |
| Bottom | WiFi status | Uptime since unplug |

## Features

**Step streak (LVL)** — counts consecutive days you hit your step goal. Miss a day, it resets to 0. Persists across reboots via NVS.

**HP bar** — battery percentage. LiPo curve: 4.2V = 100%, 3.3V = 0%.

**XP bar** — today's steps toward your daily goal. Resets at midnight.

**Auto location** — on first WiFi connect, fetches lat/lon from `ip-api.com` and caches it in NVS. Weather uses your actual location without a hardcoded city ID.

**Auto timezone** — OWM returns the timezone offset with weather data. DST handled automatically after first fetch.

**WiFi** — tries your configured networks in order on each weather update. Saves credentials to NVS so Watchy's built-in connect also works.

**Status messages**

| Message | Condition |
|---------|-----------|
| `[!] LOW BATTERY` | Battery ≤ 15% |
| `[*] STEPS MAXED` | Daily goal reached |
| `[!] MON DEBUFF` | It's Monday |
| `[+] FRI BUFF` | It's Friday |
| `[+] FULLY CHARGED` | Battery ≥ 95% |
| `[ ] ALL CLEAR` | Everything fine |

## Setup

```bash
cp settings.h.example settings.h
# edit settings.h with your values
```

Required fields in `settings.h`:

| Field | Description |
|-------|-------------|
| `WIFI_NETWORKS` | SSID / password pairs, tried in order |
| `OPENWEATHERMAP_APIKEY` | Free key from openweathermap.org |
| `DEFAULT_LAT` / `DEFAULT_LON` | Fallback location for first boot |
| `PLAYER_NAME` | Displayed top-left (max ~8 chars for monospace fit) |
| `STEPS_GOAL` | Daily step target (default 8000) |

## Hardware notes

- Tested on **Watchy v3.0** (SQFMI-WATCHY-10, ESP32-S3)
- `USB_DETECT_PIN 21` — active HIGH USB presence pin, v3.0; used for uptime tracking (GPIO10 `CHRG_STATUS_PIN` is unsuitable — it de-asserts on full charge, not unplug)
- Step counter uses BMA423 accelerometer via `sensor.getCounter()` (cumulative, not auto-reset)
