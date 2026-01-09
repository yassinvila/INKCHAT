#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <time.h>

#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include "api.h"
#include "icon.h"


// ------------------------------- PINS ----------------------------- //
static const int EPD_CS   = 5;
static const int EPD_DC   = 14;
static const int EPD_RST  = 16;  // if boot issues: change to 16 and rewire
static const int EPD_BUSY = 4;

// ------------------------------- WiFi ----------------------------- //
// WiFi credentials: set via platformio.ini build_flags or .env file
// (See .env.example for setup instructions)
#ifndef WIFI_SSID
#define WIFI_SSID "SET_WIFI_SSID"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "SET_WIFI_PASSWORD"
#endif

const char* SSID = WIFI_SSID;
const char* PASSWORD = WIFI_PASSWORD;

// ------------------------------- LINKS ----------------------------- //
static const char* TIMEZONE = "EST5EDT,M3.2.0/2,M11.1.0/2";
const char* MTA_URL = "http://192.168.1.205:8787/mta";
const char* WEATHER_URL = "http://192.168.1.205:8787/weather";

// ------------------------------- FONT ----------------------------- //
static const GFXfont* FONT = &FreeMonoBold9pt7b;
static const GFXfont* FONT_BIG = &FreeMonoBold24pt7b;

// ------------------------------- MTA (storage) ----------------------------- //
static const int MAX_ARR = 5;
char northTrain[MAX_ARR] = {'?','?','?','?','?'};
int  northMin[MAX_ARR]   = {0,0,0,0,0};
char southTrain[MAX_ARR] = {'?','?','?','?','?'};
int  southMin[MAX_ARR]   = {0,0,0,0,0};

// ------------------------------- WEATHER (storage) -------------------------- //
int   weatherStartIndex = 0;
int   weatherCount = 0;
int   wTemp[WEATHER_MAX] = {0};
float wPrec[WEATHER_MAX] = {0};
int   wCode[WEATHER_MAX] = {0};
int   wDay[WEATHER_MAX]  = {0};

// ------------------------------- SCREENS ----------------------------- //
enum Screen : uint8_t { SCREEN_TIME, SCREEN_MTA, SCREEN_WEATHER };
static Screen currentScreen = SCREEN_TIME;

static unsigned long lastSwitchMs = 0;
static const unsigned long SWITCH_EVERY_MS = 60000; // 1 minute

// ---------------- WEATHER paging (30s shift) ----------------
static uint8_t weatherPage = 0;
static unsigned long lastWeatherFlipMs = 0;
static const unsigned long WEATHER_FLIP_EVERY_MS = 20000; // 20 seconds

static const int SCREEN_W = 800;
static const int SCREEN_H = 480;

static const int HALF_H   = 240;

// Manual screen control via serial "encoder"
static int selectedScreen = 0;          // 0 TIME, 1 MTA, 2 WEATHER
static bool manualMode = true;          // disable auto-rotation when true

// Icon boxes (160x160)
static const int ICON_W   = 160;
static const int ICON_H   = 160;
static const int ICON_X   = 20;

// Route area start (right side of icon)
static const int ROUTE_X  = 220;

// Dot geometry
static const int DOT_R = 6;

// Hard-coded dot positions (5 dots)
static const int DOT_X[MAX_ARR] = {330, 430, 510, 620, 730};

// Per-half vertical layout
static const int TOP_Y0 = 0;
static const int BOT_Y0 = 240;

// Where the route line sits in each half
static const int ROUTE_Y_TOP = 140;
static const int ROUTE_Y_BOT = 380;

// Icon Y per half
static const int ICON_Y_TOP = 60;
static const int ICON_Y_BOT = 300;

// Text offsets around dot
static const int TRAIN_TEXT_DY = -18;
static const int MIN_TEXT_DY   =  26;

// WEATHER layout (200x200 everywhere for now)
static const int ICON_SIDE_W = 200;
static const int ICON_SIDE_H = 200;
static const int ICON_MID_W  = 200;
static const int ICON_MID_H  = 200;

// x positions for 3x 200px icons on 800px width
static const int ICON_L_X = 40;   // 40..240
static const int ICON_M_X = 300;  // 300..500
static const int ICON_R_X = 560;  // 560..760

// y positions
static const int ICON_SIDE_Y = 45;
static const int ICON_MID_Y  = 45;

// text under icons (unused for now; keep if you want)
static const int ICON_TEXT_Y0 = 255;

// middle 6-hour rows area
static const int ROWS_X = 240;
static const int ROWS_Y = 280;
// ---------------- 7.5" 800x480 Good Display (UC8179) -------------- //
GxEPD2_BW<GxEPD2_750_GDEY075T7, GxEPD2_750_GDEY075T7::HEIGHT> display(
  GxEPD2_750_GDEY075T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// --------------------------- SETUP FUNCTION ----------------------- //
void displayInit();
bool wifiConnect();
bool timeSync();
String getTime();

// ------------------------- SCREEN FUNCTIONS ---------------------- //
void drawTimeScreen();
void updateTimePartialEveryMinute();

void drawMTAScreen();
void updateMtaDotsPartial();

void drawWeatherScreen();
void updateWeatherPartial();

static void drawMtaHalf(int y0, bool isNorth);

static int safeIdx(int idx);
static bool hasIdx(int idx);
static int dayBaseIndexFromPage(uint8_t page);
static void drawXbmCrop(int x, int y, const unsigned char* bmp, int srcW, int srcH, int drawW, int drawH);
static void drawTopIconBlock(int x, int y, int w, int h, int dayBaseIdx, bool emptySlot);
static void drawSixHourRows(int startIdx);
static void handleSerialEncoder();
static void goToScreen(Screen s);

// ------------------------------- SETUP ----------------------------- //
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("BOARD CONNECTED");

  displayInit();
  if (wifiConnect()) {
    timeSync();
  }

  drawTimeScreen();
}

// ------------------------------- LOOP ------------------------------ //
void loop() {
  unsigned long now = millis();

  handleSerialEncoder();

  if (!manualMode && (now - lastSwitchMs >= SWITCH_EVERY_MS)) {
    lastSwitchMs = now;

    if (currentScreen == SCREEN_TIME) {
      currentScreen = SCREEN_MTA;

      drawMTAScreen();
      if (mtaFetch()) {
        updateMtaDotsPartial();
      }
    }
    else if (currentScreen == SCREEN_MTA) {
      currentScreen = SCREEN_WEATHER;

      weatherFetch();
      drawWeatherScreen();

      weatherPage = 0;
      lastWeatherFlipMs = now;
      updateWeatherPartial();
    }
    else {
      currentScreen = SCREEN_TIME;
      drawTimeScreen();
    }
  }

  if (currentScreen == SCREEN_TIME) {
    updateTimePartialEveryMinute();
  }

  if (currentScreen == SCREEN_WEATHER) {
    if (now - lastWeatherFlipMs >= WEATHER_FLIP_EVERY_MS) {
      lastWeatherFlipMs = now;
      weatherPage = (weatherPage + 1) % 3;
      updateWeatherPartial();
    }
  }

  delay(50);
}


// --------------------------- DISPLAY INIT -------------------------- //
void displayInit() {
  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();
  Serial.println("Display Good");
}

// --------------------------- WIFI CONNECT -------------------------- //
bool wifiConnect() {
  WiFi.mode(WIFI_STA);

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(FONT);

    display.setCursor(330, 20);
    display.print("SETUP START");
    display.drawLine(0, 30, 799, 30, GxEPD_BLACK);

    display.setCursor(300, 220);
    display.print("Testing Connection");
    display.setCursor(320, 245);
    display.print("Connecting...");
  } while (display.nextPage());

  display.setPartialWindow(0, 200, 800, 120);

  for (int attempt = 1; attempt <= 3; attempt++) {
    WiFi.begin(SSID, PASSWORD);

    unsigned long start = millis();
    while (!WiFi.isConnected() && millis() - start < 8000) {
      delay(100);
      Serial.print(".");
    }
    Serial.println();

    bool ok = WiFi.isConnected();

    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      display.setTextColor(GxEPD_BLACK);
      display.setFont(FONT);

      display.setCursor(300, 220);
      display.print("Testing Connection");

      if (ok) {
        display.setCursor(330, 245);
        display.print("Success");
      } else {
        if (attempt < 3) {
          display.setCursor(290, 245);
          display.print("Failed. Trying Again");
        } else {
          display.setCursor(275, 245);
          display.print("Failed. Bro Wifi is Cooked.");
        }
      }
    } while (display.nextPage());

    if (ok) {
      Serial.println(WiFi.localIP());
      Serial.println(WiFi.RSSI());
      return true;
    }

    delay(800);
  }

  return false;
}

// --------------------------- TIME SYNC ----------------------------- //
bool timeSync() {
  configTzTime(TIMEZONE, "pool.ntp.org","time.nist.gov", "time.google.com");

  unsigned long start = millis();
  struct tm timeinfo;

  while (millis() - start < 8000) {
    if (getLocalTime(&timeinfo, 200)) {
      Serial.println("Time Recieved");
      return true;
    }
    delay(100);
  }

  Serial.println("Time Not Recieved");
  return false;
}

String getTime() {
  struct tm tm_info;

  // If NTP time isn't ready yet
  if (!getLocalTime(&tm_info, 200)) {
    return "--:--";
  }

  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
  return String(buf);
}

void drawTimeScreen() {
  display.setFullWindow();

  String timeStr = getTime();

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    display.setFont(FONT);
    display.setCursor(330, 20);
    display.print("TIME");
    display.drawLine(0, 30, 799, 30, GxEPD_BLACK);

    display.setCursor(310, 120);
    display.print("Hello, Yassin!");

    display.drawRect(200, 140, 400, 200, GxEPD_BLACK);

    display.setFont(FONT_BIG);
    display.setCursor(300, 260);
    display.print(timeStr);
  } while (display.nextPage());
}

void updateTimePartialEveryMinute() {
  static int lastMinute = -1;

  struct tm tm_info;
  if (!getLocalTime(&tm_info, 200)) return;

  int currentMinute = tm_info.tm_min;
  if (currentMinute == lastMinute) return;
  lastMinute = currentMinute;

  // Partial window = the big box region (plus a little padding)
  display.setPartialWindow(195, 135, 410, 210);

  String t = getTime();

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    // Redraw the box border (because we cleared the partial area)
    display.drawRect(200, 140, 400, 200, GxEPD_BLACK);

    // Redraw the time (big)
    display.setFont(FONT_BIG);
    display.setCursor(300, 260);
    display.print(t);

  } while (display.nextPage());
}

static void drawMtaHalf(int y0, bool isNorth)
{
  // Title
  display.setFont(FONT);
  display.setTextColor(GxEPD_BLACK);

  if (isNorth) {
    display.setCursor(20, y0 + 55);
    display.print("Northbound");
  } else {
    display.setCursor(20, y0 + 55);
    display.print("Southbound");
  }

  // Icon placeholder box (160x160)
  int iconY = isNorth ? ICON_Y_TOP : ICON_Y_BOT;
  display.drawRect(ICON_X, iconY, ICON_W, ICON_H, GxEPD_BLACK);

  // ------------------- Route "station" marker like: *\  \_  -------------------
  int routeY = isNorth ? ROUTE_Y_TOP : ROUTE_Y_BOT;

  // Star (station) as text
  display.setFont(FONT);
  display.setCursor(ROUTE_X, routeY - 10);
  display.print("*");

  // Slanted "\" from star down-right
  display.drawLine(ROUTE_X + 8, routeY - 8, ROUTE_X + 28, routeY + 12, GxEPD_BLACK);

  // Small "_" (horizontal) after the slash
  display.drawLine(ROUTE_X + 28, routeY + 12, ROUTE_X + 55, routeY + 12, GxEPD_BLACK);

  // Main track line to the first dot
  display.drawLine(ROUTE_X + 55, routeY + 12, DOT_X[0] - DOT_R - 8, routeY + 12, GxEPD_BLACK);

  // Dots + connecting segments (like ". ____ . ____ .")
  for (int i = 0; i < MAX_ARR; i++) {
    int x = DOT_X[i];
    int y = routeY + 12;

    // Dot (.)
    display.drawCircle(x, y, DOT_R, GxEPD_BLACK);

    // Connect to next dot
    if (i < MAX_ARR - 1) {
      display.drawLine(x + DOT_R, y, DOT_X[i + 1] - DOT_R, y, GxEPD_BLACK);
    }

    // Train letter above dot + minutes below dot
    display.setFont(FONT);

    if (isNorth) {
      // Train above
      display.setCursor(x - 3, y + TRAIN_TEXT_DY);
      display.print(northTrain[i]);

      // Minutes below
      display.setCursor(x - 10, y + MIN_TEXT_DY);
      display.print(northMin[i]);
    } else {
      display.setCursor(x - 3, y + TRAIN_TEXT_DY);
      display.print(southTrain[i]);

      display.setCursor(x - 10, y + MIN_TEXT_DY);
      display.print(southMin[i]);
    }
  }
}

void drawMTAScreen()
{
  display.setFullWindow();

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(FONT);

    // Header top centered-ish (hard-coded)
    display.setCursor(370, 20);
    display.print("MTA");
    display.drawLine(0, 30, 799, 30, GxEPD_BLACK);

    // Split line across middle (horizontal)
    display.drawLine(0, 239, 799, 239, GxEPD_BLACK);

    // Top half (Northbound)
    drawMtaHalf(TOP_Y0, true);

    // Bottom half (Southbound)
    drawMtaHalf(BOT_Y0, false);

  } while (display.nextPage());
}

// ------------------- PARTIAL UPDATE: ONLY THE ROUTE/DOTS AREA (both halves) -------------------
void updateMtaDotsPartial() {
  // Route area bounds (covers both halves route area, not headers, not icon boxes)
  // X: start at ROUTE_X-10, width to end
  // Y: from top route band down to bottom route band region
  const int PX = ROUTE_X - 10;
  const int PY = 80;
  const int PW = SCREEN_W - PX;
  const int PH = SCREEN_H - PY - 20;

  display.setPartialWindow(PX, PY, PW, PH);

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    // Redraw the route + dots + text for both halves (inside the partial region)
    drawMtaHalf(TOP_Y0, true);
    drawMtaHalf(BOT_Y0, false);

  } while (display.nextPage());
}

// --------------------------- WEATHER SCREEN ------------------------ //
static int safeIdx(int idx) {
  if (weatherCount <= 0) return 0;
  if (idx < 0) return 0;
  if (idx >= weatherCount) return weatherCount - 1;
  return idx;
}

static bool hasIdx(int idx) {
  return (idx >= 0 && idx < weatherCount);
}

static int dayBaseIndexFromPage(uint8_t page) {
  int dayOffset = 0;
  if (page == 0) dayOffset = 0;
  else if (page == 1) dayOffset = 1;
  else dayOffset = 2;

  return safeIdx(weatherStartIndex + (dayOffset * 24));
}

// Draw cropped region from an XBM-style 1bpp bitmap (LSB-first).
// INVERTED POLARITY: 0-bits = black pixels (icon), 1-bits = white (background).
static void drawXbmCrop(int x, int y, const unsigned char* bmp, int srcW, int srcH, int drawW, int drawH) {
  const int srcBytesPerRow = (srcW + 7) / 8;
  const int h = (drawH > srcH) ? srcH : drawH;
  const int w = (drawW > srcW) ? srcW : drawW;
  for (int row = 0; row < h; row++) {
    const int rowOffset = row * srcBytesPerRow;
    for (int col = 0; col < w; col++) {
      const int byteIndex = rowOffset + (col >> 3);
      uint8_t byteVal = pgm_read_byte(&bmp[byteIndex]);
      bool bitSet = byteVal & (1 << (col & 7)); // LSB-first (XBM)
      if (!bitSet) {  // Draw black when bit is 0 (inverted polarity)
        display.drawPixel(x + col, y + row, GxEPD_BLACK);
      }
    }
  }
}

static void drawTopIconBlock(int x, int y, int w, int h, int dayBaseIdx, bool emptySlot) {
  display.drawRect(x, y, w, h, GxEPD_BLACK);

  // pick a representative hour for the day block (midday-ish)
  int midIdx = dayBaseIdx + 12;

  // empty slot rule OR out-of-range -> placeholder icon
  const unsigned char* bmp = clear_night;
  if (!emptySlot && hasIdx(midIdx)) {
    bmp = mapWeatherIcon(wCode[midIdx], wDay[midIdx]);
  }

  // Draw big icon with inverted polarity (0-bits = black)
  const int srcBytesPerRow = (w + 7) / 8;
  for (int row = 0; row < h; row++) {
    const int rowOffset = row * srcBytesPerRow;
    for (int col = 0; col < w; col++) {
      const int byteIndex = rowOffset + (col >> 3);
      uint8_t byteVal = pgm_read_byte(&bmp[byteIndex]);
      bool bitSet = byteVal & (1 << (col & 7)); // LSB-first
      if (!bitSet) {  // Draw black when bit is 0 (inverted polarity)
        display.drawPixel(x + col, y + row, GxEPD_BLACK);
      }
    }
  }
}

static void drawSixHourRows(int startIdx) {
  display.setFont(FONT);
  display.setTextColor(GxEPD_BLACK);

  // Horizontal tile layout settings
  const int startX = 20;          // left margin for tiles area
  const int startY = ROWS_Y;      // top margin (constant Y for all tiles)
  const int tileW = 120;          // width of each tile
  const int tileH = 120;          // height of each tile
  const int gap = 10;             // horizontal gap between tiles
  const int iconSize = 50;        // icon side inside the tile

  // Offsets within the tile
  const int timeOffsetX = 8;
  const int timeOffsetY = 18;
  const int iconOffsetY = 40;     // vertical position of icon top inside the tile
  const int textOffsetX = 8;
  const int textOffsetY = iconOffsetY + iconSize + 20; // below icon

  const bool DEBUG_WEATHER_CARDS = true; // draw tile and icon rectangles

  // Draw 6 tiles with 4-hour stepping
  for (int i = 0; i < 6; i++) {
    // Compute dataIndex: 4-hour steps (0, 4, 8, 12, 16, 20)
    int dataIdx = startIdx + (i * 4);
    
    // Bounds check: skip if out of range (weatherCount is per-day slice, check absolute bounds)
    if (dataIdx < 0 || dataIdx >= WEATHER_MAX) continue;

    // Horizontal placement: same Y, increment X per tile
    int tileX = startX + i * (tileW + gap);
    int tileY = startY;

    if (DEBUG_WEATHER_CARDS) {
      display.drawRect(tileX, tileY, tileW, tileH, GxEPD_BLACK);
    }

    // LABEL: (i+1)*4 → 04:00, 08:00, 12:00, 16:00, 20:00, 24:00
    int hoursAhead = (i + 1) * 4;
    display.setCursor(tileX + timeOffsetX, tileY + timeOffsetY);
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:00", hoursAhead);
    display.print(buf);

    // ICON (centered in tile)
    const unsigned char* rbmp = mapWeatherIcon(wCode[dataIdx], wDay[dataIdx]);
    int iconX = tileX + (tileW - iconSize) / 2;
    int iconY = tileY + iconOffsetY;
    if (DEBUG_WEATHER_CARDS) {
      display.drawRect(iconX, iconY, iconSize, iconSize, GxEPD_BLACK);
    }
    drawXbmCrop(iconX, iconY, rbmp, 200, 200, iconSize, iconSize);

    // TEMP + PRECIP
    display.setCursor(tileX + textOffsetX, tileY + textOffsetY);
    display.print(wTemp[dataIdx]);
    display.print("F  ");
    display.print(wPrec[dataIdx], 2);
    display.print("in");
  }
}

void drawWeatherScreen() {
  display.setFullWindow();

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(FONT);

    display.setCursor(345, 20);
    display.print("WEATHER");
    display.drawLine(0, 30, 799, 30, GxEPD_BLACK);
  } while (display.nextPage());

  updateWeatherPartial();
}

void updateWeatherPartial() {
  display.setPartialWindow(0, 35, 800, 445);

  int baseToday    = 0;
  int baseTomorrow = 24;
  int baseFollow   = 48;

  // default assignment (will be overridden)
  int leftBase  = baseToday;
  int midBase   = baseToday;
  int rightBase = baseTomorrow;

  bool leftEmpty  = false;
  bool midEmpty   = false;
  bool rightEmpty = false;

  // page 0: EMPTY | TODAY | TOMORROW
  // page 1: TODAY | TOMORROW | FOLLOW
  // page 2: TOMORROW | FOLLOW | EMPTY
  if (weatherPage == 0) {
    leftEmpty = true;

    midBase = baseToday;
    rightBase = baseTomorrow;
    if (!hasIdx(midBase + 12)) midEmpty = true;
    if (!hasIdx(rightBase + 12)) rightEmpty = true;

  } else if (weatherPage == 1) {
    leftBase  = baseToday;
    midBase   = baseTomorrow;
    rightBase = baseFollow;

    if (!hasIdx(leftBase + 12)) leftEmpty = true;
    if (!hasIdx(midBase + 12))  midEmpty = true;
    if (!hasIdx(rightBase + 12)) rightEmpty = true;

  } else {
    rightEmpty = true;

    leftBase = baseTomorrow;
    midBase  = baseFollow;

    if (!hasIdx(leftBase + 12)) leftEmpty = true;
    if (!hasIdx(midBase + 12))  midEmpty = true;
  }

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    drawTopIconBlock(ICON_L_X, ICON_SIDE_Y, ICON_SIDE_W, ICON_SIDE_H, leftBase, leftEmpty);
    drawTopIconBlock(ICON_M_X, ICON_MID_Y,  ICON_MID_W,  ICON_MID_H,  midBase,  midEmpty);
    drawTopIconBlock(ICON_R_X, ICON_SIDE_Y, ICON_SIDE_W, ICON_SIDE_H, rightBase, rightEmpty);

    drawSixHourRows(midBase);
  } while (display.nextPage());
}

static void goToScreen(Screen s) {
  currentScreen = s;

  if (currentScreen == SCREEN_TIME) {
    drawTimeScreen();
  } else if (currentScreen == SCREEN_MTA) {
    drawMTAScreen();
    if (mtaFetch()) updateMtaDotsPartial();
  } else { // SCREEN_WEATHER
    weatherFetch();
    drawWeatherScreen();
    weatherPage = 0;
    lastWeatherFlipMs = millis();
    updateWeatherPartial();
  }
}

static void handleSerialEncoder() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r') continue;

    if (c == 'a') {
      selectedScreen = (selectedScreen + 2) % 3; // -1 mod 3
      Serial.println("ENC: LEFT");
      goToScreen((Screen)selectedScreen);
      lastSwitchMs = millis();
    }
    else if (c == 'd') {
      selectedScreen = (selectedScreen + 1) % 3;
      Serial.println("ENC: RIGHT");
      goToScreen((Screen)selectedScreen);
      lastSwitchMs = millis();
    }
    else if (c == 's') {
      Serial.println("ENC: PRESS");
      goToScreen((Screen)selectedScreen);
      lastSwitchMs = millis(); // reset auto timer so it doesn't instantly switch
    }
  }
}

