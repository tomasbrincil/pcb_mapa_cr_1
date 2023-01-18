#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "Freenove_WS2812_Lib_for_ESP32.h"
#define LEDS_COUNT  77
#define LEDS_PIN	27
#define CHANNEL		0
const char* ssid     = "wifi";
const char* password = "heslo";
const char* serverName = "http://tmep.cz/vystup-json.php?okresy_cr=1";
unsigned long lastTime = 0;
unsigned long timerDelay = 1000;
int lastid, value, color;
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
  if ((millis() - lastTime) > timerDelay) {
    if(WiFi.status()== WL_CONNECTED){   
      sensorReadings = httpGETRequest(serverName);
      JSONVar myObject = JSON.parse(sensorReadings);
      if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!");
        return;
      }
      for (int i = 0; i < 77; i++) {
        color = map(myObject[i]["h1"], -15, 40, 170, 0);           
        strip.setLedColorData(i, strip.Wheel(color));
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