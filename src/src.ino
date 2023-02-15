/*
  __  __                _______         _ _ __  __
  |  \/  |              |__   __|       (_|_)  \/  |
  | \  / | __ _ _ __   __ _| |_   _____  _ _| \  / | __ _ _ __ ___  _   _
  | |\/| |/ _` | '_ \ / _` | \ \ / / _ \| | | |\/| |/ _` | '_ ` _ \| | | |
  | |  | | (_| | |_) | (_| | |\ V / (_) | | | |  | | (_| | | | | | | |_| |
  |_|  |_|\__,_| .__/ \__,_|_| \_/ \___/| |_|_|  |_|\__,_|_| |_| |_|\__, |
              | |                     _/ |                          __/ |
              |_|                    |__/                          |___/
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson v6+
#include "Freenove_WS2812_Lib_for_ESP32.h"
#define LEDS_COUNT  77
#define LEDS_PIN	27
#define CHANNEL		0

const char* ssid     = "Internet";
const char* password = "lopata#*";
const char* json_url = "http://cdn.tmep.cz/app/export/okresy-cr-teplota.json";
unsigned long lastTime = 0;
unsigned long timerDelay = 60000;
int lastid, value, color, first_time;
Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);
String sensorReadings;
float sensorReadingsArr[3];

void setup()
{
    Serial.begin(115200);
    delay(10);
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    strip.begin();
    strip.setBrightness(10); 
}
void loop()
{
  // Did we wait long enough or was it just powered on?
  if ((millis() - lastTime) > timerDelay || first_time == 0) {
    if(WiFi.status()== WL_CONNECTED){
      first_time = 1;
      sensorReadings = httpGETRequest(json_url);
      DynamicJsonDocument doc(6144);
      DeserializationError error = deserializeJson(doc, sensorReadings);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      //int ledIndex = 0;
      for (JsonObject item : doc.as<JsonArray>()) {
        int ledIndex = item["id"];
        ledIndex -= 1;
        double h = item["h"];
        color = map(h, -15, 40, 170, 0);
        strip.setLedColorData(ledIndex, strip.Wheel(color));
      }
      strip.show();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }
}
String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverName);
  int httpResponseCode = http.GET();
  String payload = "{}"; 
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  return payload;
}
