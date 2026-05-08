#pragma once

#include <Watchy.h>
#include <WiFi.h>
#include <math.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include "bitmaps.h"
#include "settings.h"

static RTC_DATA_ATTR uint32_t _mpStepBase = 0;
static RTC_DATA_ATTR uint8_t  _mpBaseDay  = 0;

class MoonPhaseFace : public Watchy {
  using Watchy::Watchy;

public:
  void drawWatchFace() override {
    display.fillScreen(GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setTextWrap(false);

    preConnectWiFi();
    getWeatherData();  // sole purpose: update settings.gmtOffset from OWM timezone field

    if (currentTime.Day != _mpBaseDay) {
      _mpStepBase = sensor.getCounter();
      _mpBaseDay  = currentTime.Day;
    }

    drawMoon();
    drawDTG();
    drawCorners();
  }

private:

  // ── Moon ─────────────────────────────────────────────────────────────────

  void drawMoon() {
    display.drawBitmap(0, 0, BITMAP_BACKGROUND, 200, 200, GxEPD_WHITE);
    drawShadow(moonPhase());
  }

  // Returns phase in [0, 1): 0 = new moon, 0.5 = full moon.
  // Julian Day formula; reference new moon JD 2451550.1 (2000-01-06 18:14 UTC).
  float moonPhase() {
    int y = currentTime.Year + 1970;
    int m = currentTime.Month;
    int d = currentTime.Day;
    if (m < 3) { y--; m += 12; }
    long A = y / 100;
    long B = 2 - A + A / 4;
    double jd = (long)(365.25  * (y + 4716))
              + (long)(30.6001 * (m + 1))
              + d + B - 1524.5
              + currentTime.Hour   / 24.0
              + currentTime.Minute / 1440.0;
    double phase = fmod((jd - 2451550.1) / 29.53059, 1.0);
    if (phase < 0.0) phase += 1.0;
    return (float)phase;
  }

  // Scanline shadow mask. Northern hemisphere convention:
  //   waxing (0→0.5): shadow on left, right side illuminated
  //   waning (0.5→1): shadow on right, left side illuminated
  // Uses drawFastHLine only — no fillEllipse dependency.
  void drawShadow(float phase) {
    const int cx = 99, cy = 99, r = 91;
    float cos_a = cosf(phase * 2.0f * (float)PI);
    bool waxing = phase < 0.5f;

    for (int y = cy - r; y <= cy + r; y++) {
      float chord = sqrtf((float)(r * r) - (float)((y - cy) * (y - cy)));
      int x_left  = (int)(cx - chord);
      int x_right = (int)(cx + chord);

      if (waxing) {
        // terminator moves right→left as phase grows toward full
        int x_term = constrain((int)(cx + cos_a * chord), x_left, x_right);
        if (x_term > x_left)
          display.drawFastHLine(x_left, y, x_term - x_left + 1, GxEPD_BLACK);
      } else {
        // terminator moves left→right as phase grows toward next new
        int x_term = constrain((int)(cx - cos_a * chord), x_left, x_right);
        if (x_term < x_right)
          display.drawFastHLine(x_term, y, x_right - x_term + 1, GxEPD_BLACK);
      }
    }
  }

  // ── DTG ──────────────────────────────────────────────────────────────────

  // NATO timezone letter from UTC offset in hours.
  // UTC+1=A, UTC+2=B (covers CET/CEST); UTC+0=Z; UTC-1=N, UTC-2=O, …
  char natoLetter() {
    int offset = (int)(settings.gmtOffset / 3600);
    if (offset > 0) return (char)('A' + offset - 1);
    if (offset == 0) return 'Z';
    return (char)('N' - offset - 1);
  }

  // Military Date-Time Group: DD HHMM L MMMYY (12 chars, no spaces)
  //   DD    — day, regular
  //   HHMM  — time, bold
  //   L     — NATO timezone letter, regular
  //   MMMYY — month + 2-digit year, regular
  // Centering: measure each segment with its actual font, sum advances, center total.
  void drawDTG() {
    static const char* MONTHS[] =
      {"JAN","FEB","MAR","APR","MAY","JUN",
       "JUL","AUG","SEP","OCT","NOV","DEC"};

    char dayBuf[3], hhmmBuf[5], ltrBuf[2], monYrBuf[6];
    snprintf(dayBuf,   sizeof(dayBuf),   "%02d",    currentTime.Day);
    snprintf(hhmmBuf,  sizeof(hhmmBuf),  "%02d%02d", currentTime.Hour, currentTime.Minute);
    ltrBuf[0] = natoLetter(); ltrBuf[1] = '\0';
    snprintf(monYrBuf, sizeof(monYrBuf), "%s%02d",
             MONTHS[currentTime.Month - 1],
             (currentTime.Year + 1970) % 100);

    // xAdvance per char = w("MM") - w("M") — exact glyph advance, not bounding box width
    int16_t x1, y1; uint16_t wa, wb, h;
    display.setFont(&FreeMono12pt7b);
    display.getTextBounds("MM", 0, 0, &x1, &y1, &wa, &h);
    display.getTextBounds("M",  0, 0, &x1, &y1, &wb, &h);
    int regAdv = (int)wa - (int)wb;

    display.setFont(&FreeMonoBold12pt7b);
    display.getTextBounds("MM", 0, 0, &x1, &y1, &wa, &h);
    display.getTextBounds("M",  0, 0, &x1, &y1, &wb, &h);
    int boldAdv = (int)wa - (int)wb;

    // Cursor advances per segment
    int dayAdv   = 2 * regAdv;
    int hhmmAdv  = 4 * boldAdv;
    int ltrAdv   = 1 * regAdv;
    int monYrAdv = 5 * regAdv;
    int totalAdv = dayAdv + hhmmAdv + ltrAdv + monYrAdv;

    // Derive baseline so text is vertically centered at y=100
    display.setFont(&FreeMonoBold12pt7b);
    display.getTextBounds("0", 0, 0, &x1, &y1, &wa, &h);
    int dtgY = 100 - y1 - (int)h / 2;

    // Black rect: use getTextBounds on a representative string for height/y
    display.setFont(&FreeMono12pt7b);
    display.getTextBounds(dayBuf, 0, dtgY, &x1, &y1, &wa, &h);
    int rectY = (int)y1; int rectH = (int)h;

    int startX = (200 - totalAdv) / 2;
    display.fillRect(startX, rectY, totalAdv, rectH, GxEPD_BLACK);

    display.setFont(&FreeMono12pt7b);
    display.setCursor(startX, dtgY);
    display.print(dayBuf);

    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(startX + dayAdv, dtgY);
    display.print(hhmmBuf);

    display.setFont(&FreeMono12pt7b);
    display.setCursor(startX + dayAdv + hhmmAdv, dtgY);
    display.print(ltrBuf);

    display.setCursor(startX + dayAdv + hhmmAdv + ltrAdv, dtgY);
    display.print(monYrBuf);
  }

  // ── Corners ───────────────────────────────────────────────────────────────

  // G6EJD 4th-degree polynomial fit to measured LiPo discharge curve.
  // Horner form to avoid repeated powf calls.
  int battPct() {
    float v = getBatteryVoltage();
    float p = v * (v * (v * (2808.38f * v - 43560.92f) + 252848.59f) - 650767.46f)
            + 626532.57f;
    return constrain((int)p, 0, 100);
  }

  void drawCorners() {
    display.setFont(&FreeMono9pt7b);

    // Bottom-left: daily step delta
    uint32_t steps = sensor.getCounter() - _mpStepBase;
    char stepBuf[8];
    snprintf(stepBuf, sizeof(stepBuf), "%lu", (unsigned long)steps);
    display.setCursor(4, 196);
    display.print(stepBuf);

    // Bottom-right: battery percentage
    char battBuf[6];
    snprintf(battBuf, sizeof(battBuf), "%d%%", battPct());
    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds(battBuf, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor(196 - (int)bw - bx, 196);
    display.print(battBuf);
  }

  // ── WiFi ──────────────────────────────────────────────────────────────────

  // Try each network in order. WiFi.begin(ssid, pass) saves to NVS so
  // Watchy's own connectWiFi() works on subsequent wakes.
  void preConnectWiFi() {
    WiFi.mode(WIFI_STA);
    for (const auto& n : WIFI_NETWORKS) {
      WiFi.begin(n.ssid, n.pass);
      if (WiFi.waitForConnectResult(8000) == WL_CONNECTED) return;
      WiFi.disconnect(false, false);
    }
  }
};
