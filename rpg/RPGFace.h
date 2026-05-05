// RPGFace.h — 8-bit RPG HUD watch face for Watchy v3.0 (SQFMI-WATCHY-10)
// Hardware: ESP32-S3, GPIO10 charge indicator, BMA423 accelerometer
//
// Display layout (200x200 e-paper):
//   FRITSCH               LVL 4
//   HP [████████░░░░░░░░]
//   XP [████░░░░░░░░░░░░]
//   - - - - - - - - - - -
//           14:32
//       THU 30.APR.26
//   - - - - - - - - - - -
//   CLOUDY          17C
//   [!] MON DEBUFF
//   - - - - - - - - - - -
//   NET:OK        UP:4H32M

#pragma once

#include <Watchy.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include "settings.h"

class RPGFace : public Watchy {
  using Watchy::Watchy;

public:
  void drawWatchFace() override {
    display.fillScreen(GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setTextWrap(false);

    preConnectWiFi();
    autoDetectLocation();
    weather = getWeatherData();
    trackCharge();
    updateStreak();

    drawNameRow();    // y=0..16
    drawHPRow();      // y=17..29
    drawXPRow();      // y=30..43
    drawDash(48);
    drawTime();       // y=53..97
    drawDate();       // y=98..113
    drawDash(118);
    drawWeatherRow(); // y=123..135
    drawStatusRow();  // y=136..150
    drawDash(155);
    drawBottomRow();  // y=160..172
  }

private:
  weatherData weather;
  uint16_t    _streak   = 0;
  uint32_t    _stepBase = 0;

  // Computes today's steps as delta from day-start baseline stored in NVS.
  // On day rollover: delta >= STEPS_GOAL increments streak, else resets to 0.
  void updateStreak() {
    uint32_t curSteps = sensor.getCounter();
    Preferences p;
    p.begin("rpg", false);

    uint8_t  sDay = p.getUChar("stkDay", 0);
    uint8_t  sMon = p.getUChar("stkMon", 0);
    uint16_t sYr  = p.getUShort("stkYr",  0);
    uint16_t cnt  = p.getUShort("stkCnt", 0);
    uint32_t base = p.getULong("stkBase", curSteps);

    uint8_t  today = currentTime.Day;
    uint8_t  mon   = currentTime.Month;
    uint16_t yr    = (uint16_t)(currentTime.Year + 1970);

    if (sYr == 0) {
      // First boot: initialise baseline, streak starts at 0
      base = curSteps;
      cnt  = 0;
      p.putUChar("stkDay",   today);
      p.putUChar("stkMon",   mon);
      p.putUShort("stkYr",   yr);
      p.putULong("stkBase",  base);
      p.putUShort("stkCnt",  cnt);
    } else if (today != sDay || mon != sMon || yr != sYr) {
      // New day: score yesterday, roll baseline forward
      uint32_t delta = (curSteps >= base) ? curSteps - base : 0;
      cnt  = (delta >= (uint32_t)STEPS_GOAL) ? cnt + 1 : 0;
      base = curSteps;
      p.putUChar("stkDay",   today);
      p.putUChar("stkMon",   mon);
      p.putUShort("stkYr",   yr);
      p.putULong("stkBase",  base);
      p.putUShort("stkCnt",  cnt);
    }

    p.end();
    _streak   = cnt;
    _stepBase = base;
  }

  // Fetch lat/lon from ip-api.com once and cache in NVS.
  // Updates settings.lat/lon so getWeatherData() uses the detected location.
  void autoDetectLocation() {
    if (WiFi.status() != WL_CONNECTED) return;

    Preferences p;
    p.begin("rpg", false);
    String lat = p.getString("geoLat", "");
    String lon = p.getString("geoLon", "");

    if (lat.length() == 0) {
      HTTPClient http;
      http.setConnectTimeout(3000);
      http.begin("http://ip-api.com/json?fields=lat,lon");
      if (http.GET() == 200) {
        JSONVar obj = JSON.parse(http.getString());
        if (JSON.typeof(obj) == "object") {
          lat = String((double)obj["lat"], 4);
          lon = String((double)obj["lon"], 4);
          p.putString("geoLat", lat);
          p.putString("geoLon", lon);
        }
      }
      http.end();
    }
    p.end();

    if (lat.length() > 0) {
      settings.cityID = "";
      settings.lat    = lat;
      settings.lon    = lon;
    }
  }

  // Try each network in order. WiFi.begin(ssid, pass) saves to NVS so
  // Watchy's own connectWiFi() also works on subsequent wakes.
  void preConnectWiFi() {
    WiFi.mode(WIFI_STA);
    for (const auto& n : WIFI_NETWORKS) {
      WiFi.begin(n.ssid, n.pass);
      if (WiFi.waitForConnectResult(8000) == WL_CONNECTED) return;
      WiFi.disconnect(false, false);
    }
  }

  // ── Charge tracking ──────────────────────────────────────────────────────
  // Saves a timestamp to NVS whenever the watch transitions from
  // charging → not charging (i.e. the moment you unplug).

  void trackCharge() {
    pinMode(CHARGE_PIN, INPUT);
    bool charging = !digitalRead(CHARGE_PIN);  // active LOW

    Preferences p;
    p.begin("rpg", false);
    bool wasCharging = p.getBool("chg", false);
    if (wasCharging && !charging) {
      p.putULong("chgEnd", approxUnix());
    }
    p.putBool("chg", charging);
    p.end();
  }

  // ── Helpers ──────────────────────────────────────────────────────────────

  // Approximate unix timestamp from RTC — good enough for uptime display.
  // Not accounting for full leap year precision; error < 1 day per century.
  unsigned long approxUnix() {
    int y = currentTime.Year + 1970;
    static const uint16_t mdays[] =
      {0,31,59,90,120,151,181,212,243,273,304,334};
    unsigned long days = (unsigned long)(y - 1970) * 365UL + (y - 1969) / 4;
    days += mdays[currentTime.Month - 1];
    if (currentTime.Month > 2 && y % 4 == 0) days++;
    days += currentTime.Day - 1;
    return days * 86400UL
         + (unsigned long)currentTime.Hour   * 3600UL
         + (unsigned long)currentTime.Minute * 60UL
         + currentTime.Second;
  }

  String fmtUptime(unsigned long secs) {
    unsigned long h = secs / 3600;
    unsigned long m = (secs % 3600) / 60;
    if (h >= 24) return String(h / 24) + "D" + String(h % 24) + "H";
    return String(h) + "H" + String(m) + "M";
  }

  int battPct() {
    // LiPo 402030: 4.2V = 100%, 3.3V = 0%
    float v = getBatteryVoltage();
    return constrain((int)((v - 3.3f) / 0.9f * 100.f), 0, 100);
  }

  // Map OWM condition code to short biome label.
  // Codes: 2xx=storm 3xx=drizzle 5xx=rain 6xx=snow 7xx=atmo 800=clear 80x=clouds
  String biomeCode() {
    int c = weather.weatherConditionCode;
    if (c >= 200 && c < 300) return "STORM";
    if (c >= 300 && c < 400) return "DRIZZLE";
    if (c >= 500 && c < 600) return "RAIN";
    if (c >= 600 && c < 700) return "SNOW";
    if (c == 701)             return "MISTY";
    if (c == 741)             return "FOGGY";
    if (c >= 700 && c < 800) return "HAZY";
    if (c == 800)             return "CLEAR";
    if (c == 801)             return "PARTLY";
    if (c >= 802)             return "CLOUDY";
    return "UNKNOWN";
  }

  // Right-align: returns x cursor position for given string
  int rightX(const String& s, int margin = 4) {
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    return 200 - margin - (int)w - x1;
  }

  // Center horizontally: returns x cursor position
  int centerX(const String& s) {
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    return (200 - (int)w) / 2 - x1;
  }

  // Dashed separator line
  void drawDash(int y) {
    for (int x = 4; x < 196; x += 7)
      display.drawFastHLine(x, y, 4, GxEPD_WHITE);
  }

  // Segmented RPG bar: dithered background, solid fill, 10 tick marks
  void drawBar(int x, int y, int w, int h, int pct) {
    // checkerboard dither for empty portion
    for (int px = x; px < x + w; px++)
      for (int py = y; py < y + h; py++)
        if ((px + py) % 2 == 0) display.drawPixel(px, py, GxEPD_WHITE);
    // solid fill
    int fillW = w * constrain(pct, 0, 100) / 100;
    display.fillRect(x, y, fillW, h, GxEPD_WHITE);
    // outer border
    display.drawRect(x, y, w, h, GxEPD_WHITE);
    // 10 segment tick marks
    int seg = w / 10;
    for (int i = 1; i < 10; i++)
      display.drawFastVLine(x + i * seg, y, h, GxEPD_BLACK);
  }

  // ── Draw sections ────────────────────────────────────────────────────────

  void drawNameRow() {
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(4, 14);
    display.print(PLAYER_NAME);
    String lvl = "LVL " + String(_streak);
    display.setCursor(rightX(lvl), 14);
    display.print(lvl);
  }

  void drawHPRow() {
    display.setFont(&FreeMono9pt7b);
    display.setCursor(4, 28);
    display.print("HP");
    drawBar(26, 20, 170, 8, battPct());
  }

  void drawXPRow() {
    display.setFont(&FreeMono9pt7b);
    display.setCursor(4, 42);
    display.print("XP");
    uint32_t todaySteps = sensor.getCounter() - _stepBase;
    int pct = (int)min(todaySteps * 100UL / (unsigned long)STEPS_GOAL, 100UL);
    drawBar(26, 34, 170, 8, pct);
  }

  void drawTime() {
    display.setFont(&FreeMonoBold24pt7b);
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d",
      currentTime.Hour, currentTime.Minute);
    display.setCursor(centerX(String(buf)), 97);
    display.print(buf);
  }

  void drawDate() {
    static const char* WDAYS[] =
      {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    static const char* MONTHS[] =
      {"JAN","FEB","MAR","APR","MAY","JUN",
       "JUL","AUG","SEP","OCT","NOV","DEC"};
    display.setFont(&FreeMono9pt7b);
    char buf[18];
    snprintf(buf, sizeof(buf), "%s %02d.%s.%02d",
      WDAYS[currentTime.Wday - 1],
      currentTime.Day,
      MONTHS[currentTime.Month - 1],
      (currentTime.Year + 1970) % 100);
    display.setCursor(centerX(String(buf)), 113);
    display.print(buf);
  }

  void drawWeatherRow() {
    display.setFont(&FreeMono9pt7b);
    char buf[20];
    snprintf(buf, sizeof(buf), "%-8s    %3dC",
      biomeCode().c_str(),
      (int)weather.temperature);
    display.setCursor(4, 133);
    display.print(buf);
  }

  void drawStatusRow() {
    display.setFont(&FreeMono9pt7b);
    int pct              = battPct();
    uint32_t todaySteps  = sensor.getCounter() - _stepBase;
    int wday             = currentTime.Wday;  // 1=Sun, 2=Mon ... 7=Sat

    String status;
    if      (pct <= 15)                    status = "[!] LOW BATTERY";
    else if (todaySteps >= STEPS_GOAL)     status = "[*] STEPS MAXED";
    else if (wday == 2)                status = "[!] MON DEBUFF";
    else if (wday == 6)                status = "[+] FRI BUFF";
    else if (pct >= 95)                status = "[+] FULLY CHARGED";
    else                               status = "[ ] ALL CLEAR";

    display.setCursor(4, 150);
    display.print(status);
  }

  void drawBottomRow() {
    display.setFont(&FreeMono9pt7b);

    // Left: WiFi status (proxied via weather.external — true = API reached this cycle)
    display.setCursor(4, 168);
    display.print(weather.external ? "NET:OK" : "NET:--");

    // Right: uptime since last unplug
    Preferences p;
    p.begin("rpg", true);
    unsigned long chgEnd = p.getULong("chgEnd", 0);
    p.end();

    String uptime = "UP:--";
    if (chgEnd > 0) {
      unsigned long now_ = approxUnix();
      if (now_ > chgEnd) uptime = "UP:" + fmtUptime(now_ - chgEnd);
    }
    display.setCursor(rightX(uptime), 168);
    display.print(uptime);
  }
};
