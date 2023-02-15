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
#include <FS.h>
#include <Arduino.h>
#include <WiFiManager.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson v6+
#include <HTTPClient.h>
#include "Freenove_WS2812_Lib_for_ESP32.h"
#define JSON_CONFIG_FILE "/config.json"
#define ESP_DRD_USE_SPIFFS true
#define TRIGGER_PIN 0
#define LEDS_COUNT 77
#define LEDS_PIN 27
#define CHANNEL 0

// Default values
char json_url[256] = "http://cdn.tmep.cz/app/export/okresy-cr-teplota.json";
char json_led[8] = "27";
char json_button[8] = "0";
bool wm_nonblocking = false;
unsigned long lastTime = 0;
unsigned long timerDelay = 20000;
int lastid, value, color;

WiFiManager wm;
String sensorReadings;
WiFiManagerParameter custom_json_server("server", "JSON source url address", json_url, 256);
WiFiManagerParameter custom_led_pin("led", "LED GPIO", json_led, 2);
WiFiManagerParameter custom_reset_button("button", "WIFI GPIO", json_button, 2);
Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);
bool shouldSaveConfig = false;
bool spiffsEnabled = false;

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup()
{
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(3000);
  Serial.println("Running");
  Serial.println("Mounting SPIFFS");
  if (SPIFFS.begin()) {
    Serial.println("Mounted file system");
    spiffsEnabled = true;
    loadConfigFile();
  }

  if (wm_nonblocking) wm.setConfigPortalBlocking(false);
  wm.addParameter(&custom_json_server);
  wm.addParameter(&custom_led_pin);
  wm.addParameter(&custom_reset_button);
  wm.setSaveParamsCallback(saveConfigCallback);
  std::vector<const char*> menu = { "wifi", "info", "param", "sep", "restart", "exit" };

  wm.setMenu(menu);
  wm.setClass("invert");
  //wm.setConfigPortalTimeout(30);

  bool res = wm.autoConnect("MapaTvojiMamy", "12345678");
  if (!res)
  {
    Serial.println("Failed to connect or hit timeout");
    ESP.restart();
  }
  else
  {
    Serial.println("Connected");
  }

  if (shouldSaveConfig && spiffsEnabled) {
    saveConfigFile();
  }
  Serial.println("Initiating WS2818 LED strip");
  strip.begin();
  Serial.println("Brightness set");
  strip.setBrightness(10);
  Serial.println("WiFi reset button set");
  pinMode(TRIGGER_PIN, INPUT);
}

void checkButton()
{
  if (digitalRead(TRIGGER_PIN) == LOW)
  {
    delay(50);
    if (digitalRead(TRIGGER_PIN) == LOW)
    {
      Serial.println("Button pressed");
      delay(3000);
      if (digitalRead(TRIGGER_PIN) == LOW)
      {
        Serial.println("Button held");
        Serial.println("Erasing WiFi config, restarting");
        wm.resetSettings();
        ESP.restart();
      }

      Serial.println("Starting config portal");
      wm.setConfigPortalTimeout(120);
      if (!wm.startConfigPortal("MapaTvojiMamy", "12345678"))
      {
        Serial.println("Failed to connect or hit timeout");
        delay(3000);
        ESP.restart();
      }
      else
      {
        Serial.println("Conneted");
      }
    }
  }
}

String getParam(String name)
{
  String value;
  if (wm.server->hasArg(name))
  {
    value = wm.server->arg(name);
  }

  return value;
}

void loadConfigFile()
{
  // Uncomment if we need to format filesystem
  //SPIFFS.format();

  if (SPIFFS.exists(JSON_CONFIG_FILE))
  {
    Serial.println("Reading config file");
    File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
    if (configFile)
    {
      Serial.println("opened config file");
      size_t size = configFile.size();
      // Allocate a buffer to store contents of the file.
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);
      DynamicJsonDocument json(1024);
      auto deserializeError = deserializeJson(json, buf.get());
      serializeJson(json, Serial);
      if (!deserializeError) {

        Serial.println("\nparsed json");

        if (json["json_url"]) {
          Serial.println("setting custom url from config");
          strcpy(json_url, json["json_url"]);
          Serial.println(json_url);
        } else {
          Serial.println("no custom url in config");
        }

      } else {
        Serial.println("Error deserializing json");
      }
      configFile.close();
    } else {
      Serial.println("error opening the file");
    }
  } else {
    Serial.println("Config file not present");
  }
}

void saveConfigFile()
{
  Serial.println("Saving configuration");
  //read updated parameters
  strcpy(json_url, custom_json_server.getValue());
  strcpy(json_led, custom_led_pin.getValue());
  strcpy(json_button, custom_reset_button.getValue());
  DynamicJsonDocument json(1024);
  json["json_url"] = json_url;
  json["json_led"] = json_led;
  json["json_button"] = json_button;
  Serial.println(json_url);
  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile)
  {
    Serial.println("Failed to open config file for writing");
  }
  serializeJson(json, Serial);
  serializeJson(json, configFile);
  configFile.close();
}

String httpGETRequest(const char *serverName)
{
  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverName);
  int httpResponseCode = http.GET();
  String payload = "{}";
  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
  return payload;
}

void loop()
{
  if (wm_nonblocking) wm.process();
  checkButton();
  if ((millis() - lastTime) > timerDelay)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      sensorReadings = httpGETRequest(json_url);
      DynamicJsonDocument doc(6144);
      DeserializationError error = deserializeJson(doc, sensorReadings);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      for (JsonObject item : doc.as<JsonArray>()) {
        int ledIndex = item["id"];
        ledIndex -= 1;
        double h = item["h"];
        color = map(h, -15, 40, 170, 0);
        strip.setLedColorData(ledIndex, strip.Wheel(color));
      }
      strip.show();
    }
    else
    {
      Serial.println("WiFi Disconnected");
    }

    lastTime = millis();
  }
}
