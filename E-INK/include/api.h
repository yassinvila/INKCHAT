#pragma once
#include <Arduino.h>

// ---------------- MTA (storage lives in main.cpp) ----------------
extern char northTrain[5];
extern int  northMin[5];
extern char southTrain[5];
extern int  southMin[5];

// ---------------- Weather (storage lives in main.cpp) -------------
static const int WEATHER_MAX = 72;   // ~3 days hourly

extern int weatherStartIndex;
extern int weatherCount;           // how many hourly items we got (<=72)

extern int wTemp[WEATHER_MAX];     // rounded F
extern float wPrec[WEATHER_MAX];     // inches
extern int wCode[WEATHER_MAX];     // weather_code
extern int wDay[WEATHER_MAX];      // 0/1 is_day

// ---------------- URLs live in main.cpp ----------------
extern const char* MTA_URL;
extern const char* WEATHER_URL;

// ---------------- API functions ----------------
bool mtaFetch();
bool weatherFetch();