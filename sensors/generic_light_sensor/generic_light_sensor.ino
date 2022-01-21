#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <BH1750.h>
#include <RTCVars.h>

// Wifi Settings
#define SSID                          ""
#define PASSWORD                      ""

// MQTT Settings
#define HOSTNAME                      "light_sensor"
#define MQTT_SERVER                   ""
#define AVAILABILITY_TOPIC            "sensors/light_sensor/available"
#define SENSOR_TOPIC                  "sensors/light_sensor"
#define MQTT_USERNAME                 ""
#define MQTT_PASSWORD                 ""
#define MAXIMUM_INTERVAL              15

const unsigned char One_Wire_Bus = 0;

//Declare JSON variables
DynamicJsonDocument mqttMessage(100);
char mqttBuffer[100];

OneWire oneWire(One_Wire_Bus);
DallasTemperature sensors(&oneWire);
WiFiClient wifiClient;                // Initiate WiFi library
PubSubClient client(wifiClient);      // Initiate PubSubClient library
RTCVars state; // create the state object

BH1750 lightMeter;

float temperature;
float light_level;
float temperature_reported;
float light_level_reported;
int reset_counter;

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
  client.setServer(MQTT_SERVER, 1883);
    
  while (WiFi.status() == WL_CONNECTED && !client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(HOSTNAME, MQTT_USERNAME, MQTT_PASSWORD, AVAILABILITY_TOPIC, 1, true, "offline")) {       //Connect to MQTT server
      Serial.println("connected");
      client.publish(AVAILABILITY_TOPIC, "online");         // Once connected, publish online to the availability topic
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

void setup() {
  Serial.begin(57600);
  while (!Serial);
  delay(100);

  Wire.begin();
  lightMeter.begin();

  pinMode(BUILTIN_LED, OUTPUT);

  // keep these variables
  state.registerVar( &reset_counter );
  state.registerVar( &temperature_reported );
  state.registerVar( &light_level_reported );

  if (state.loadFromRTC()) {
    // there was something stored in the RTC
    reset_counter++;
    Serial.println("Minutes since last report: " + (String)reset_counter);
  } else {
    // cold boot
    reset_counter = 0;
    temperature_reported = -273;
    light_level_reported = 0;
    Serial.println("This seems to be a cold boot.");
  }

   state.saveToRTC();
}

void loop() {
    sensors.requestTemperatures();
    temperature = sensors.getTempCByIndex(0);
    light_level = lightMeter.readLightLevel();

    // check if a new report is necessary
    if (reset_counter > MAXIMUM_INTERVAL || abs(temperature - temperature_reported) > 0.2 || abs(light_level - light_level_reported) > 20) {
      // send it
      Serial.println("Sending the data, Delta: " + (String)abs(temperature - temperature_reported) + " / " + (String)abs(light_level - light_level_reported));
      
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
  
      mqttMessage["sensor"] = HOSTNAME;
      mqttMessage["status"] = "data";
      mqttMessage["temperature"] = temperature;
      mqttMessage["light_level"] = light_level;
  
      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      if (client.publish(SENSOR_TOPIC, mqttBuffer, mqttMessageSize)) {
        Serial.println("Data sent, TOPIC: " + (String)SENSOR_TOPIC);
      }
  
      temperature_reported = temperature;
      light_level_reported = light_level;

      reset_counter = 0;
    }

    state.saveToRTC();

    delay(100);

    ESP.deepSleep(60 * 1000000); // sleep for one minute
}
