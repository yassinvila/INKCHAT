#pragma once
#include <Arduino.h>

extern const unsigned char clear_day[] PROGMEM;
extern const unsigned char clear_night[] PROGMEM;

extern const unsigned char snow[] PROGMEM;
extern const unsigned char rain[] PROGMEM;

extern const unsigned char partly_cloudy_day[] PROGMEM;
extern const unsigned char partly_cloudy_day_snow[] PROGMEM;
extern const unsigned char partly_cloudy_day_rain[] PROGMEM;
extern const unsigned char partly_cloudy_day_sleet[] PROGMEM;
extern const unsigned char partly_cloudy_day_fog[] PROGMEM;

extern const unsigned char partly_cloudy_night[] PROGMEM;
extern const unsigned char partly_cloudy_night_snow[] PROGMEM;
extern const unsigned char partly_cloudy_night_rain[] PROGMEM;
extern const unsigned char partly_cloudy_night_sleet[] PROGMEM;
extern const unsigned char partly_cloudy_night_fog[] PROGMEM;

extern const unsigned char thunderstorms_day[] PROGMEM;
extern const unsigned char thunderstorms_night[] PROGMEM;

// mapping (no UI)
const unsigned char* mapWeatherIcon(int weather_code, int is_day);