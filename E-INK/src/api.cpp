#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "api.h"

// Static JSON documents (allocated once at startup, reused forever)
// Eliminates heap fragmentation from repeated malloc/free cycles
static StaticJsonDocument<30000> weatherDoc;

// -------------------- MTA --------------------
bool mtaFetch() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("MTA Not Connected");
    return false;
  }

  HTTPClient http;
  http.begin(MTA_URL);

  int code = http.GET();
  if (code != 200) {
    Serial.print("MTA Error: ");
    Serial.println(code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<4096> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("MTA JSON parse error: ");
    Serial.println(err.c_str());
    return false;
  }

  JsonArray north = doc["north"].as<JsonArray>();
  JsonArray south = doc["south"].as<JsonArray>();

  for (int i = 0; i < 5; i++) {
    northTrain[i] = '?';
    northMin[i] = -1;
    southTrain[i] = '?';
    southMin[i] = -1;

    if (i < (int)north.size()) {
      int minutes = north[i]["minutes"] | -1;
      const char* trainStr = north[i]["train"] | "?";
      northMin[i] = minutes;
      northTrain[i] = (trainStr && trainStr[0]) ? trainStr[0] : '?';
    }

    if (i < (int)south.size()) {
      int minutes = south[i]["minutes"] | -1;
      const char* trainStr = south[i]["train"] | "?";
      southMin[i] = minutes;
      southTrain[i] = (trainStr && trainStr[0]) ? trainStr[0] : '?';
    }
  }

  Serial.println("MTA OK");
  return true;
}


// -------------------- Weather --------------------
bool weatherFetch() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Weather Not Connected");
    return false;
  }

  HTTPClient http;
  http.begin(WEATHER_URL);

  int code = http.GET();
  if (code != 200) {
    Serial.print("Weather Error: ");
    Serial.println(code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  // Clear previous data and reuse static document (no heap fragmentation)
  weatherDoc.clear();

  DeserializationError err = deserializeJson(weatherDoc, payload);
  if (err) {
    Serial.print("Weather JSON parse error: ");
    Serial.println(err.c_str());
    return false;
  }

  // startIndex
  weatherStartIndex = weatherDoc["startIndex"] | 0;

  JsonArray hourly = weatherDoc["hourly"].as<JsonArray>();
  int n = (int)hourly.size();
  if (n > WEATHER_MAX) n = WEATHER_MAX;
  weatherCount = n;

  for (int i = 0; i < WEATHER_MAX; i++) {
    wTemp[i] = 0;
    wPrec[i] = 0.0f;
    wCode[i] = 0;
    wDay[i]  = 0;
  }

  for (int i = 0; i < n; i++) {
    JsonObject h = hourly[i].as<JsonObject>();

    wTemp[i] = h["temp"] | 0;
    wPrec[i] = h["prec"] | 0.0f;
    wDay[i]  = h["day"]  | 0;
    wCode[i] = h["code"] | 0;
  }

  Serial.println("Weather OK");
  return true;
}