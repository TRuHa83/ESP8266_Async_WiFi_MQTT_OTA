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
DNSServer dns;

AsyncWiFiManager wifiManager(&server,&dns);

Ticker mqttReconnectTimer;
Ticker alive;

char IDname[20] = "wemos";  // variable to store ID name
char mqtt_server[40];       // variable to store the mqtt server
char mqtt_port[6];          // variable to store the mqtt port
char mqtt_user[40];         // variable to store the mqtt user
char mqtt_psw[40];          // variable to store the mqtt psw

// Topics
const char* state = "/state";
const char* command = "/command";
const char* setState = "/set/state";

char topicState[40];
char topicCommnad[60];
char topicSetState[80];


// Commands
const String reset = "reset";
const String reboot = "reboot";
const String OTA = "OTA";

const int relay = 5; // Digital pinout Relay
char* statusRelay;   // variable to store state Relay
 

bool shouldSaveConfig = false;
bool otaserver = false;

void sendAlive(){
  if (digitalRead(relay) == 0){
    statusRelay = "OFF";
  }
  else{
    statusRelay = "ON";
  }

  mqttClient.publish( topicState, 2, true, statusRelay );
}

void changeState(String value){
  if (value == "ON" || value == "1"){
    digitalWrite(relay, HIGH);
    mqttClient.publish( topicState, 2, true, "ON" );
  }
  else if (value == "OFF" || value == "0"){
    digitalWrite(relay, LOW);
    mqttClient.publish( topicState, 2, true, "OFF" );
  }

  Serial.print("Change state to: ");
  Serial.println(value);
}

void reset_config(){
    Serial.println("Reset config WiFi.");
    wifiManager.resetSettings();
  }

void OTA_server(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am ESP8266.");
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
  Serial.println(" done.");

  Serial.print(IDname);
  Serial.println(" ready.");

  mqttClient.publish( topicState, 2, true, "ready" );
  mqttClient.subscribe( topicCommnad, 2 );
  mqttClient.subscribe( topicSetState, 2 );

  alive.attach(300, sendAlive);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }

  alive.detach();
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

  String valueT = String(topic);
  String valueP = String(fixedPayload);

  if (valueT == topicSetState){
    changeState(valueP);
  }
  else {
    if (valueP == reset){
      mqttClient.publish( topicState, 2, true, "reset" );
      reset_config();
    }
    else if (valueP == reboot){
      Serial.println("rebooting...");
      mqttClient.publish( topicState, 2, true, "rebooting..." );
      delay(5000);
      ESP.restart();
    }
    else if (valueP == OTA){
      mqttClient.publish( topicState, 2, true, "OTA" );
      OTA_server();
    }
  }
}

void makeTopics(){
  strcpy(topicState, IDname);
  strcat(topicState, state);

  strcpy(topicCommnad, IDname);
  strcat(topicCommnad, command);

  strcpy(topicSetState, IDname);
  strcat(topicSetState, setState);
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
        //json.printTo(Serial); // show configuration file
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(IDname, json["IDname"]);
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_psw, json["mqtt_psw"]);

        } else {
          Serial.println("failed to load json config");
          LittleFS.remove("/config.json");

          wifiManager.resetSettings();
          Serial.println("Rebooting as default config");

          delay(5000);
          ESP.restart();
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

  // Setup Pins
  pinMode(relay, OUTPUT);

  load_config();

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // id/name, placeholder/prompt, default, length
  AsyncWiFiManagerParameter custom_name("name", "ID / Name", IDname, 40);
  AsyncWiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  AsyncWiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  AsyncWiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 40);
  AsyncWiFiManagerParameter custom_mqtt_psw("psw", "mqtt password", mqtt_psw, 40);

  wifiManager.addParameter(&custom_name);
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

  strcpy(IDname, custom_name.getValue());
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

    json["IDname"] = IDname;
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"]   = mqtt_port;
    json["mqtt_user"]   = mqtt_user;
    json["mqtt_psw"]   = mqtt_psw;

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    //json.prettyPrintTo(Serial);
    //json.printTo(configFile);
    //Serial.println();

    configFile.close();
    //end save
    shouldSaveConfig = false;
  }

  makeTopics();

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