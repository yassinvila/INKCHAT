# E-INK Transit Display: AI Coding Instructions

## Project Overview

**E-INK Transit Display** is a real-time NYC transit information display using an ESP32 microcontroller with a 7.5" e-ink display. The system fetches MTA train arrivals and weather forecasts, displaying them in a low-power, paper-like interface that rotates between three screens every 60 seconds.

### Architecture: Two-Tier System

1. **ESP32 Firmware (C++/Arduino)** - [E-INK/](E-INK/): Display driver, WiFi client, screen rendering
2. **Node.js Proxy Server (JavaScript)** - [proxy/](proxy/): API gateway that handles CORS, rate-limiting, and protocol translation (GTFS Realtime → JSON)

**Critical Data Flow**: ESP32 → WiFi → Proxy Server (localhost:8787) → MTA GTFS Realtime + Open-Meteo Weather API

## Key Components & Patterns

### Display Architecture (GxEPD2)
- **Resolution**: 800×480 pixels (7.5" Good Display UC8179)
- **Three Rotating Screens** (screen enumeration in [main.cpp](E-INK/src/main.cpp#L63)):
  1. **TIME**: Digital clock (updates every minute, partial refresh)
  2. **MTA**: Train arrivals (5 dots per direction, visual progress indicator)
  3. **WEATHER**: 72-hour hourly forecast with weather icons

### Data Storage Pattern
Global extern arrays declared in [api.h](E-INK/include/api.h), **defined in [main.cpp](E-INK/src/main.cpp#L43)** (not in api.cpp):
- `northTrain[5]`, `northMin[5]` - North-bound trains (character + minutes)
- `southTrain[5]`, `southMin[5]` - South-bound trains
- `wTemp[72]`, `wPrec[72]`, `wCode[72]`, `wDay[72]` - Weather hourly data

**Rationale**: Keeps large buffers in RAM from fragmenting, centralizes state visibility.

### API Integration Pattern
Two fetch functions populate global state:
- `mtaFetch()` ([api.cpp](E-INK/src/api.cpp#L7)): Calls proxy `/mta` endpoint, deserializes JSON into train arrays
- `weatherFetch()` ([api.cpp](E-INK/src/api.cpp#L60)): Calls proxy `/weather` endpoint, parses Open-Meteo hourly array

**Important**: Both handle WiFi disconnection gracefully; ESP32 prints debug to Serial if failed.

### Weather Icon Mapping
[icon.h](E-INK/include/icon.h) exposes pre-baked bitmap arrays (3 sizes: 200×200, 160×160, 48×48) for day/night variants. The `mapWeatherIcon(weather_code, is_day)` function ([icon.cpp](E-INK/src/icon.cpp)) translates Open-Meteo codes to bitmap pointers. **Icons are stored in PROGMEM** (Flash memory) to reduce RAM.

### MTA Screen Layout
Hard-coded visual coordinates:
- Left side (X=20): 160×160 weather icon
- Right side (X=220): Route indicator line, 5 filled dots for train arrivals
- Dot X positions: `[330, 430, 510, 620, 730]` (manual spacing, not computed)
- Two halves (top/bottom) at Y=0 and Y=240 for north/south

**Design note**: Fixed layout avoids floating-point math on ESP32; trades flexibility for performance.

## Build & Deploy Workflow

### Prerequisites
- PlatformIO CLI installed (or VS Code extension)
- ESP32 connected to COM3 (see [platformio.ini](E-INK/platformio.ini))
- `.env` file in [E-INK/](E-INK/) with `WIFI_SSID` and `WIFI_PASSWORD`

### Build & Flash
```bash
cd E-INK
pio run --target upload --monitor-port COM3
```

Alternatively: Open folder in VS Code with PlatformIO extension, click "Upload" button.

### Start Proxy Server (Required)
```bash
cd proxy
npm install  # if first time
node server.js
```
Server listens on `http://0.0.0.0:8787` (accessible from local network).

### Configuration
- **MTA Proxy**: Decodes GTFS Realtime protobuf using `gtfs-realtime-bindings` npm package
- **Weather Provider**: Open-Meteo (free, no key required)
- **Target Coordinates**: Hardcoded in [proxy/server.js](proxy/server.js#L12): 40.7506, -73.9935 (Midtown NYC)

## Project-Specific Conventions

### Display Rendering
- **Partial updates**: Only refresh changed regions (e.g., `updateMtaDotsPartial()`) to reduce flicker and power consumption
- **Full refresh**: Called at screen transitions; triggered by `drawTimeScreen()`, `drawMTAScreen()`, `drawWeatherScreen()`
- **No double-buffering**: E-ink driver handles framebuffer internally

### Serial Communication
- **Debug output**: 115200 baud
- **Manual screen control**: Reserved for future encoder-based UI (pins 27, 32, 33 reserved)
- **No REPL**: No interactive serial shell; prints are one-way logging only

### Clock & Timezone
- Syncs from NTP on WiFi connect using POSIX `setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1)`
- Time zone is hardcoded as Eastern Time in [main.cpp](E-INK/src/main.cpp#L34)

### JSON Parsing
- Uses **ArduinoJson v7** with `StaticJsonDocument<4096>` (MTA response)
- Fallback values with `|` operator: `north[i]["minutes"] | -1` defaults to -1 if key missing
- **No dynamic allocation** to avoid heap fragmentation on long-running device

## Common Tasks & Edge Cases

### Adding a New Data Source
1. Add fetch function in [api.cpp](E-INK/src/api.cpp) matching `bool newSourceFetch()`
2. Declare global storage arrays in [api.h](E-INK/include/api.h) as `extern`
3. Define storage in [main.cpp](E-INK/src/main.cpp) (not api.cpp)
4. Call from `loop()` on appropriate screen transition
5. Add proxy endpoint in [proxy/server.js](proxy/server.js) if external API required

### Troubleshooting Serial Issues
- If upload fails: Try `pio run --target erase` then re-upload
- Monitor port mismatch: Update `upload_port` in [platformio.ini](E-INK/platformio.ini)
- Garbage output at 115200 baud: Check USB cable and board selection (`esp32dev`)

### WiFi Provisioning
The [testsample.md](E-INK/src/testsample.md) file documents an experimental WiFi setup UI (SoftAP mode) as a reference; it's **not integrated** into the main build. To enable: merge its provisioning logic into `setup()` before `displayInit()`.

### Performance Considerations
- ESP32 has ~320KB free RAM after WiFi stack; weather array (72×4 ints + 72 floats) consumes ~600 bytes
- E-ink refresh takes ~3 seconds (GxEPD2 internal operation); avoid calling during critical sections
- Proxy server is single-threaded; acceptable for one device but won't scale

## File Reference

| File | Purpose |
|------|---------|
| [E-INK/src/main.cpp](E-INK/src/main.cpp) | Display loop, screen logic, global state, rendering |
| [E-INK/src/api.cpp](E-INK/src/api.cpp) | HTTP fetch functions, JSON parsing |
| [E-INK/src/icon.cpp](E-INK/src/icon.cpp) | Weather code → bitmap mapping |
| [E-INK/include/api.h](E-INK/include/api.h) | API function declarations, extern storage |
| [E-INK/include/icon.h](E-INK/include/icon.h) | Weather bitmap declarations |
| [proxy/server.js](proxy/server.js) | Express routes, GTFS + Weather aggregation |
| [platformio.ini](E-INK/platformio.ini) | Build config, board, dependencies, environment vars |
