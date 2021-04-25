#include <Arduino.h>
#include "LittleFS.h"

#include <ESP8266WiFi.h>

#include <Ticker.h>
#include <AsyncMqttClient.h>

#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>

#include <ArduinoJson.h>

#define MQTT_HOST IPAddress(10, 0, 10, 51)
#define MQTT_PORT 1883
#define USERNAME "mqttserver"
#define PASSWORD "mqttserver"
#define TOPIC_SUB "letrero/command"
#define TOPIC_PUB "letrero/state"

AsyncWebServer server(80);
DNSServer dns;

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

char mqtt_server[40] = "10.0.10.51";
char mqtt_port[6] = "1883"; 
char mqtt_user[40] = "mqtt";
char mqtt_psw[40] = "mqtt";

const String command = "letrero/command";
const String toggle = "toggle";
const String reset = "reset";
const String reboot = "reboot";
const String OTA = "OTA";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  Serial.print("IP: ");
  Serial.println(mqtt_server);
  Serial.print("Port: ");
  Serial.println(mqtt_port);
  Serial.print("User: ");
  Serial.println(mqtt_user);
  Serial.print("Psw: ");
  Serial.println(mqtt_psw);

  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Letrero ready.");

  mqttClient.publish( TOPIC_PUB, 0, false, "ready" );
  mqttClient.subscribe(TOPIC_SUB, 2);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.print("topic: ");
  Serial.println(topic);
  Serial.print("payload: ");
  Serial.println(payload);
}

void load_config(){
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (LittleFS.begin()) {
    Serial.println("mounted file system");
    if (LittleFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_psw, json["mqtt_psw"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    } else {
      //file no exists
      Serial.println("file no exists");
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
}

void setup() {
  Serial.begin(115200);

  load_config();

  AsyncWiFiManager wifiManager(&server,&dns);
  //wifiManager.resetSettings();

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // id/name, placeholder/prompt, default, length
  AsyncWiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  AsyncWiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  AsyncWiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 40);
  AsyncWiFiManagerParameter custom_mqtt_psw("psw", "mqtt password", mqtt_psw, 40);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_psw);

  int  port;
  port = atoi(mqtt_port);

  mqttClient.setCredentials( mqtt_user, mqtt_psw );
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(mqtt_server, port);

  //wifiManager.autoConnect();
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_psw, custom_mqtt_psw.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"]   = mqtt_port;
    json["mqtt_user"]   = mqtt_user;
    json["mqtt_psw"]   = mqtt_psw;

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }

  //wifiManager.setConnectTimeout(300);
  //wifiManager.startConfigPortal("AutoConnectAP", "password");

  connectToMqtt();
}

void loop() {
  // put your main code here, to run repeatedly:
}