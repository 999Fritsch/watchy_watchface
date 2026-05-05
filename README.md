# watchy-watchfaces

Custom watch faces for the [Watchy](https://watchy.sqfmi.com/) open-source e-paper watch (v3.0, ESP32-S3).

## Watch Faces

| Folder | Name | Description |
|--------|------|-------------|
| [`rpg/`](rpg/) | RPG Face | 8-bit RPG HUD — HP bar, XP bar, step streak, weather, uptime |

## Requirements

- [Watchy library](https://github.com/sqfmi/Watchy) >= 1.4.11
- arduino-esp32 >= 3.0.2
- Board: **ESP32S3 Dev Module**
- Flash: 8MB, Partition: 8MB with SPIFFS

## Setup

1. Install the Watchy library via Arduino Library Manager
2. Clone this repo
3. Open a watch face sketch in Arduino IDE
4. Copy `settings.h.example` → `settings.h` inside the sketch folder
5. Fill in your WiFi credentials, OWM API key, and player name
6. Select **ESP32S3 Dev Module** as board, pick the correct port
7. Upload

## Contributing

Each watch face lives in its own folder as a self-contained Arduino sketch. See the individual README for details.
