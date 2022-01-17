#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const int LEVEL = 1;
const char* SSID = "";
const char* PSK = "";
const char* MQTT_BROKER = "";
const char* MQTT_ID = "heating_controller_1";
const char* MQTT_TOPIC = "heating/level1/+";

int RelaisTarget[4] = {0, 0, 0, 0};
int RelaisStatus[4] = {0, 0, 0, 0};

int PinWifi = 16;
int PinStatus[4] = {14, 12, 13, 15};
int PinRelais[4] = {5, 4, 0, 2};

bool relais_locked = false;
int relais_cooldown = 90;
int relais_counter = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);

  // set pin modes
  pinMode(PinWifi, OUTPUT);
  digitalWrite(PinWifi, LOW);

  for (int i = 0; i < 4; i = i + 1) {
    pinMode(PinStatus[i], OUTPUT);
    pinMode(PinRelais[i], OUTPUT);
  }

  // setup wifi
  connect_wifi();

  digitalWrite(PinWifi, HIGH);

  client.setServer(MQTT_BROKER, 1883);
  client.setCallback(callback);
}

void connect_wifi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PSK);

  digitalWrite(PinWifi, HIGH);
  delay(100);
  digitalWrite(PinWifi, LOW);
  delay(100);
}

void loop() {
  int wifi_retry = 0;

  while (WiFi.status() != WL_CONNECTED && wifi_retry < 5) {
    wifi_retry++;
    connect_wifi();

    if (wifi_retry >= 50) {
      Serial.println("\nReboot");
      ESP.restart();
    }
  }

  digitalWrite(PinWifi, LOW);
  delay(50);
  digitalWrite(PinWifi, HIGH);

  if (!client.connected()) {
    while (!client.connected()) {
      client.connect(MQTT_ID);
      client.subscribe(MQTT_TOPIC);
      digitalWrite(PinWifi, LOW);
      delay(50);
      digitalWrite(PinWifi, HIGH);
      delay(100);
    }
    Serial.println(MQTT_TOPIC);
    Serial.println("MQTT connected");
  }

  if (relais_locked) relais_counter++;

  if (relais_counter >= relais_cooldown) {
    relais_locked = false;
    relais_counter = 0;
  }

  set_relais();

  delay(1000);

  client.loop();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Received message [");
  Serial.print(topic);
  Serial.print("] ");
  char msg[length + 1];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    msg[i] = (char)payload[i];
  }
  Serial.println();

  msg[length] = '\0';

  int relais_id = atoi(&topic[strlen(topic) - 1]);
  int relais_status = atoi(msg);

  RelaisTarget[relais_id] = relais_status;
  Serial.print("Relais [");
  Serial.print(relais_id);
  Serial.print("] ");
  Serial.print(RelaisTarget[relais_id]);
  Serial.println("");
}

void set_relais() {
  for (int i = 0; i < 4; i = i + 1) {
    digitalWrite(PinStatus[i], LOW);

    if (RelaisTarget[i] != RelaisStatus[i]) {
      char topic[34];
      snprintf(topic, sizeof(topic), "device/heating/level%d/actuator/%d", LEVEL, i);
      
      if (RelaisTarget[i] == 1) {
        // turn on
        if (!relais_locked) {
          digitalWrite(PinStatus[i], HIGH);
          digitalWrite(PinRelais[i], HIGH);
          RelaisStatus[i] = 1;
          relais_locked = true;

          client.publish(topic, "1");
        } else {
          digitalWrite(PinStatus[i], relais_counter % 2 ? HIGH : LOW);
        }
      } else {
        // turn off
        digitalWrite(PinRelais[i], LOW);
        RelaisStatus[i] = 0;
        client.publish(topic, "0");
      }
    } else {
      // simply show status led
      digitalWrite(PinStatus[i], RelaisStatus[i] ? HIGH : LOW);
    }
  }
}
