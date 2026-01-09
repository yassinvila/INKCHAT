#include <Arduino.h>
#include "icon.h"

static inline bool codeIsThunder(int c) {
  return (c == 95 || c == 96 || c == 99);
}
static inline bool codeIsSnow(int c) {
  return (c >= 71 && c <= 77) || (c == 85 || c == 86);
}
static inline bool codeIsRain(int c) {
  return (c >= 51 && c <= 67) || (c >= 80 && c <= 82);
}
static inline bool codeIsFog(int c) {
  return (c == 45 || c == 48);
}
static inline bool codeIsCloudy(int c) {
  return (c >= 1 && c <= 3);
}
const unsigned char* mapWeatherIcon(int weather_code, int is_day) {
  const bool day = (is_day != 0);

  if (codeIsThunder(weather_code)) {
    return day ? thunderstorms_day : thunderstorms_night;
  }

  if (codeIsFog(weather_code)) {
    return day ? partly_cloudy_day_fog : partly_cloudy_night_fog;
  }

  if (codeIsSnow(weather_code)) {
    if (day) return partly_cloudy_day_snow;
    return partly_cloudy_night_snow;
  }

  if (codeIsRain(weather_code)) {
    if (day) return partly_cloudy_day_rain;
    return partly_cloudy_night_rain;
  }

  if (weather_code == 0) {
    return day ? clear_day : clear_night;
  }

  if (codeIsCloudy(weather_code)) {
    return day ? partly_cloudy_day : partly_cloudy_night;
  }

  // unknown or "empty slot" placeholder
  return clear_night;
}