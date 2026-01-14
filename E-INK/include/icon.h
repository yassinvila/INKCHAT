#pragma once
#include <Arduino.h>

// 200x200
extern const unsigned char sun_200[] PROGMEM;
extern const unsigned char cloud_sun_200[] PROGMEM;
extern const unsigned char drizzle_sun_200[] PROGMEM;
extern const unsigned char fog_sun_200[] PROGMEM;
extern const unsigned char hail_sun_200[] PROGMEM;
extern const unsigned char lightning_sun_200[] PROGMEM;
extern const unsigned char rain_sun_200[] PROGMEM;
extern const unsigned char snow_sun_200[] PROGMEM;
extern const unsigned char wind_sun_200[] PROGMEM;
extern const unsigned char unknown_200[] PROGMEM;

extern const unsigned char moon_200[] PROGMEM;
extern const unsigned char cloud_moon_200[] PROGMEM;
extern const unsigned char drizzle_moon_200[] PROGMEM;
extern const unsigned char fog_moon_200[] PROGMEM;
extern const unsigned char hail_moon_200[] PROGMEM;
extern const unsigned char lightning_moon_200[] PROGMEM;
extern const unsigned char rain_moon_200[] PROGMEM;
extern const unsigned char snow_moon_200[] PROGMEM;
extern const unsigned char wind_moon_200[] PROGMEM;

// 160x160
extern const unsigned char sun_160[] PROGMEM;
extern const unsigned char cloud_sun_160[] PROGMEM;
extern const unsigned char drizzle_sun_160[] PROGMEM;
extern const unsigned char fog_sun_160[] PROGMEM;
extern const unsigned char hail_sun_160[] PROGMEM;
extern const unsigned char lightning_sun_160[] PROGMEM;
extern const unsigned char rain_sun_160[] PROGMEM;
extern const unsigned char snow_sun_160[] PROGMEM;
extern const unsigned char wind_sun_160[] PROGMEM;
extern const unsigned char unknown_160[] PROGMEM;

extern const unsigned char moon_160[] PROGMEM;
extern const unsigned char cloud_moon_160[] PROGMEM;
extern const unsigned char drizzle_moon_160[] PROGMEM;
extern const unsigned char fog_moon_160[] PROGMEM;
extern const unsigned char hail_moon_160[] PROGMEM;
extern const unsigned char lightning_moon_160[] PROGMEM;
extern const unsigned char rain_moon_160[] PROGMEM;
extern const unsigned char snow_moon_160[] PROGMEM;
extern const unsigned char wind_moon_160[] PROGMEM;

// 48x48
extern const unsigned char sun_48[] PROGMEM;
extern const unsigned char cloud_sun_48[] PROGMEM;
extern const unsigned char drizzle_sun_48[] PROGMEM;
extern const unsigned char fog_sun_48[] PROGMEM;
extern const unsigned char hail_sun_48[] PROGMEM;
extern const unsigned char lightning_sun_48[] PROGMEM;
extern const unsigned char rain_sun_48[] PROGMEM;
extern const unsigned char snow_sun_48[] PROGMEM;
extern const unsigned char wind_sun_48[] PROGMEM;
extern const unsigned char unknown_48[] PROGMEM;

extern const unsigned char moon_48[] PROGMEM;
extern const unsigned char cloud_moon_48[] PROGMEM;
extern const unsigned char drizzle_moon_48[] PROGMEM;
extern const unsigned char fog_moon_48[] PROGMEM;
extern const unsigned char hail_moon_48[] PROGMEM;
extern const unsigned char lightning_moon_48[] PROGMEM;
extern const unsigned char rain_moon_48[] PROGMEM;
extern const unsigned char snow_moon_48[] PROGMEM;
extern const unsigned char wind_moon_48[] PROGMEM;

extern const unsigned char train_north [] PROGMEM;
extern const unsigned char train_south [] PROGMEM;

// mapping functions for different icon sizes
const unsigned char* mapWeatherIcon200(int weather_code, int is_day);
const unsigned char* mapWeatherIcon160(int weather_code, int is_day);
const unsigned char* mapWeatherIcon48(int weather_code, int is_day);

// legacy 200x200 mapping (for backward compat)
const unsigned char* mapWeatherIcon(int weather_code, int is_day);
