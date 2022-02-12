#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <RTCVars.h>
#include <credentials.h>

// MQTT Settings
#define HOSTNAME                      "temperature_sensor"
#define AVAILABILITY_TOPIC            "sensors/" HOSTNAME "/available"
#define SENSOR_TOPIC                  "sensors/" HOSTNAME
#define MAXIMUM_INTERVAL              15

const unsigned char One_Wire_Bus = 0;

// declare JSON variables
DynamicJsonDocument mqttMessage(100);
char mqttBuffer[100];

OneWire oneWire(One_Wire_Bus);
DallasTemperature sensors(&oneWire);
WiFiClient wifiClient; // initiate WiFi library
PubSubClient client(wifiClient); // initiate PubSubClient library
RTCVars state; // create the state object

float temperature;
float temperature_reported;
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
    if (client.connect(HOSTNAME, MQTT_USERNAME, MQTT_PASSWORD, AVAILABILITY_TOPIC, 1, true, "offline")) { // connect to MQTT server
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

void setup() {
  Serial.begin(57600);
  while (!Serial);
  delay(100);

  Wire.begin();

  pinMode(BUILTIN_LED, OUTPUT);

  // keep these variables
  state.registerVar( &reset_counter );
  state.registerVar( &temperature_reported );

  if (state.loadFromRTC()) {
    // there was something stored in the RTC
    reset_counter++;
    Serial.println("Minutes since last report: " + (String)reset_counter);
  } else {
    // cold boot
    reset_counter = 0;
    temperature_reported = -273;
    Serial.println("This seems to be a cold boot.");
  }

   state.saveToRTC();
}

void loop() {
    sensors.requestTemperatures();
    temperature = sensors.getTempCByIndex(0);

    // check if a new report is necessary
    if (reset_counter > MAXIMUM_INTERVAL || abs(temperature - temperature_reported) > 0.1) {
      // send it
      Serial.println("Sending the data, Delta: " + (String)abs(temperature - temperature_reported));
      
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
  
      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      if (client.publish(SENSOR_TOPIC, mqttBuffer, mqttMessageSize)) {
        Serial.println("Data sent, TOPIC: " + (String)SENSOR_TOPIC);
      }
  
      temperature_reported = temperature;

      reset_counter = 0;
    }

    state.saveToRTC();

    delay(100);

    ESP.deepSleep(60 * 1000000); // sleep for one minute
}
