#include "settings.h"
#include "RPGFace.h"

watchySettings settings{
    .cityID                = "",
    .lat                   = DEFAULT_LAT,
    .lon                   = DEFAULT_LON,
    .weatherAPIKey         = OPENWEATHERMAP_APIKEY,
    .weatherURL            = OPENWEATHERMAP_URL,
    .weatherUnit           = WEATHER_UNIT,
    .weatherLang           = WEATHER_LANG,
    .weatherUpdateInterval = WEATHER_UPDATE_INTERVAL,
    .ntpServer             = NTP_SERVER,
    .gmtOffset             = GMT_OFFSET_SEC,
    .vibrateOClock         = VIBRATE_OCLOCK,
};

RPGFace face(settings);

void setup() { face.init(); }
void loop()  {}
