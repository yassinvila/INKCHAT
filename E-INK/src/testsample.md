/*
  ESP32 Wi-Fi Provisioning via Phone (SoftAP + simple web form)
  - If saved Wi-Fi creds exist and connect succeeds: runs in STA mode.
  - Otherwise: starts AP "ESP32-SETUP" and hosts a setup page at http://192.168.4.1
  - Enter SSID/PASS, it saves to NVS (Preferences) and reboots.

  Dependencies (Arduino IDE / PlatformIO):
    - WiFi.h (built-in)
    - WebServer.h (built-in for ESP32)
    - Preferences.h (built-in)

  Notes:
    - AP password must be >= 8 chars if used.
    - This is intentionally minimal; no captive portal DNS interception.
      Most phones won’t auto-pop the page without DNS tricks, so you usually open 192.168.4.1 manually.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

static const char* AP_SSID = "ESP32-SETUP";
static const char* AP_PASS = "12345678"; // set to nullptr or "" to make open AP (not recommended)

static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

WebServer server(80);
Preferences prefs;

// -------------------- HTML UI --------------------
static const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32 WiFi Setup</title>
  <style>
    body { font-family: Arial, sans-serif; padding: 16px; }
    input { width: 100%; padding: 10px; margin: 8px 0; font-size: 16px; }
    button { width: 100%; padding: 12px; font-size: 16px; }
    .box { max-width: 420px; margin: 0 auto; }
  </style>
</head>
<body>
  <div class="box">
    <h2>WiFi Setup</h2>
    <form action="/save" method="post">
      <label>SSID</label>
      <input name="ssid" placeholder="Your WiFi name" required />
      <label>Password</label>
      <input name="pass" type="password" placeholder="Your WiFi password" />
      <button type="submit">Save & Reboot</button>
    </form>

    <hr/>
    <form action="/clear" method="post">
      <button type="submit">Clear Saved WiFi</button>
    </form>
  </div>
</body>
</html>
)HTML";

// -------------------- Helpers --------------------
static void saveCreds(const String& ssid, const String& pass)
{
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

static bool loadCreds(String& ssidOut, String& passOut)
{
  prefs.begin("wifi", true);
  ssidOut = prefs.getString("ssid", "");
  passOut = prefs.getString("pass", "");
  prefs.end();
  return ssidOut.length() > 0;
}

static void clearCreds()
{
  prefs.begin("wifi", false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.end();
}

static bool connectWiFiSTA(const char* ssid, const char* pass, uint32_t timeoutMs)
{
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  // Flush any stale state
  WiFi.disconnect(true, true);
  delay(200);

  WiFi.begin(ssid, pass);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs)
  {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.print("WiFi connect failed. status=");
  Serial.println((int)WiFi.status());
  return false;
}

// -------------------- AP Mode + Web Server --------------------
static void handleRoot()
{
  server.send(200, "text/html", PAGE);
}

static void handleSave()
{
  // WebServer parses POST form fields into server.arg()
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  ssid.trim();
  // pass can be empty (open networks), though uncommon

  if (ssid.length() == 0)
  {
    server.send(400, "text/plain", "SSID is required.");
    return;
  }

  saveCreds(ssid, pass);

  server.send(200, "text/plain", "Saved WiFi. Rebooting...");
  delay(800);
  ESP.restart();
}

static void handleClear()
{
  clearCreds();
  server.send(200, "text/plain", "Cleared saved WiFi. Rebooting...");
  delay(800);
  ESP.restart();
}

static void startAPMode()
{
  WiFi.mode(WIFI_AP);

  // Start AP (open if AP_PASS is empty)
  if (AP_PASS && strlen(AP_PASS) >= 8)
  {
    WiFi.softAP(AP_SSID, AP_PASS);
  }
  else
  {
    WiFi.softAP(AP_SSID);
  }

  Serial.println("Started AP mode.");
  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/clear", HTTP_POST, handleClear);
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("Web server started. Open http://192.168.4.1");
}

// -------------------- Arduino --------------------
void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("Booting...");

  String ssid, pass;
  if (loadCreds(ssid, pass))
  {
    Serial.print("Found saved SSID: ");
    Serial.println(ssid);

    if (connectWiFiSTA(ssid.c_str(), pass.c_str(), WIFI_CONNECT_TIMEOUT_MS))
    {
      // Connected: your normal app can start here
      Serial.println("STA mode ready. Continue with your app...");
      return;
    }

    Serial.println("STA connect failed. Falling back to AP setup...");
  }
  else
  {
    Serial.println("No saved WiFi. Starting AP setup...");
  }

  startAPMode();
}

void loop()
{
  // Only needed in AP mode; harmless in STA mode if server not started.
  server.handleClient();
}
