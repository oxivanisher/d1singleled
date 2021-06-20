#include <PubSubClient.h>

#include "ESP8266WiFi.h"

// Read settingd from config.h
#include "config.h"

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient espClient;
// or... use WiFiFlientSecure for SSL
// WiFiClientSecure espClient;

// Initialize MQTT
PubSubClient mqttClient(espClient);
// used for splitting arguments to effects
String s = String();

// Variable to store Wifi retries (required to catch some problems when i.e. the
// wifi ap mac address changes)
uint8_t wifiConnectionRetries = 0;

// Logic switches and temporary vars
bool readyToUpload = false;
bool initialPublish = false;
// unsigned long lastBlinkStart = 0;
// unsigned long blinkDelay = 0;
// uint8_t blinkAmount = 0;

bool mqttReconnect() {
  // Create a client ID based on the MAC address
  String clientId = String("D1SINGLELED") + "-";
  clientId += String(WiFi.macAddress());

  // Loop 5 times or until we're reconnected
  int counter = 0;
  while (!mqttClient.connected()) {
    counter++;
    if (counter > 5) {
      DEBUG_PRINTLN("Exiting MQTT reconnect loop");
      return false;
    }

    DEBUG_PRINT("Attempting MQTT connection...");

    // Attempt to connect
    String clientMac = WiFi.macAddress();
    char lastWillTopic[39] = "/d1singleled/lastwill/";
    strcat(lastWillTopic, clientMac.c_str());
    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD, lastWillTopic, 1, 1, clientMac.c_str())) {
      DEBUG_PRINTLN("connected");

      // clearing last will message
      mqttClient.publish(lastWillTopic, "", true);

      // subscribe to "all" topic
      mqttClient.subscribe("/d1singleled/all", 1);

      // subscript to the mac address (private) topic
      char topic[30];
      strcat(topic, "/d1singleled/");
      strcat(topic, clientMac.c_str());
      mqttClient.subscribe(topic, 1);

      return true;
    } else {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINT(mqttClient.state());
      DEBUG_PRINTLN(" try again in 2 seconds");
      // Wait 1 second before retrying
      delay(1000);
    }
  }
  return false;
}

// connect to wifi
bool wifiConnect() {
  // testing http://blog.flynnmetrics.com/uncategorized/esp8266-exception-3/
  WiFi.persistent(false);
  WiFi.disconnect(true);

  wifiConnectionRetries += 1;
  int retryCounter = CONNECT_TIMEOUT * 1000;
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.mode(WIFI_STA);  //  Force the ESP into client-only mode
  delay(1);
  DEBUG_PRINT("My Mac: ");
  DEBUG_PRINTLN(WiFi.macAddress());
  DEBUG_PRINT("Reconnecting to Wifi ");
  DEBUG_PRINT(wifiConnectionRetries);
  DEBUG_PRINT("/20 ");
  while (WiFi.status() != WL_CONNECTED) {
    retryCounter--;
    if (retryCounter <= 0) {
      DEBUG_PRINTLN(" timeout reached!");
      if (wifiConnectionRetries > 19) {
        DEBUG_PRINTLN(
            "Wifi connection not sucessful after 20 tries. Resetting ESP8266!");
        ESP.restart();
      }
      return false;
    }
    delay(1);
    if (retryCounter % 500 == 0) {
      DEBUG_PRINT(".");
    }
  }
  DEBUG_PRINT(" done, got IP: ");
  DEBUG_PRINTLN(WiFi.localIP().toString());
  wifiConnectionRetries = 0;
  return true;
}

// helper functions
// Thanks to https://gist.github.com/mattfelsen/9467420
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length();

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// logic
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  unsigned int numOfOptions = 0;
  DEBUG_PRINT("Message arrived: Topic [");
  DEBUG_PRINT(topic);
  DEBUG_PRINT("] | Data [");
  for (unsigned int i = 0; i < length; i++) {
    DEBUG_PRINT((char)payload[i]);
    if ((char)payload[i] == ';') {
      numOfOptions++;
    }
  }
  DEBUG_PRINT("] - Found ");
  DEBUG_PRINT(numOfOptions);
  DEBUG_PRINTLN(" options.");

  s = String((char*)payload);

  // Just keep one as an example. I.E. request the cfg values
  if (s == "on") {
    DEBUG_PRINTLN("MQTT Requests enable the LED");
    digitalWrite(LED_PIN, HIGH);
  } else if (s == "off") {
    DEBUG_PRINTLN("MQTT Requests disable the LED");
    digitalWrite(LED_PIN, LOW);
  } else {
    DEBUG_PRINTLN("Unknown request");
  }
}

void setup() {
#ifdef DEBUG
  Serial.begin(SERIAL_BAUD);  // initialize serial connection
  // delay for the serial monitor to start
  delay(3000);
#endif

  // Start the Pub/Sub client
  mqttClient.setServer(MQTT_SERVER, MQTT_SERVERPORT);
  mqttClient.setCallback(mqttCallback);

  // initial delay to let millis not be 0
  delay(1);

  // initial wifi connect
  wifiConnect();

  // set button pin
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  // Check if the wifi is connected
  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("Calling wifiConnect() as it seems to be required");
    wifiConnect();
    DEBUG_PRINTLN("My MAC: " + String(WiFi.macAddress()));
  }

  if ((WiFi.status() == WL_CONNECTED) && (!mqttClient.connected())) {
    delay(500);

    DEBUG_PRINTLN("MQTT is not connected, let's try to reconnect");
    if (!mqttReconnect()) {
      // This should not happen, but seems to...
      DEBUG_PRINTLN("MQTT was unable to connect! Exiting the upload loop");
      delay(500);
      // force reconnect to mqtt
      initialPublish = false;
    } else {
      // readyToUpload = true;
      DEBUG_PRINTLN("MQTT successfully reconnected");
    }
  }

  if ((WiFi.status() == WL_CONNECTED) && (!initialPublish)) {
    DEBUG_PRINT("MQTT discovery publish loop:");

    String clientMac = WiFi.macAddress();  // 17 chars
    char topic[40] = "/d1singleled/discovery/";
    strcat(topic, clientMac.c_str());

    if (mqttClient.publish(topic, VERSION, true)) {
      // Publishing values successful, removing them from cache
      DEBUG_PRINTLN(" successful");

      initialPublish = true;
    } else {
      DEBUG_PRINTLN(" FAILED!");
    }
  }

  // implement the blinking stuff here. use those vars:
  // unsigned long lastBlinkStart = 0;
  // unsigned long blinkDelay = 0;
  // uint8_t blinkAmount = 0;

  // calm down, boy
  delay(10);

  // calling loop at the end as proposed
  mqttClient.loop();
}
