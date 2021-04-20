#include <Arduino.h>

// softwareserial,pubsub and dht libraries linked in platform.ini library deps
#include <SoftwareSerial.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include <DHT.h>
#define DHTPIN D4
// #define DHTTYPE DHT11 // cheaper DHT11 version like in wemos d1 mini DHT shield
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// credentials to wifi and mqtt in here
#include "config.h"

// 2 seconds
#define publish_rate_ms 30000

#define humidity_topic "hass/humidity"
#define temperature_topic "hass/temperature"
#define co2_topic "hass/co2"

WiFiClient espClient;

int CO2Value;              // CO2 Measuring value in ppm
int readInterval = 1;     // 1 Minute

SoftwareSerial co2Serial(D2, D1); // RX, TX Pins festlegen

// from http://esp8266-server.de/CO2Ampel.html
int leseCO2() {
  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  char response[9];
  co2Serial.write(cmd, 9);
  co2Serial.readBytes(response, 9);

  Serial.println("Raw co2 responses ");
  Serial.println(response[0], HEX);
  Serial.println(response[1], HEX);

  if (response[0] != 0xFF) return -1;
  if (response[1] != 0x86) return -1;
  int responseHigh = (int) response[2]; // CO2 High Byte
  int responseLow = (int) response[3];  // CO2 Low Byte
  int ppm = (256 * responseHigh) + responseLow;

  // 410 seems to be a common value happening when is invalid to for some reason
  if (ppm == 410){
    Serial.println("CO2 was strange 410 value");
    return -1;
  } else if(ppm == 5000) {
    Serial.println("Returned max value of 5000");
    return -1;
  }

  return ppm;                         // Response of MH-Z19 CO2 Sensors in ppm
}
/*///////////////////////////////////////////////////////*/


void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived maybe?
}

PubSubClient mqttClient(espClient);

void setupWifi() {
  // network
  WiFi.begin(SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void setupArduinoOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  // serial ports and sensors
  Serial.begin(9600);
  co2Serial.begin(9600);
  dht.begin();

  // network
  setupWifi();

  //Start mDNS
  if (MDNS.begin("sensors")) { 
    Serial.println("MDNS started");
  }

  // currently non ssl TODO change that
  mqttClient.setServer(mqtt_server, mqtt_port);
}

void reconnectMQTT() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP8266Sensors")) { //, mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

long lastMsg = 0;
int readingCount = 0;
void loop() {
  // connections and looping
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
  ArduinoOTA.handle();

  // sending of messages
  long now = millis();
  if (now - lastMsg > publish_rate_ms) {
    readingCount++;

    // humidity and temperature
    float humidity = dht.readHumidity();
    delay(2000);
    float temp = dht.readTemperature();  
    delay(2000);
    if (isnan(humidity) || isnan(temp)) {
      Serial.println("DHT sensor could not be read out");
    } else {
      Serial.print("Humidity: ");
      Serial.println(humidity);
      Serial.print("Temperature: ");
      Serial.println(temp);
      mqttClient.publish(temperature_topic, String(temp).c_str());
      mqttClient.publish(humidity_topic, String(humidity).c_str());
    }

    // CO2
    int currentCO2 = leseCO2();
    if (currentCO2 != -1){
      Serial.println("co2: " + String(currentCO2));
      mqttClient.publish(co2_topic, String(currentCO2).c_str());
      // TODO retain all messages via true at end?
      // mqttClient.publish(co2_topic, String(currentCO2).c_str(), true);
    } else {
      Serial.println("Unable to read C02 value");
    }
    lastMsg = now;
  }

  if (readingCount >= 2) {
    // don't know why but co2 sensor is only working on first use so restart software serial, maybe dht kills software serial or mqtt
    Serial.println("Reset in 5 seconds");
    delay(5000);
    ESP.restart();


    // co2Serial.flush();
    // co2Serial.end();
    // co2Serial.begin(9600);
  }
}
