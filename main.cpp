#include <Arduino.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600);

#define TOUCH_PIN 4 ///< GPIO0 bruges som touch pin
#define CSV_FILE "/data.csv"

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void setupSPIFFS();
void setupWiFi();
void setupWebServer();
void logTouchData();
String readCSVData();

unsigned long previousMillis = 0; ///< Tidsinterval til dataopdatering
const long interval = 2000; ///< Intervallet for opdateringer i millisekunder
int touchCount = 0; ///< Antal touch registreringer

/**
 * @brief Initialiserer enheden og opsætter SPIFFS, WiFi, og WebServer.
 */
void setup() {
  Serial.begin(9600);
  setupSPIFFS();
  setupWiFi();
  setupWebServer();
  timeClient.begin();
  timeClient.update();
  pinMode(TOUCH_PIN, INPUT);
}

/**
 * @brief Hovedloop, der overvåger touch-indgange og logger data, hvis berøringsmålingen overstiger tærsklen.
 */
void loop() {
  timeClient.update();
  if (touchRead(TOUCH_PIN) < 20) {
    touchCount++;
    logTouchData();
    delay(200);
  }
}

/**
 * @brief Initialiserer SPIFFS-filsystemet.
 */
void setupSPIFFS() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS initialisering fejlede");
    return;
  }
  Serial.println("LittleFS initialiseret");
}

/**
 * @brief Opsætter WiFi ved hjælp af WiFiManager.
 */
void setupWiFi() {
  WiFiManager wifiManager;
  if (!wifiManager.autoConnect("ESP32-Config-AP")) {
    Serial.println("Forbindelse til Wi-Fi mislykkedes!");
    ESP.restart();
  }
  Serial.println("Forbundet til Wi-Fi!");
  Serial.print("IP adresse: ");
  Serial.println(WiFi.localIP());
}

/**
 * @brief Nulstiller graf og touch count ved at nulstille CSV-filindholdet.
 */
void resetGraphData() {
  touchCount = 0;
  File file = LittleFS.open(CSV_FILE, "w");
  if (file) file.close();
}

/**
 * @brief Opsætter webserveren og tilføjer ruter til hentning, sletning og nulstilling af data.
 */
void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/get-data", HTTP_GET, [](AsyncWebServerRequest *request) {
    String csvData = readCSVData();
    request->send(200, "text/plain", csvData);
  });

  server.on("/clear-csv", HTTP_POST, [](AsyncWebServerRequest *request) {
    LittleFS.open(CSV_FILE, "w");
    request->send(200, "text/plain", "CSV file cleared");
  });

  server.on("/download-csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, CSV_FILE, "text/csv", true);
  });

  server.on("/reset-graph", HTTP_POST, [](AsyncWebServerRequest *request) {
    resetGraphData();
    request->send(200, "text/plain", "Graf og touch count nulstillet!");
  });

  server.on("/clear-wifi-config", HTTP_POST, [](AsyncWebServerRequest *request) {
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    ESP.restart();
    request->send(200, "text/plain", "WiFi-konfigurationen er nulstillet. Genstart enheden for at tilslutte igen.");
    Serial.println("WiFi-konfiguration nulstillet.");
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.begin();
}

/**
 * @brief Håndterer WebSocket-begivenheder for at sende touch count data til klienten.
 * @param server WebSocket server pointer
 * @param client WebSocket klient pointer
 * @param type Begivenhedstype
 * @param arg Ekstra argumenter
 * @param data Data fra begivenheden
 * @param len Data længde
 */
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.println("WebSocket client connected");
    client->text("touchData:" + String(touchCount));
  }
}

/**
 * @brief Logger touch data til en CSV-fil med tidsstempel og tæller.
 */
void logTouchData() {
  time_t now = timeClient.getEpochTime();
  setTime(now);

  File file = LittleFS.open(CSV_FILE, "a");
  if (file) {
    String date = String(day()) + "/" + String(month()) + "/" + String(year());
    String time = String(hour()) + ":" + String(minute()) + ":" + String(second());
    String logEntry = String(touchCount) + "," + date + "," + time + "\n";
    file.print(logEntry);
    file.close();
    Serial.println("Touch logged: " + logEntry);
  } else {
    Serial.println("Could not open CSV file for logging");
  }
}

/**
 * @brief Læser indholdet af CSV-filen og returnerer det som en streng.
 * @return En streng med CSV-filens indhold.
 */
String readCSVData() {
  File file = LittleFS.open(CSV_FILE, "r");
  String csvContent = "";
  if (file) {
    while (file.available()) {
      csvContent += (char)file.read();
    }
    file.close();
  } else {
    Serial.println("Kunne ikke åbne CSV fil til læsning");
  }
  return csvContent;
}
