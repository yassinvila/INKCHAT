#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>

#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include "api.h"
#include "icon.h"


// ------------------------------- PINS ----------------------------- //
static const int ENC_SW  = 27;  // rotary encoder switch button
static const int ENC_CLK = 33;  // rotary encoder CLK
static const int ENC_DT  = 32;  // rotary encoder DT
static const int EPD_CS   = 5;
static const int EPD_DC   = 14;
static const int EPD_RST  = 16;  // if boot issues: change to 16 and rewire
static const int EPD_BUSY = 4;

// ------------------------------- WiFi ----------------------------- //
// Fallback hardcoded WiFi credentials (used if provisioning fails)
const char* FALLBACK_SSID = "My 3G";
const char* FALLBACK_PASSWORD = "newyork@10";

// WiFi Provisioning AP settings
static const char* AP_SSID = "ESP32-SETUP";
static const char* AP_PASS = "pitchfest"; // Must be >= 8 chars
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

WebServer server(80);
Preferences prefs;
bool apModeActive = false;

// ------------------------------- LINKS ----------------------------- //
static const char* TIMEZONE = "EST5EDT,M3.2.0/2,M11.1.0/2";
const char* MTA_URL = "https://inkchat-ruby.vercel.app/api/mta";
const char* WEATHER_URL = "https://inkchat-ruby.vercel.app/api/weather";

// ------------------------------- FONT ----------------------------- //
static const GFXfont* FONT = &FreeMonoBold9pt7b;
static const GFXfont* FONT_BIG = &FreeMonoBold24pt7b;
static const GFXfont* FONT_MED = &FreeMonoBold12pt7b;

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
int   wCode[WEATHER_MAX];  // Initialized to -1 in setup()
int   wDay[WEATHER_MAX]  = {0};

// ------------------------------- SCREENS ----------------------------- //
enum Screen : uint8_t { SCREEN_TIME, SCREEN_MTA, SCREEN_WEATHER };
static Screen currentScreen = SCREEN_TIME;

// Manual navigation state: 0=TIME, 1=MTA, 2=WEATHER page0, 3=WEATHER page1, 4=WEATHER page2
static int navState = 0;

static unsigned long lastSwitchMs = 0;
static const unsigned long SWITCH_EVERY_MS = 60000; // 1 minute

// ---------------- WEATHER paging (30s shift) ----------------
static uint8_t weatherPage = 0;
static unsigned long lastWeatherFlipMs = 0;
static const unsigned long WEATHER_FLIP_EVERY_MS = 20000; // 20 seconds

static const int SCREEN_W = 800;
static const int SCREEN_H = 480;

static const int HALF_H   = 240;

// Manual screen control via button
static bool manualMode = false;          // disable auto-rotation when true

// Icon boxes (for MTA screen)
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

// WEATHER layout icon sizes
static const int ICON_SIDE_W = 160;  // Left and right blocks
static const int ICON_SIDE_H = 160;
static const int ICON_MID_W  = 200;  // Middle block
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

// Button press navigation: 1 press = next screen, 2 presses = previous screen
static const unsigned long DOUBLE_PRESS_WINDOW_MS = 1200;  // 1200ms window for double press
// ---------------- 7.5" 800x480 Good Display (UC8179) -------------- //
GxEPD2_BW<GxEPD2_750_GDEY075T7, GxEPD2_750_GDEY075T7::HEIGHT> display(
  GxEPD2_750_GDEY075T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// --------------------------- SETUP FUNCTION ----------------------- //
void displayInit();
bool wifiConnect();
bool connectWiFiSTA(const char* ssid, const char* pass, uint32_t timeoutMs);
void startAPMode();
void saveCreds(const String& ssid, const String& pass);
bool loadCreds(String& ssidOut, String& passOut);
void clearCreds();
void handleRoot();
void handleSave();
void handleClear();
bool timeSync();
String getTime();

// --------------------------- SCREEN FUNCTIONS ---------------------- //
void drawBootLogo();
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
static void draw1bppWhiteOnBlack(int x, int y, int w, int h, const unsigned char* bmp);
static void drawTopIconBlock(int x, int y, int w, int h, int dayBaseIdx, bool emptySlot, bool forceMoon);
static void drawSixHourRows(int startIdx);
static void handleSerialEncoder();
static int8_t read_rotary();
static void goToScreen(Screen s);
static void applyNavState();

// ------------------------------- SETUP ----------------------------- //
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("BOARD CONNECTED");
  Serial.println("Type 'CLEAR_WIFI' in serial monitor to clear saved WiFi credentials for testing");
  pinMode(ENC_SW, INPUT_PULLUP);
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  
  // Initialize all weather codes to -1 (no data)
  for (int i = 0; i < WEATHER_MAX; i++) {
    wCode[i] = -1;
  }
  
  displayInit();
  drawBootLogo();
  
  if (wifiConnect()) {
    timeSync();
  }

  // Only draw time screen if not in AP mode (WiFi setup)
  if (!apModeActive) {
    drawTimeScreen();
  }
}

// ------------------------------- LOOP ------------------------------ //
void loop() {
  unsigned long now = millis();

  // Handle serial commands (for testing/development)
  if (Serial.available()) {
    char ch = Serial.read();
    Serial.print("[SERIAL] Received char: '");
    Serial.print(ch);
    Serial.print("' (0x");
    Serial.print((int)ch, HEX);
    Serial.println(")");

    if (ch == 'W' || ch == 'w') {
      Serial.println("[SERIAL] CLEAR_WIFI command recognized");
      clearCreds();
      Serial.println("WiFi credentials cleared. Rebooting...");
      delay(1000);
      ESP.restart();
    }
    else if (ch == 'D' || ch == 'd') {
      Serial.println("[SERIAL] PIN_DEBUG command recognized");
      Serial.println("Pin debug mode: Reading pins for 10 seconds (ROTATE ENCODER NOW)...");
      int lastCLK = digitalRead(ENC_CLK);
      int lastDT = digitalRead(ENC_DT);
      for (int i = 0; i < 100; i++) {
        int clk = digitalRead(ENC_CLK);
        int dt = digitalRead(ENC_DT);
        int sw = digitalRead(ENC_SW);
        
        if (clk != lastCLK || dt != lastDT || sw == LOW) {
          Serial.print("[");
          Serial.print(i * 100);
          Serial.print("ms] SW=");
          Serial.print(sw == LOW ? "LOW" : "HIGH");
          Serial.print(" | CLK=");
          Serial.print(clk == LOW ? "LOW" : "HIGH");
          if (clk != lastCLK) Serial.print("*");
          Serial.print(" | DT=");
          Serial.print(dt == LOW ? "LOW" : "HIGH");
          if (dt != lastDT) Serial.print("*");
          Serial.println();
          lastCLK = clk;
          lastDT = dt;
        }
        delay(100);
      }
      Serial.println("Pin debug complete.");
    }
    else {
      Serial.println("[SERIAL] Unknown command. Use 'W' to clear WiFi or 'D' for pin debug.");
    }
  }

  // Handle web server in AP mode
  if (apModeActive) {
    server.handleClient();
    return; // Don't run normal display logic in AP mode
  }

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
    else if (currentScreen == SCREEN_WEATHER) {
      currentScreen = SCREEN_TIME;
      drawTimeScreen();
    }
  }

  if (currentScreen == SCREEN_TIME) {
    updateTimePartialEveryMinute();
  }

  if (!manualMode && currentScreen == SCREEN_WEATHER) {
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

// --------------------------- BOOT LOGO ANIMATION ------------------- //
void drawBootLogo() {
  display.setFullWindow();
  display.setFont(FONT_BIG);
  display.setTextColor(GxEPD_BLACK);
  
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(280, 240);
    display.print("INK-HAT");
  } while (display.nextPage());
  
  delay(1000);  // Hold the splash for 1 second
}

// --------------------------- WIFI CONNECT -------------------------- //
bool wifiConnect() {
  String ssid, pass;
  
  // TESTING: Skip saved credentials, always go to AP setup
  Serial.println("TESTING MODE: Skipping saved credentials. Starting AP setup...");
  startAPMode();
  return false;
  
  // Re-enable this code after testing
  /*
  // Try saved credentials once; otherwise go straight to AP setup
  if (loadCreds(ssid, pass)) {
    Serial.print("Found saved SSID: ");
    Serial.println(ssid);
    if (connectWiFiSTA(ssid.c_str(), pass.c_str(), WIFI_CONNECT_TIMEOUT_MS)) {
      Serial.println("Connected with saved credentials");
      return true;
    }
    Serial.println("Saved credentials failed. Going to AP setup...");
  } else {
    Serial.println("No saved credentials. Starting AP setup...");
  }

  startAPMode();
  return false;
  */
}

// Connect to WiFi in STA mode with timeout and display feedback
bool connectWiFiSTA(const char* ssid, const char* pass, uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect(true, true);
  delay(200);

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

  for (int attempt = 1; attempt <= 2; attempt++) {
    WiFi.begin(ssid, pass);

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
        display.setCursor(345, 245);
        display.print("Success");
      } else {
        if (attempt < 2) {
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

  while (millis() - start < 5000) {
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
    display.setCursor(360, 20);
    display.print("TIME");
    display.drawLine(0, 30, 799, 30, GxEPD_BLACK);

    display.setFont(FONT_MED);
    display.setCursor(290, 130);
    display.print("Hello, PitchFest!");
    
    display.setFont(FONT_BIG);
    display.setTextSize(2);  // make time larger
    display.setCursor(250, 250);
    display.print(timeStr);
    display.setTextSize(1);  // reset size for other text
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
    
    // Redraw the time (big)
    display.setFont(FONT_BIG);
    display.setTextSize(2);  // larger time readout
    display.setCursor(250, 250);
    display.print(t);
    display.setTextSize(1);

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

  // Icon placeholder box (160x160) with train icon
  int iconY;
  if (isNorth) {
    iconY = ICON_Y_TOP;
  } else {
    iconY = ICON_Y_BOT;
  }
  display.drawRect(ICON_X, iconY, ICON_W, ICON_H, GxEPD_BLACK);
  
  // Draw train icon inside the box
  const unsigned char* trainIcon = isNorth ? train_north : train_south;
  if (trainIcon != nullptr) {
    // Center the 160x160 icon in the 160x160 box
    draw1bppWhiteOnBlack(ICON_X, iconY, ICON_W, ICON_H, trainIcon);
  }

  // ------------------- Route "station" marker like: *\  \_  -------------------
  int routeY;
  if (isNorth) {
    routeY = ROUTE_Y_TOP;
  } else {
    routeY = ROUTE_Y_BOT;
  }

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
    display.print("THE N TRAIN");
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
  const int PY = 70;  // moved up to include the middle line at Y=239
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

// Helper: Draw 1bpp bitmap as BLACK on WHITE
static void draw1bppWhiteOnBlack(int x, int y, int w, int h, const unsigned char* bmp) {
  if (bmp == nullptr) return;
  display.drawBitmap(x, y, bmp, w, h, GxEPD_BLACK, GxEPD_WHITE);
}

static void drawTopIconBlock(int x, int y, int w, int h, int dayBaseIdx, bool emptySlot, bool forceMoon) {
  // If forceMoon is true, show unknown icon (no data)
  if (forceMoon) {
    // Clear the area first
    display.fillRect(x, y, w, h, GxEPD_WHITE);
    
    const unsigned char* bmp;
    if (w == 200 && h == 200) {
      bmp = unknown_200;
    } else if (w == 160 && h == 160) {
      bmp = unknown_160;
    } else {
      bmp = unknown_200;
    }
    
    if (bmp != nullptr) {
      draw1bppWhiteOnBlack(x, y, w, h, bmp);
    }
    return;
  }

  // Pick representative hour for day block (midday-ish: +12 hours)
  int midIdx = dayBaseIdx + 12;

  // Empty slot or out-of-range: show unknown icon instead of blank
  if (emptySlot || !hasIdx(midIdx)) {
    display.fillRect(x, y, w, h, GxEPD_WHITE);
    
    const unsigned char* bmp;
    if (w == 200 && h == 200) {
      bmp = unknown_200;
    } else if (w == 160 && h == 160) {
      bmp = unknown_160;
    } else {
      bmp = unknown_200;
    }
    
    if (bmp != nullptr) {
      draw1bppWhiteOnBlack(x, y, w, h, bmp);
    }
    return;
  }

  // Get correct-sized bitmap based on block dimensions
  const unsigned char* bmp;
  if (w == 200 && h == 200) {
    bmp = mapWeatherIcon200(wCode[midIdx], wDay[midIdx]);
    draw1bppWhiteOnBlack(x, y, w, h, bmp);
  } else if (w == 160 && h == 160) {
    bmp = mapWeatherIcon160(wCode[midIdx], wDay[midIdx]);
    draw1bppWhiteOnBlack(x, y, w, h, bmp);
  } else {
    // Fallback to 200 if size doesn't match expected
    bmp = mapWeatherIcon200(wCode[midIdx], wDay[midIdx]);
    draw1bppWhiteOnBlack(x, y, w, h, bmp);
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
  const int iconSize = 48;        // icon size (48x48)

  // Offsets within the tile
  const int timeOffsetX = 8;
  const int timeOffsetY = 18;
  const int iconOffsetY = 38;     // vertical position of icon top inside the tile
  const int textOffsetX = 8;
  const int textOffsetY = iconOffsetY + iconSize + 12; // below icon

  const bool DEBUG_WEATHER_CARDS = false; // disable outlines

  // Draw 6 tiles with 4-hour stepping
  for (int i = 0; i < 6; i++) {
    // Compute dataIndex: 4-hour steps (0, 4, 8, 12, 16, 20)
    int dataIdx = startIdx + (i * 4);
    
    // Bounds check: skip if out of range
    if (dataIdx < 0 || dataIdx >= WEATHER_MAX) continue;

    // Horizontal placement: same Y, increment X per tile
    int tileX = startX + i * (tileW + gap);
    int tileY = startY;

    // Draw box around the tile
    display.drawRect(tileX, tileY, tileW, tileH, GxEPD_BLACK);

    // LABEL: (i+1)*4 → 04:00, 08:00, 12:00, 16:00, 20:00, 24:00
    int hoursAhead = (i + 1) * 4;
    display.setCursor(tileX + timeOffsetX + 15, tileY + timeOffsetY);
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:00", hoursAhead);
    display.print(buf);

    // ICON (48x48, centered in tile)
    const unsigned char* bmp = mapWeatherIcon48(wCode[dataIdx], wDay[dataIdx]);
    int iconX = tileX + (tileW - iconSize) / 2;
    int iconY = tileY + iconOffsetY;
    draw1bppWhiteOnBlack(iconX, iconY - 10, iconSize, iconSize, bmp);

    // TEMP + PRECIP
    display.setCursor(tileX + textOffsetX - 10, tileY + textOffsetY);
    display.print(wTemp[dataIdx]);
    display.print("F ");
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

    // page 0: left = moon (yesterday), page 2: right = moon (day after)
    bool leftMoon = (weatherPage == 0);
    bool rightMoon = (weatherPage == 2);

    drawTopIconBlock(ICON_L_X, ICON_SIDE_Y, ICON_SIDE_W, ICON_SIDE_H, leftBase, leftEmpty, leftMoon);
    drawTopIconBlock(ICON_M_X, ICON_MID_Y,  ICON_MID_W,  ICON_MID_H,  midBase,  midEmpty, false);
    drawTopIconBlock(ICON_R_X, ICON_SIDE_Y, ICON_SIDE_W, ICON_SIDE_H, rightBase, rightEmpty, rightMoon);

    // Day labels underneath icons
    display.setFont(FONT);
    display.setTextColor(GxEPD_BLACK);
    const int labelY = 225;
    
    if (weatherPage == 0) {
      display.setCursor(ICON_L_X + 30, labelY - 20);
      display.print("Yesterday");
      display.setCursor(ICON_M_X + 80, labelY + 20);
      display.print("Today");
      display.setCursor(ICON_R_X + 50, labelY - 10);
      display.print("Tomorrow");
    } else if (weatherPage == 1) {
      display.setCursor(ICON_L_X + 50, labelY - 10);
      display.print("Today");
      display.setCursor(ICON_M_X + 45, labelY + 30);
      display.print("Tomorrow");
      display.setCursor(ICON_R_X + 10, labelY - 20);
      display.print("Following Day");
    } else {
      display.setCursor(ICON_L_X + 40, labelY - 10);
      display.print("Tomorrow");
      display.setCursor(ICON_M_X + 15, labelY);
      display.print("Following Day");
      display.setCursor(ICON_R_X, labelY - 15);
      display.print("The Third Morrow");
    }

    drawSixHourRows(midBase);
  } while (display.nextPage());
}

static void applyNavState() {
  // Map navState -> screen + weatherPage
  if (navState == 0) {
    currentScreen = SCREEN_TIME;
    drawTimeScreen();
  } else if (navState == 1) {
    currentScreen = SCREEN_MTA;
    drawMTAScreen();
    if (mtaFetch()) updateMtaDotsPartial();
  } else {
    currentScreen = SCREEN_WEATHER;
    weatherPage = navState - 2; // 0,1,2
    weatherFetch();
    drawWeatherScreen();
    lastWeatherFlipMs = millis();
    updateWeatherPartial();
  }
}

static void goToScreen(Screen s) {
  currentScreen = s;

  if (currentScreen == SCREEN_TIME) {
    navState = 0;
    drawTimeScreen();
  } else if (currentScreen == SCREEN_MTA) {
    navState = 1;
    drawMTAScreen();
    if (mtaFetch()) updateMtaDotsPartial();
  } else { // SCREEN_WEATHER
    navState = 2; // reset to first weather page
    weatherFetch();
    drawWeatherScreen();
    weatherPage = 0;
    lastWeatherFlipMs = millis();
    updateWeatherPartial();
  }
}

// Button press detection: single press = next, double press = previous
static void detectButtonPress(bool &singlePress, bool &doublePress) {
  static unsigned long lastPressTime = 0;
  static unsigned long pressCount = 0;
  static unsigned long lastCheckTime = 0;
  unsigned long now = millis();
  
  singlePress = false;
  doublePress = false;
  
  // Check if we have a pending single press that timed out
  if (pressCount == 1 && (now - lastPressTime > DOUBLE_PRESS_WINDOW_MS)) {
    singlePress = true;
    pressCount = 0;
    Serial.println(">>> SINGLE PRESS detected");
  }
  
  // Check for second press to make it a double
  if (pressCount == 2) {
    doublePress = true;
    pressCount = 0;
    Serial.println(">>> DOUBLE PRESS detected");
  }
}

static void handleSerialEncoder() {
  static unsigned long lastBtnMs = 0;
  static unsigned long firstPressTime = 0;
  static int lastBtn = HIGH;
  static int pressCount = 0;
  static unsigned long lastDebugPrintMs = 0;
  unsigned long now = millis();

  // BUTTON edge detection with double-press logic
  int btn = digitalRead(ENC_SW);
  
  // Debug: Print all encoder pin states every 500ms
  if (now - lastDebugPrintMs >= 500) {
    lastDebugPrintMs = now;
    Serial.print("[DEBUG] ENC_SW(25)=");
    Serial.print(btn == LOW ? "LOW" : "HIGH");
    Serial.print(" | ENC_CLK(32)=");
    Serial.print(digitalRead(ENC_CLK) == LOW ? "LOW" : "HIGH");
    Serial.print(" | ENC_DT(33)=");
    Serial.println(digitalRead(ENC_DT) == LOW ? "LOW" : "HIGH");
  }
  if (btn == LOW && lastBtn == HIGH) {
    Serial.println("[BUTTON] *** EDGE DETECTED: HIGH -> LOW ***");
    if (now - lastBtnMs > 300) {  // Debounce - increased to 300ms to filter encoder noise
      Serial.print("[BUTTON] Debounce passed (");
      Serial.print(now - lastBtnMs);
      Serial.println("ms since last press)");
      lastBtnMs = now;
      
      // Check if this is within double-press window
      if (pressCount == 0) {
        // First press
        firstPressTime = now;
        pressCount = 1;
        Serial.println("[BUTTON] *** FIRST PRESS registered - waiting for second press ***");
      } else if (pressCount == 1 && (now - firstPressTime <= DOUBLE_PRESS_WINDOW_MS)) {
        // Second press within window = DOUBLE PRESS (go backwards)
        pressCount = 0;
        Serial.print("[BUTTON] *** DOUBLE PRESS DETECTED *** (time between presses: ");
        Serial.print(now - firstPressTime);
        Serial.println("ms)");
        navState = (navState + 4) % 5;  // Go backwards (5-1=4)
        Serial.print("[BUTTON] Going to PREVIOUS screen (state=");
        Serial.print(navState);
        Serial.println(")");
        applyNavState();
        manualMode = true;
        lastSwitchMs = now;
      } else if (pressCount == 1) {
        // Late second press - execute delayed single press for first press, then start new sequence
        Serial.print("[BUTTON] Second press too late (");
        Serial.print(now - firstPressTime);
        Serial.println("ms) - executing delayed single press for first press");
        
        // Execute the first press as single press
        navState = (navState + 1) % 5;
        Serial.print("[BUTTON] Going to NEXT screen (state=");
        Serial.print(navState);
        Serial.println(")");
        applyNavState();
        manualMode = true;
        lastSwitchMs = now;
        
        // Now start new sequence for this press
        firstPressTime = now;
        pressCount = 1;
        Serial.println("[BUTTON] *** Starting new FIRST PRESS sequence ***");
      }
    }
  }
  lastBtn = btn;
  
  // Check if single press window expired
  if (pressCount == 1 && (now - firstPressTime > DOUBLE_PRESS_WINDOW_MS)) {
    // SINGLE PRESS (go forward)
    pressCount = 0;
    Serial.print("[BUTTON] *** SINGLE PRESS CONFIRMED *** (timeout: ");
    Serial.print(now - firstPressTime);
    Serial.println("ms)");
    navState = (navState + 1) % 5;  // Go forward
    Serial.print("[BUTTON] Going to NEXT screen (state=");
    Serial.print(navState);
    Serial.println(")");
    applyNavState();
    manualMode = true;
    lastSwitchMs = now;
  }
}

// -------------------- WiFi Provisioning Functions -------------------- //

// HTML page for WiFi setup
static const char SETUP_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <meta charset="utf-8" />
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500&display=swap" rel="stylesheet">
  <title>INK HAT WiFi Setup</title>
  <style>
    body { 
      margin: 0;
      min-height: 100vh;
      background: #0b0f14;
      overflow: hidden;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
    }
    
    #binary {
      transform: translate(-24px, -24px);
      width: calc(100vw + 48px);
      height: calc(100vh + 48px);
      position: fixed;
      inset: 0;
      z-index: 0;
      display: block;
      opacity: 0.15;
      pointer-events: none;
      white-space: pre;
      line-height: 1.05;
      font-size: 12px;
      color: #cfe3ff;
    }
    
    #content {
      position: relative;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      color: #e7eef7;
      z-index: 1;
    }
    
    .box { 
      max-width: 420px;
      padding: 40px;
      background: rgba(21, 25, 33, 0.95);
      border-radius: 8px;
      box-shadow: 0 0 20px rgba(255, 255, 255, 0.08);
      border: 1px solid rgba(207, 227, 255, 0.1);
    }
    
    h2 {
      text-align: center;
      color: #cfe3ff;
      margin-top: 0;
      margin-bottom: 30px;
      font-size: 28px;
      letter-spacing: 0.05em;
      text-shadow: 0 0 10px rgba(255, 255, 255, 0.1);
    }
    
    input { 
      width: 100%; 
      padding: 12px; 
      margin: 12px 0; 
      font-size: 16px;
      background: #1a1f26;
      color: #e7eef7;
      border: 1px solid #3a4552;
      border-radius: 4px;
      box-sizing: border-box;
      font-family: "Inter", sans-serif;
    }
    
    input:focus {
      outline: none;
      border-color: #cfe3ff;
      box-shadow: 0 0 10px rgba(207, 227, 255, 0.2);
    }
    
    label {
      display: block;
      margin-top: 16px;
      margin-bottom: 6px;
      color: #9ab4d1;
      font-size: 14px;
      letter-spacing: 0.05em;
    }
    
    button { 
      width: 100%; 
      padding: 14px; 
      font-size: 16px;
      background: #2d5a8a;
      color: #e7eef7;
      border: none;
      border-radius: 4px;
      cursor: pointer;
      margin-top: 24px;
      letter-spacing: 0.08em;
      font-family: "Inter", sans-serif;
      transition: all 0.2s ease;
    }
    
    button:hover {
      background: #3a6fa5;
      box-shadow: 0 0 15px rgba(58, 111, 165, 0.3);
    }
    
    .info {
      text-align: center;
      font-size: 14px;
      color: #7a8a9a;
      margin-top: 24px;
      line-height: 1.6;
    }
  </style>
</head>
<body>
  <pre id="binary"></pre>
  
  <div id="content">
    <div class="box">
      <h2>INK HAT SETUP</h2>
      <form action="/save" method="post">
        <label>WiFi SSID</label>
        <input name="ssid" placeholder="Network name" required />
        <label>Password</label>
        <input name="pass" type="password" placeholder="WiFi password" />
        <button type="submit"> SAVE </button>
      </form>
      
      <div class="info">
        Connect to this network and visit<br>
        <strong>192.168.4.1</strong>
      </div>
    </div>
  </div>

  <script>
    const binary = document.getElementById("binary");
    let cols = 0, rows = 0;
    let grid = [];
    let timerId = null;

    function measureChar() {
      const probe = document.createElement("span");
      const cs = getComputedStyle(binary);
      probe.textContent = "0";
      probe.style.position = "fixed";
      probe.style.left = "-9999px";
      probe.style.visibility = "hidden";
      probe.style.fontFamily = cs.fontFamily;
      probe.style.fontSize = cs.fontSize;
      probe.style.lineHeight = cs.lineHeight;
      probe.style.whiteSpace = "pre";
      document.body.appendChild(probe);
      const rect = probe.getBoundingClientRect();
      probe.remove();
      return { w: rect.width || 1, h: rect.height || 1 };
    }

    function randBit() {
      return Math.random() < 0.5 ? "0" : "1";
    }

    function randInt(min, max) {
      return Math.floor(Math.random() * (max - min + 1)) + min;
    }

    function buildGrid() {
      const { w, h } = measureChar();
      cols = Math.ceil(window.innerWidth / w) + 2;
      rows = Math.ceil(window.innerHeight / h) + 2;
      grid = new Array(rows);
      for (let r = 0; r < rows; r++) {
        let line = "";
        for (let c = 0; c < cols; c++) line += randBit();
        grid[r] = line;
      }
      render();
    }

    function render() {
      let out = "";
      for (let r = 0; r < rows; r++) {
        out += grid[r] + "\n";
      }
      binary.textContent = out;
    }

    function shiftColumnsDown() {
      if (!rows || !cols) return;
      const mat = new Array(rows);
      for (let r = 0; r < rows; r++) mat[r] = grid[r].split("");
      for (let c = 0; c < cols; c++) {
        const bottom = mat[rows - 1][c];
        for (let r = rows - 1; r > 0; r--) {
          mat[r][c] = mat[r - 1][c];
        }
        mat[0][c] = Math.random() < 0.65 ? randBit() : bottom;
      }
      for (let r = 0; r < rows; r++) grid[r] = mat[r].join("");
      render();
    }

    function startRain() {
      if (timerId) clearInterval(timerId);
      timerId = setInterval(() => {
        shiftColumnsDown();
      }, 100);
    }

    window.addEventListener("resize", () => {
      buildGrid();
      startRain();
    });

    buildGrid();
    startRain();
  </script>
</body>
</html>
)HTML";

// Save WiFi credentials to NVS
void saveCreds(const String& ssid, const String& pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
  Serial.println("[WIFI] Credentials saved to NVS");
}

// Load WiFi credentials from NVS
bool loadCreds(String& ssidOut, String& passOut) {
  prefs.begin("wifi", true);
  ssidOut = prefs.getString("ssid", "");
  passOut = prefs.getString("pass", "");
  prefs.end();
  return ssidOut.length() > 0;
}

// Clear saved WiFi credentials
void clearCreds() {
  prefs.begin("wifi", false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.end();
  Serial.println("[WIFI] Credentials cleared from NVS");
}

// Web server handler: root page
void handleRoot() {
  server.send(200, "text/html", SETUP_PAGE);
}

// Web server handler: save credentials
void handleSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  ssid.trim();

  if (ssid.length() == 0) {
    server.send(400, "text/plain", "SSID is required.");
    return;
  }

  // TESTING: Disabled credential saving for now
  // saveCreds(ssid, pass);

  server.send(200, "text/plain", "WiFi credentials received (NOT saved - testing mode)!");
  delay(2000);
  ESP.restart();
}

// Web server handler: clear credentials
void handleClear() {
  clearCreds();
  server.send(200, "text/plain", "WiFi credentials cleared! Rebooting in 2 seconds...");
  delay(2000);
  ESP.restart();
}

// Start Access Point mode with web server
void startAPMode() {
  apModeActive = true;
  WiFi.mode(WIFI_AP);

  if (AP_PASS && strlen(AP_PASS) >= 8) {
    WiFi.softAP(AP_SSID, AP_PASS);
  } else {
    WiFi.softAP(AP_SSID);
  }

  Serial.println("[AP] Access Point started");
  Serial.print("[AP] SSID: ");
  Serial.println(AP_SSID);
  Serial.print("[AP] IP: ");
  Serial.println(WiFi.softAPIP());

  // Display AP info on e-ink
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(FONT_BIG);

    display.setCursor(250, 35);
    display.print("WiFi Setup");
    display.drawLine(10, 50, 790, 50, GxEPD_BLACK);      // Top
    display.drawLine(10, 450, 790, 450, GxEPD_BLACK);    // Bottom
    display.drawLine(10, 50, 10, 450, GxEPD_BLACK);      // Left
    display.drawLine(790, 50, 790, 450, GxEPD_BLACK);    // Right


    display.setFont(FONT_BIG);
    display.setCursor(250, 100);
    display.print("Connect to:");
    display.setCursor(310, 140);
    display.setFont(FONT_MED);
    display.print(AP_SSID);

    display.setFont(FONT_BIG);
    display.setCursor(270, 220);
    display.print("Password:");
    display.setCursor(330, 260);
    display.setFont(FONT_MED);
    display.print(AP_PASS);

    display.setFont(FONT_BIG);
    display.setCursor(190, 340);
    display.print("Visit in browser:");
    display.setCursor(330, 380);
    display.setFont(FONT_MED);
    display.print("192.168.4.1");

  } while (display.nextPage());

  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/clear", HTTP_POST, handleClear);
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("[AP] Web server started at http://192.168.4.1");
}
