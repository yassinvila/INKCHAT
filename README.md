# E-INK Transit Display

An ESP32-based e-ink display that shows real-time NYC MTA train arrivals and weather information.

## Overview

This project combines an ESP32 microcontroller with an e-ink display to create a low-power transit information screen. It displays:
- **MTA Train Times**: Real-time northbound and southbound train arrivals
- **Weather Data**: Temperature, precipitation, and weather conditions with icons
- **Auto-refresh**: Updates periodically while maintaining the e-ink's paper-like appearance

## Hardware Requirements

- **ESP32 Development Board** (esp32dev)
- **GxEPD2-compatible E-ink Display** (Black & White)
- USB cable for programming (COM3)
- WiFi connection
  
## 📁 Project Structure

```
.
├── E-INK/                      # ESP32 firmware
│   ├── src/
│   │   ├── main.cpp           # Main display logic (757 lines)
│   │   ├── api.cpp            # HTTP API client functions
│   │   └── icon.cpp           # Weather icon mapping
│   ├── include/
│   │   ├── api.h              # API declarations
│   │   └── icon.h             # Weather icon definitions
│   ├── platformio.ini         # PlatformIO configuration
│   ├── .env                   # WiFi credentials (not in git)
│   └── .env.example           # Template for credentials
│
├── proxy/
│   ├── server.js              # Node.js API proxy (161 lines)
│   └── node_modules/          # Dependencies (not in git)
│
└── .gitignore                 # Git ignore rules
```

##  Setup Instructions

### 1. Clone the Repository

```bash
git clone https://github.com/yassinvila/INKCHAT.git
cd INKCHAT
```

### 2. Configure WiFi Credentials

Create `E-INK/.env` based on the example:

```bash
cd E-INK
cp .env.example .env
```

Edit `.env` and add your WiFi credentials:

```env
WIFI_SSID=YourNetworkName
WIFI_PASSWORD=YourNetworkPassword
```

### 3. Install Proxy Server Dependencies

The proxy server handles API requests to avoid CORS issues and manage API keys:

```bash
cd ../proxy
npm install
```

### 4. Start the Proxy Server

```bash
node server.js
```

The server runs on `http://localhost:8787` with two endpoints:
- `GET /mta` - Fetches MTA GTFS realtime data
- `GET /weather` - Fetches Open-Meteo weather forecast

### 5. Flash the ESP32

Using PlatformIO (VS Code):

1. Open the `E-INK` folder in VS Code
2. Install the PlatformIO extension if needed
3. Connect your ESP32 via USB (COM3)
4. Click "Upload" or run:

```bash
cd E-INK
pio run --target upload
```

## 📡 API Configuration

### MTA API
- **Endpoint**: GTFS Realtime Feed
- **Proxy URL**: `http://localhost:8787/mta`
- **Data**: Train arrival predictions for specific stops

### Weather API
- **Provider**: Open-Meteo
- **Proxy URL**: `http://localhost:8787/weather`
- **Data**: Hourly temperature, precipitation, weather codes for 72 hours

##  Display Features

### Weather Icons
The display shows contextual weather icons:
- ☀️ Clear day/night icons
- 🌧️ Rain icons
- 🌨️ Snow icons
- ⛈️ Thunderstorm icons
- 🌥️ Partly cloudy variations
- 🌫️ Fog icon

Icons automatically adjust based on weather codes and time of day.

### Train Data Arrays
- **North/South trains**: 5 train slots per direction
- **Arrival times**: Minutes until arrival
- **Auto-update**: Refreshes on schedule

### Memory Management
- **Static JSON Documents**: Uses `StaticJsonDocument<30000>` allocated once at startup and reused
- **No Heap Fragmentation**: Avoids repeated malloc/free cycles that can fragment RAM over long-term operation
- **Weather Icons in Flash**: Bitmap data stored in Flash memory (`PROGMEM`) to preserve RAM
- **Global Arrays**: Train and weather data use static arrays defined at compile time

##  Pin Configuration

The e-ink display connects to the ESP32 via SPI. Pin mappings are defined in `platformio.ini` and configured through the GxEPD2 library.

##  Dependencies

### ESP32 (PlatformIO)
- `GxEPD2` - E-ink display driver
- `Adafruit GFX Library` - Graphics primitives
- `ArduinoJson` - JSON parsing for API responses
- `WiFi.h` - ESP32 WiFi connectivity
- `HTTPClient.h` - HTTP requests

### Proxy Server (Node.js)
- `express` - Web framework
- `node-fetch` - HTTP client
- `cors` - CORS middleware

##  Security Notes

- **Never commit `.env` file** - WiFi credentials are kept local
- The `.gitignore` excludes:
  - `.env` files
  - `node_modules/`
  - `.pio/` build files
  - `.vscode/` settings

##  Troubleshooting

### ESP32 Won't Connect to WiFi
- Check `.env` credentials are correct
- Ensure WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
- Verify the proxy server is running

### Display Not Updating
- Check serial output for error messages
- Verify API endpoints are accessible from ESP32
- Ensure the proxy server is running on the correct port

### Build Errors
- Clean build: `pio run --target clean`
- Update dependencies: `pio pkg update`

## Development

### Code Style
- Main display logic: `E-INK/src/main.cpp`
- API functions: `E-INK/src/api.cpp`
- Weather icons: `E-INK/src/icon.cpp`

### Adding New Features
1. Modify source files in `E-INK/src/`
2. Update headers in `E-INK/include/`
3. Rebuild and upload firmware

### Proxy Customization
Edit `proxy/server.js` to:
- Add new API endpoints
- Modify data transformation
- Update CORS settings

##  License

This project is open source. See individual library licenses for dependencies.

## 📧 Contact

For questions or support, open an issue on GitHub.
