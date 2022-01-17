#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <BH1750.h>

// Wifi Settings
#define SSID                          ""
#define PASSWORD                      ""

// MQTT Settings
#define HOSTNAME                      "light_sensor"
#define MQTT_SERVER                   ""
#define AVAILABILITY_TOPIC            "sensors/light_sensor/available"
#define SENSOR_TOPIC                  "sensors/test"
#define mqtt_username                 ""
#define mqtt_password                 ""

// declare JSON variables
DynamicJsonDocument mqttMessage(100);
char mqttBuffer[100];

// pins
const unsigned char One_Wire_Bus = 0;

OneWire oneWire(One_Wire_Bus);
DallasTemperature sensors(&oneWire);
WiFiClient wifiClient;                // Initiate WiFi library
PubSubClient client(wifiClient);      // Initiate PubSubClient library

BH1750 lightMeter;

float temperature;
float light_level;

void connectWIFI() {
  int wifi_retry = 0;

  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  Serial.print("Connecting...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(100);
    digitalWrite(BUILTIN_LED, LOW);
    delay(100);
    digitalWrite(BUILTIN_LED, HIGH);
    delay(100);
    digitalWrite(BUILTIN_LED, LOW);
      
    Serial.print(".");

    if (wifi_retry++ > 50) {
      Serial.println("\nReboot");
      ESP.restart();
    }
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());                     // Print IP address
}

void reconnectMQTT() {
  while (WiFi.status() == WL_CONNECTED && !client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(HOSTNAME, mqtt_username, mqtt_password, AVAILABILITY_TOPIC, 1, true, "offline")) { // connect to MQTT server
      Serial.println("connected");
      client.publish(AVAILABILITY_TOPIC, "online"); // once connected, publish online to the availability topic
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in .5 seconds>");

      delay(400);
      digitalWrite(BUILTIN_LED, HIGH);
      delay(100);
      digitalWrite(BUILTIN_LED, LOW);
    }
  }
}

void MqttCallback(char* topic, byte* payload, unsigned int length) {
  // void
}

void setup() {
  Serial.begin(57600);
  while (!Serial);
  delay(100);

  Wire.begin();
  lightMeter.begin();

  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);

  delay(500);

  digitalWrite(BUILTIN_LED, HIGH);

  delay(500);

  connectWIFI();

  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(MqttCallback);

  reconnectMQTT();

  digitalWrite(BUILTIN_LED, LOW);
}

void loop() {
     if (WiFi.status() != WL_CONNECTED) {
      connectWIFI();
    }

    if (!client.connected()) {
      reconnectMQTT();
    }

    if (WiFi.status() == WL_CONNECTED && client.connected()) {
      digitalWrite(BUILTIN_LED, LOW); // redundant
    } else {
      digitalWrite(BUILTIN_LED, HIGH);
    }

    sensors.requestTemperatures();
    temperature = sensors.getTempCByIndex(0);
    light_level = lightMeter.readLightLevel();

    if (!isnan(temperature)) {
      mqttMessage["sensor"] = HOSTNAME;
      mqttMessage["status"] = "data";
      mqttMessage["temperature"] = temperature;
      mqttMessage["light_level"] = light_level;

      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      client.publish(SENSOR_TOPIC, mqttBuffer, mqttMessageSize);

      Serial.println(temperature);

      delay(100);

      ESP.deepSleep(15 * 60 * 1000000);
    } else {
      Serial.println("Unable to read sensor values.");
      delay(3000);
    }

    delay(100);
}
