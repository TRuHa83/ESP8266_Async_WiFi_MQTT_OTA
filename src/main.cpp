#include <Arduino.h>
#include "LittleFS.h"

#include <ESP8266WiFi.h>

#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <AsyncElegantOTA.h>

#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>

#include <ArduinoJson.h>

#define MQTT_PORT 1883

AsyncWebServer server(80);
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
DNSServer dns;

char mqtt_server[40];
char mqtt_port[6]; 
char mqtt_user[40];
char mqtt_psw[40];

const char* state = "wemos/state";
const char* command = "wemos/command";

const String toggle = "toggle";
const String reset = "reset";
const String reboot = "reboot";
const String OTA = "OTA";

bool shouldSaveConfig = false;
bool otaserver = false;

void reset_config(){
    Serial.println("Reset config WiFi.");
    //wifiManager.resetSettings();
    ESP.restart();
  }

void OTA_server(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP32Cam.");
  });

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP OTA server started");

  otaserver = true;
}

void connectToMqtt() {
  Serial.print("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("done.");
  Serial.println("Wemos ready.");

  mqttClient.publish( state, 2, true, "ready" );
  mqttClient.subscribe( command, 2 );
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  char fixedPayload[len + 1];
  for (int i = 0; i <= len; i++) {
    if (i == len) {
      fixedPayload[i] = '\0';
      continue;
    }
    fixedPayload[i] = payload[i];
  }

  String value = String(fixedPayload);

  if (value == toggle){
    mqttClient.publish( state, 2, true, "toggle" );
  }
  else if (value == reset){
    mqttClient.publish( state, 2, true, "reset" );
    reset_config();
  }
  else if (value == reboot){
    Serial.println("rebooting...");
    mqttClient.publish( state, 2, true, "reboot" );
    ESP.restart();
  }
  else if (value == OTA){
    mqttClient.publish( state, 2, true, "OTA" );
    OTA_server();
  }
}

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
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
        //json.printTo(Serial);
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
    LittleFS.format();
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
    Serial.println();
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }

  //wifiManager.setConnectTimeout(300);
  //wifiManager.startConfigPortal("AutoConnectAP", "password");

  mqttClient.setCredentials( mqtt_user, mqtt_psw );
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(mqtt_server, MQTT_PORT);

  connectToMqtt();
}

void loop() {
  if ( otaserver ){
    AsyncElegantOTA.loop();
  }
}