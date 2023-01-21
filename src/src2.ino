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
#include <Arduino.h>
#include <WiFiManager.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Arduino_JSON.h>
#include <HTTPClient.h>
#include "Freenove_WS2812_Lib_for_ESP32.h"
#define JSON_CONFIG_FILE "/config.json"
#define ESP_DRD_USE_SPIFFS true
#define TRIGGER_PIN 0
#define LEDS_COUNT 77
#define LEDS_PIN 27
#define CHANNEL 0

char json_url[256];
char json_led[2];
char json_button[2];
bool wm_nonblocking = false;
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;
int lastid, value, color;

WiFiManager wm;
String sensorReadings;
WiFiManagerParameter custom_json_server("server", "JSON source url address", json_url, 256);
WiFiManagerParameter custom_led_pin("led", "LED GPIO", json_led, 2);
WiFiManagerParameter custom_reset_button("button", "WIFI GPIO", json_button, 2);
Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);

void setup()
{
	WiFi.mode(WIFI_STA);
	Serial.begin(115200);
	Serial.setDebugOutput(true);
	delay(3000);
	Serial.println("Running");
	if (wm_nonblocking) wm.setConfigPortalBlocking(false);
	wm.addParameter(&custom_json_server);
  wm.addParameter(&custom_led_pin);
  wm.addParameter(&custom_reset_button);
	wm.setSaveParamsCallback(saveParamCallback);
	std::vector<const char*> menu = { "wifi", "info", "param", "sep", "restart", "exit" };

	wm.setMenu(menu);
	wm.setClass("invert");
	wm.setConfigPortalTimeout(30);

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

	Serial.println("Loading json_url from SPIFFS");
  saveConfigFile();
	loadConfigFile();
	Serial.println(json_url);
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

bool loadConfigFile()
{
	// Uncomment if we need to format filesystem
	//SPIFFS.format();
	Serial.println("Mounting SPIFFS");
	if (SPIFFS.begin(false) || SPIFFS.begin(true))
	{
		Serial.println("Mounted file system");
		if (SPIFFS.exists(JSON_CONFIG_FILE))
		{
			Serial.println("Reading config file");
			File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
			if (configFile)
			{
				Serial.println("Opened config file");
				StaticJsonDocument<512> json;
				DeserializationError error = deserializeJson(json, configFile);
				serializeJsonPretty(json, Serial);
				if (!error)
				{
          if(json["json_url"] != "" && json["json_led"] != "" && json["json_button"] != "")
          {
            Serial.println("Initiating config first");
            strcpy(json_url, "http://tmep.cz/vystup-json.php?okresy_cr=1");
            strcpy(json_led, "27");
            strcpy(json_button, "0");
            saveConfigFile();
          }
					Serial.println("Parsing JSON");
					strcpy(json_url, json["json_url"]);
          strcpy(json_led, json["json_led"]);
          strcpy(json_button, json["json_button"]);
					return true;
				}
				else
				{
					Serial.println("Failed to load json config");
				}
			}
		}
	}
	else
	{
		Serial.println("Failed to mount FS");
	}

	return false;
}

void saveConfigFile()
{
	Serial.println("Saving configuration");
	StaticJsonDocument<512> json;
	json["json_url"] = json_url;
	File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
	if (!configFile)
	{
		Serial.println("Failed to open config file for writing");
	}

	serializeJsonPretty(json, Serial);
	if (serializeJson(json, configFile) == 0)
	{
		Serial.println("Failed to write to config file");
	}

	configFile.close();
}

void saveParamCallback()
{
	Serial.println(custom_json_server.getValue());
	strcpy(json_url, custom_json_server.getValue());
  strcpy(json_led, custom_led_pin.getValue());
  strcpy(json_button, custom_reset_button.getValue());
	saveConfigFile();
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
			JSONVar myObject = JSON.parse(sensorReadings);
			if (JSON.typeof(myObject) == "undefined")
			{
				Serial.println("Parsing input failed!");
				return;
			}

			for (int i = 0; i < 77; i++)
			{
				color = map(myObject[i]["h1"], -15, 40, 170, 0);
				strip.setLedColorData(i, strip.Wheel(color));
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