// Squeeze Ball Controller for Vespera - CASA0014
// Connection code adapted from the CASA0014 workshop sketch (mkr1010_mqtt_simple)
// What it does:
//    : Reads a force sensor (FSR) on pin A0
//    : The harder the sensor is squeezed, the more of Vespera's 12 columns light up,
//     making the colours shifts from light blue to purple to red
//    : If let go, the Vespera turns off
//
// Wiring: sensor from power (VCC) to the measuring pin (A0), resistor from A0 to ground (GND)

#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include "arduino_secrets.h"
#include <utility/wifi_drv.h>   // lights the small RGB status light on the board

// Settings

String lightId = "12";          // My Vespera number 

const int SQUEEZE_MIN = 20;     // below this means the sensor is not squeezed
const int SQUEEZE_MAX = 900;    // hard squeeze (sensor reached about 900 on testing)

// Connection details 

const char* ssid          = SECRET_SSID;    // home WiFi
const char* password      = SECRET_PASS;
const char* ssid1         = SECRET_SSID1;   // classroom WiFi
const char* password1     = SECRET_PASS1;
const char* mqtt_username = SECRET_MQTTUSER;
const char* mqtt_password = SECRET_MQTTPASS;
const char* mqtt_server   = "mqtt.cetools.org";
const int   mqtt_port     = 1884;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

String mqtt_topic = "student/CASA0014/luminaire/" + lightId;
String clientId = "";

//  Vespera layout: 12 columns x 6 lights = 72 lights 

const int num_leds     = 72;
const int num_columns  = 12;
const int leds_per_col = 6;
const int payload_size = num_leds * 3;   // 3 numbers (red, green, blue) per light
byte RGBpayload[payload_size];

// Squeeze tracking 

float smoothSqueeze = 0;        // smoothed reading, so the lights wouldn't flicker
int lastSentLevel = -1;         // last message sent, only send again if it changes
unsigned long lastSendTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("Starting Squeeze Ball Controller..."));

  pinMode(LED_BUILTIN, OUTPUT);

  // Small RGB status light: blue = connecting, green = connected, red = problem
  WiFiDrv::pinMode(25, OUTPUT);
  WiFiDrv::pinMode(26, OUTPUT);
  WiFiDrv::pinMode(27, OUTPUT);
  statusLight(155, 0, 0);

  // Give this board a unique name on the server, using MAC address
  byte mac[6];
  WiFi.macAddress(mac);
  char macStr[13];
  snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  clientId = "SqueezeBall_" + String(macStr);

  Serial.print(F("Sending to topic: "));
  Serial.println(mqtt_topic);

  startWifi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(30);
}

void loop() {
  // 1. Stay connected
  if (WiFi.status() != WL_CONNECTED) startWifi();
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();

  // 2. Reads the squeeze and incorporates it with the last reading so lights won't flicker
  int raw = analogRead(A0);
  smoothSqueeze = smoothSqueeze * 0.7 + raw * 0.3;

  // 3. Turns the squeeze into a level from 0 (not squeezed) to 100 (hard squeeze)
  int level = 0;
  if (smoothSqueeze > SQUEEZE_MIN) {
    level = map((int)smoothSqueeze, SQUEEZE_MIN, SQUEEZE_MAX, 0, 100);
    level = constrain(level, 0, 100);
  }

  // the small light on the board also glows when squeezed, to check the sensor works without Vespera
  analogWrite(LED_BUILTIN, map(level, 0, 100, 0, 255));

  // 4. only send to Vespera if the squeeze has changed, and no more than 10 times a second
  bool changed = abs(level - lastSentLevel) >= 2 || (level == 0 && lastSentLevel != 0);
  if (changed && millis() - lastSendTime > 100) {
    sendSqueezeToVespera(level);
    lastSentLevel = level;
    lastSendTime = millis();

    Serial.print(F("Squeeze reading: "));
    Serial.print(raw);
    Serial.print(F("   Level: "));
    Serial.println(level);
  }

  delay(20);
}

// works out which lights to turn on and what colour from the squeeze level (0-100), then sends it to Vespera
void sendSqueezeToVespera(int level) {
  if (!mqttClient.connected()) return;

  // How many of the 12 columns to light (squeeze harder = more columns)
  int columnsLit = map(level, 0, 100, 0, num_columns);
  if (level > 0 && columnsLit == 0) columnsLit = 1;   // so that even a light squeeze shows something

  // Colour: soft touch = light blue, medium = purple, hard squeeze = full red
  int r, g, b;
  if (level <= 50) {
    // first half: light blue (0,150,255) fades to purple (150,0,255)
    r = map(level, 0, 50, 0, 150);
    g = map(level, 0, 50, 150, 0);
    b = 255;
  } else {
    // second half: purple (150,0,255) fades to full red (255,0,0)
    r = map(level, 50, 100, 150, 255);
    g = 0;
    b = map(level, 50, 100, 255, 0);
  }
  if (level == 0) { r = 0; g = 0; b = 0; }   // not squeezed = all lights off

  for (int col = 0; col < num_columns; col++) {
    for (int i = 0; i < leds_per_col; i++) {
      int pixel = col * leds_per_col + i;
      bool on = col < columnsLit;
      RGBpayload[pixel * 3 + 0] = on ? r : 0;
      RGBpayload[pixel * 3 + 1] = on ? g : 0;
      RGBpayload[pixel * 3 + 2] = on ? b : 0;
    }
  }

  mqttClient.publish(mqtt_topic.c_str(), RGBpayload, payload_size);
}

// Connections

// Tries home WiFi first, then classroom WiFi
void startWifi() {
  statusLight(0, 0, 155);   // blue = connecting

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("WiFi module not found!"));
    while (true);
  }

  if (!tryWifi(ssid, password)) {
    tryWifi(ssid1, password1);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("WiFi connected to: "));
    Serial.println(WiFi.SSID());
    statusLight(0, 155, 0);   // green
  } else {
    Serial.println(F("WiFi failed on both networks - check the names and passwords in arduino_secrets.h"));
    statusLight(155, 0, 0);   // red
  }
}

bool tryWifi(const char* name, const char* pass) {
  Serial.print(F("Trying WiFi: "));
  Serial.println(name);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 3) {
    WiFi.begin(name, pass);
    Serial.print(F("."));
    delay(3000);
    attempts++;
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

void reconnectMQTT() {
  statusLight(0, 0, 155);
  wifiClient.stop();
  Serial.println(F("Connecting to Vespera's server (MQTT)..."));
  if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
    Serial.println(F("Connected - squeeze away!"));
    statusLight(0, 155, 0);
  } else {
    Serial.print(F("Failed, code "));
    Serial.print(mqttClient.state());
    Serial.println(F(" - check the MQTT username and password. Retrying in 5 seconds."));
    statusLight(155, 0, 0);
    delay(5000);
  }
}

void statusLight(int r, int g, int b) {
  WiFiDrv::analogWrite(25, r);
  WiFiDrv::analogWrite(26, g);
  WiFiDrv::analogWrite(27, b);
}
