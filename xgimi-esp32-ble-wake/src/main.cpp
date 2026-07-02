#include <Arduino.h>
#include <ESPmDNS.h>
#include <NimBLEDevice.h>
#include <WebServer.h>
#include <WiFi.h>

#if __has_include("config.h")
#include "config.h"
#else
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define DEVICE_HOSTNAME "xgimi-ble-wake"
#define XGIMI_PAYLOAD_HEX "5386b44412c970ffffff3043524b544d"
#endif

namespace {

constexpr uint16_t XGIMI_COMPANY_ID = 0x0046;
constexpr uint8_t ADVERTISEMENT_REPEATS = 4;
constexpr uint16_t ADVERTISEMENT_ON_MS = 700;
constexpr uint16_t ADVERTISEMENT_OFF_MS = 200;

WebServer server(80);

uint32_t powerOnCount = 0;
String lastPowerOn = "never";

uint8_t hexNibble(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return 0;
}

size_t hexToBytes(const char* hex, uint8_t* output, size_t maxLength) {
  const size_t hexLength = strlen(hex);
  const size_t byteCount = min(hexLength / 2, maxLength);

  for (size_t index = 0; index < byteCount; index++) {
    output[index] = (hexNibble(hex[index * 2]) << 4) |
                    hexNibble(hex[index * 2 + 1]);
  }

  return byteCount;
}

String uptimeText() {
  const uint32_t seconds = millis() / 1000;
  const uint32_t hours = seconds / 3600;
  const uint32_t minutes = (seconds % 3600) / 60;
  const uint32_t rest = seconds % 60;

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%luh %lum %lus", hours, minutes, rest);
  return String(buffer);
}

String htmlPage() {
  String page;
  page.reserve(3500);
  page += F("<!doctype html><html lang=\"de\"><head>");
  page += F("<meta charset=\"utf-8\">");
  page += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  page += F("<title>XGIMI Wake</title>");
  page += F("<style>");
  page += F(":root{color-scheme:dark light;font-family:system-ui,-apple-system,Segoe UI,sans-serif}");
  page += F("body{margin:0;min-height:100vh;display:grid;place-items:center;background:#101418;color:#f8fafc}");
  page += F("main{width:min(420px,calc(100vw - 32px));text-align:center}");
  page += F("h1{font-size:34px;margin:0 0 20px}");
  page += F("button{width:100%;height:72px;border:0;border-radius:8px;background:#22c55e;color:#07110b;font-size:22px;font-weight:750;cursor:pointer}");
  page += F("button:disabled{opacity:.6;cursor:wait}");
  page += F(".status{margin-top:18px;color:#aab4bf;font-size:15px;line-height:1.5}");
  page += F("code{color:#dbeafe}");
  page += F("</style></head><body><main>");
  page += F("<h1>XGIMI Wake</h1>");
  page += F("<button id=\"power\">Einschalten</button>");
  page += F("<div class=\"status\">");
  page += F("<div>Adresse: <code>");
  page += WiFi.localIP().toString();
  page += F("</code></div>");
  page += F("<div>Endpoint: <code>/poweron</code></div>");
  page += F("<div>Gesendet: <span id=\"count\">");
  page += String(powerOnCount);
  page += F("</span>x</div>");
  page += F("<div id=\"message\">Bereit</div>");
  page += F("</div>");
  page += F("<script>");
  page += F("const b=document.getElementById('power'),m=document.getElementById('message'),c=document.getElementById('count');");
  page += F("b.onclick=async()=>{b.disabled=true;m.textContent='Sende BLE Power-On...';");
  page += F("try{const r=await fetch('/poweron',{method:'POST'});const j=await r.json();c.textContent=j.power_on_count;m.textContent='Gesendet';}");
  page += F("catch(e){m.textContent='Fehler beim Senden';}");
  page += F("finally{setTimeout(()=>{b.disabled=false},600)}};");
  page += F("</script></main></body></html>");
  return page;
}

void sendXgimiPowerOnAdvertisement() {
  uint8_t capturedPayload[64] = {0};
  const size_t capturedLength =
      hexToBytes(XGIMI_PAYLOAD_HEX, capturedPayload, sizeof(capturedPayload));

  uint8_t manufacturerData[80] = {0};
  manufacturerData[0] = static_cast<uint8_t>(XGIMI_COMPANY_ID & 0xff);
  manufacturerData[1] = static_cast<uint8_t>((XGIMI_COMPANY_ID >> 8) & 0xff);
  memcpy(manufacturerData + 2, capturedPayload, capturedLength);

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->stop();

  NimBLEAdvertisementData advertisementData;
  advertisementData.setFlags(0x06);
  advertisementData.setName("XGIMI Wake");
  advertisementData.setManufacturerData(
      std::string(reinterpret_cast<char*>(manufacturerData), capturedLength + 2));

  advertising->setAdvertisementData(advertisementData);

  for (uint8_t attempt = 0; attempt < ADVERTISEMENT_REPEATS; attempt++) {
    advertising->start();
    delay(ADVERTISEMENT_ON_MS);
    advertising->stop();
    delay(ADVERTISEMENT_OFF_MS);
  }

  powerOnCount++;
  lastPowerOn = String(millis() / 1000) + "s after boot";
}

void sendJsonStatus() {
  String json;
  json.reserve(320);
  json += F("{\"status\":\"ok\",");
  json += F("\"ip\":\"");
  json += WiFi.localIP().toString();
  json += F("\",\"hostname\":\"");
  json += DEVICE_HOSTNAME;
  json += F("\",\"rssi\":");
  json += String(WiFi.RSSI());
  json += F(",\"uptime\":\"");
  json += uptimeText();
  json += F("\",\"power_on_count\":");
  json += String(powerOnCount);
  json += F(",\"last_power_on\":\"");
  json += lastPowerOn;
  json += F("\"}");
  server.send(200, "application/json", json);
}

void handlePowerOn() {
  sendXgimiPowerOnAdvertisement();
  sendJsonStatus();
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupRoutes() {
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", htmlPage()); });
  server.on("/status", HTTP_GET, sendJsonStatus);
  server.on("/poweron", HTTP_GET, handlePowerOn);
  server.on("/poweron", HTTP_POST, handlePowerOn);
  server.onNotFound([]() {
    server.send(404, "application/json", "{\"status\":\"not_found\"}");
  });
  server.begin();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);

  connectWifi();

  if (MDNS.begin(DEVICE_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.print("mDNS: http://");
    Serial.print(DEVICE_HOSTNAME);
    Serial.println(".local/");
  }

  NimBLEDevice::init("XGIMI Wake");
  setupRoutes();

  Serial.println("XGIMI BLE Wake ready");
}

void loop() {
  server.handleClient();
}
