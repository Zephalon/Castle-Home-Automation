#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Fingerprint.h>

// Wifi Settings
#define SSID                          ""
#define PASSWORD                      ""

// MQTT Settings
#define HOSTNAME                      "portal"
#define MQTT_SERVER                   ""
#define SENSOR_ENABLED_TOPIC          "portal/enabled"
#define OPEN_DOOR_TOPIC               "portal/open"
#define STATE_TOPIC                   "portal/mode/status"
#define MODE_LEARNING                 "portal/mode/learning"
#define MODE_READING                  "portal/mode/reading"
#define MODE_DELETE                   "portal/mode/delete"
#define AVAILABILITY_TOPIC            "portal/available"
#define DOORBELL_TOPIC                "portal/doorbell"
#define mqtt_username                 ""
#define mqtt_password                 ""

#define SENSOR_TX 12                  //GPIO Pin for RX
#define SENSOR_RX 14                  //GPIO Pin for TX

SoftwareSerial mySerial(SENSOR_TX, SENSOR_RX);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

WiFiClient wifiClient;                // Initiate WiFi library
PubSubClient client(wifiClient);      // Initiate PubSubClient library

uint8_t id = 0;                       //Stores the current fingerprint ID
uint8_t lastID = 0;                   //Stores the last matched ID
uint8_t lastConfidenceScore = 0;      //Stores the last matched confidence score
boolean modeLearning = false;
boolean modeReading = true;
boolean modeDelete = false;
boolean sensorOn = true;

//Declare JSON variables
DynamicJsonDocument mqttMessage(100);
char mqttBuffer[100];

// Pins
int PinTouch = 16;
int PinOpener = 13;
int PinRed = 4;
int PinBlue = 0;
int PinGreen = 2;
int PinStatus = 15;
int PinDoorbell = 5;

// Variables
int fail_counter = 0;
int doorbell_decay = 0;
int open_duration = 5000;
boolean prepared = false;

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      //Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
    lastID = finger.fingerID;
    lastConfidenceScore = finger.confidence;
    return p;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);

  return finger.fingerID;
}

uint8_t getFingerprintEnroll() {
  setLED(1, 0, 1);
  int p = -1;
  mqttMessage["mode"] = "learning";
  mqttMessage["id"] = id;
  mqttMessage["state"] = "Place finger..";
  size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
  client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  mqttMessage["mode"] = "learning";
  mqttMessage["id"] = id;
  mqttMessage["state"] = "Remove finger..";
  mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
  client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  mqttMessage["mode"] = "learning";
  mqttMessage["id"] = id;
  mqttMessage["state"] = "Place same finger..";
  mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
  client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    mqttMessage["mode"] = "learning";
    mqttMessage["id"] = id;
    mqttMessage["state"] = "Success, stored!";
    mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
    client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
    Serial.println("Stored!");
    return true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }
}

uint8_t deleteFingerprint() {
  uint8_t p = -1;

  p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    Serial.println("Deleted!");
    mqttMessage["mode"] = "deleting";
    mqttMessage["id"] = id;
    mqttMessage["state"] = "Deleted!";
    size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
    client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
    return true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not delete in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.print("Unknown error: 0x"); Serial.println(p, HEX);
    return p;
  }
}

void connectWIFI() {
  int wifi_retry = 0;

  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  Serial.print("Connecting...");

  while (WiFi.status() != WL_CONNECTED && (digitalRead(PinTouch) == HIGH || !prepared)) {
    delay(200);
    digitalWrite(PinStatus, HIGH);
    delay(100);
    digitalWrite(PinStatus, LOW);
    delay(100);
    digitalWrite(PinStatus, HIGH);
    delay(100);
    digitalWrite(PinStatus, LOW);
      
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
  while (WiFi.status() == WL_CONNECTED && !client.connected() && (digitalRead(PinTouch) == HIGH || !prepared)) {       // Loop until connected to MQTT server
    Serial.print("Attempting MQTT connection...");
    if (client.connect(HOSTNAME, mqtt_username, mqtt_password, AVAILABILITY_TOPIC, 1, true, "offline")) {       //Connect to MQTT server
      Serial.println("connected");
      client.publish(AVAILABILITY_TOPIC, "online");         // Once connected, publish online to the availability topic
      client.subscribe(MODE_LEARNING);       //Subscribe to Learning Mode Topic
      client.subscribe(MODE_READING);
      client.subscribe(MODE_DELETE);
      client.subscribe(SENSOR_ENABLED_TOPIC);
      client.subscribe(OPEN_DOOR_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in .5 seconds>");

      delay(400);
      digitalWrite(PinStatus, HIGH);
      delay(100);
      digitalWrite(PinStatus, LOW);
    }
  }
}

void MqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println(topic);

  digitalWrite(PinStatus, HIGH);
  delay(100);
  digitalWrite(PinStatus, LOW);
  delay(100);
  digitalWrite(PinStatus, HIGH);
  delay(100);
  digitalWrite(PinStatus, LOW);

  // add fingerprint
  if (strcmp(topic, MODE_LEARNING) == 0) {
    char charArray[3];
    for (int i = 0; i < length; i++) {
      //Serial.print((char)payload[i]);
      charArray[i] = payload[i];
    }
    id = atoi(charArray);
    if (id > 0 && id < 128) {
      setLED(1, 0, 1);
      Serial.println("Entering Learning mode");
      mqttMessage["mode"] = "learning";
      mqttMessage["id"] = id;
      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
      while (!getFingerprintEnroll());
      setLED(0, 0, 0);
      Serial.println("Exiting Learning mode");
      modeLearning = false;
      modeReading = true;
      modeDelete = false;
      id = 0;
    }
    Serial.println();
  }

  // delete fingerprint
  if (strcmp(topic, MODE_DELETE) == 0) {
    char charArray[3];
    for (int i = 0; i < length; i++) {
      //Serial.print((char)payload[i]);
      charArray[i] = payload[i];
    }
    id = atoi(charArray);
    if (id > 0 && id < 128) {
      mqttMessage["mode"] = "deleting";
      mqttMessage["id"] = id;
      size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
      client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
      Serial.println("Entering delete mode");
      while (! deleteFingerprint());
      Serial.println("Exiting delete mode");
      delay(2000); //Make the mqttMessage readable in HA
      modeLearning = false;
      modeReading = true;
      modeDelete = false;
      id = 0;
    }
    Serial.println();
  }

  // enable / disable sensor
  if (strcmp(topic, SENSOR_ENABLED_TOPIC) == 0) {
    String msg;
    for (int i = 0; i < length; i++) {
      msg += (char)payload[i];
    }

    if (msg == "on") {
      sensorOn = true;
      Serial.println("Turning sensor on");
    } else if (msg == "off") {
      sensorOn = false;
      Serial.println("Turning sensor off");
    }
  }

  // open the door
  if (strcmp(topic, OPEN_DOOR_TOPIC) == 0) {
    String msg;
    for (int i = 0; i < length; i++) {
      msg += (char)payload[i];
    }
    
    if (msg == "1") {
      doorbell_decay = 50;
      setLED(0, 1, 0);
      digitalWrite(PinOpener, HIGH);
      delay(open_duration);
      digitalWrite(PinOpener, LOW);
    }
  }
}

void setLED(int red, int green, int blue) {
  digitalWrite(PinRed, red == 1 ? HIGH : LOW);
  digitalWrite(PinGreen, green == 1 ? HIGH : LOW);
  digitalWrite(PinBlue, blue == 1 ? HIGH : LOW);
}

ICACHE_RAM_ATTR void sendDoorBellNotification() {
  if (doorbell_decay == 0) {
    Serial.println("Doorbell pressed");
    client.publish(DOORBELL_TOPIC, "pressed", false);
  }

  doorbell_decay = 50;
}

ICACHE_RAM_ATTR void fingerDetected() {
  setLED(1, 1, 1);
}

void setup() {
  pinMode(PinTouch, INPUT); // Connect 0 to T-Out (pin 5 on reader), T-3v to 3v
  Serial.begin(57600);
  while (!Serial);
  delay(100);
  Serial.println("\n\nWelcome to the MQTT Fingerprint Sensor program!");

  pinMode(PinOpener, OUTPUT);
  pinMode(PinRed, OUTPUT);
  pinMode(PinBlue, OUTPUT);
  pinMode(PinGreen, OUTPUT);
  pinMode(PinStatus, OUTPUT);
  pinMode(PinDoorbell, INPUT_PULLUP);
 
  attachInterrupt(digitalPinToInterrupt(PinDoorbell), sendDoorBellNotification, RISING);
  attachInterrupt(digitalPinToInterrupt(PinTouch), fingerDetected, FALLING);

  setLED(0, 0, 0);
  digitalWrite(PinStatus, LOW);

  delay(500);
  
  setLED(1, 0, 0);
  digitalWrite(PinStatus, HIGH);

  delay(500);

  // set the data rate for the sensor serial port
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    delay(1000);
    ESP.restart();
  }

  connectWIFI();

  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(MqttCallback);

  reconnectMQTT();

  setLED(0, 0, 0);
  prepared = true;
}

void loop() {
  setLED(0, 0, 0);

  int fingerState = digitalRead(PinTouch); // Read T-Out, normally HIGH (when no finger)

  if (sensorOn == false) {
    setLED(1, 0, 0);
  }

  if (fingerState == HIGH) {
    if (WiFi.status() != WL_CONNECTED) {
      connectWIFI();
    }

    if (!client.connected()) {
      reconnectMQTT();
    }

    if (WiFi.status() == WL_CONNECTED && client.connected()) {
      digitalWrite(PinStatus, LOW); // redundant
    } else {
      digitalWrite(PinStatus, HIGH);
    }
  } else if (sensorOn == false) {
    setLED(1, 0, 0);
  } else {
    setLED(0, 0, 1);
    doorbell_decay = 50;
    if (modeReading == true && modeLearning == false) {
      uint8_t result = getFingerprintID();
      if (result == FINGERPRINT_OK) {
        setLED(0, 1, 0);
        digitalWrite(PinOpener, HIGH);
        mqttMessage["mode"] = "reading";
        mqttMessage["id"] = lastID;
        mqttMessage["state"] = "Matched";
        mqttMessage["confidence"] = lastConfidenceScore;
        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
        fail_counter = 0;
        delay(open_duration);
        digitalWrite(PinOpener, LOW);
      } else if (result == FINGERPRINT_NOTFOUND) {
        setLED(1, 0, 0);
        mqttMessage["mode"] = "reading";
        mqttMessage["match"] = false;
        mqttMessage["id"] = id;
        mqttMessage["state"] = "Not matched";
        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
        fail_counter++;
        delay(fail_counter < 25 ? 0 : fail_counter * 1000);
      } else if (result == FINGERPRINT_NOFINGER) {
        setLED(0, 0, 1);
        mqttMessage["mode"] = "reading";
        mqttMessage["id"] = id;
        mqttMessage["state"] = "Waiting";
        size_t mqttMessageSize = serializeJson(mqttMessage, mqttBuffer);
        client.publish(STATE_TOPIC, mqttBuffer, mqttMessageSize);
      } else {

      }
    }
  }

  if (doorbell_decay > 0) doorbell_decay--;

  client.loop();
  delay(fail_counter > 0 ? 50 : 100);            //don't need to run this at full speed.
}
