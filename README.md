## Introduction

This sketch implements in a stable way Wi-Fi management and configuration for MQTT in captive portal, in addition to updating by OTA.


## Requirements

 [**AsyncMqttClient**](https://github.com/marvinroger/async-mqtt-client.git)

[**AsyncElegantOTA**](https://github.com/ayushsharma82/AsyncElegantOTA.git)

[**ESPAsyncWebServer**](https://github.com/me-no-dev/ESPAsyncWebServer.git)

[**ESPAsyncWiFiManager**](https://github.com/alanswx/ESPAsyncWiFiManager.git)

[**ArduinoJson**](https://github.com/bblanchon/ArduinoJson.git)


## how to use

- At the first boot the microcontroller starts up in AP mode. By default configure the ip 192.168.4.1 and a captive portal to configure IP and mqtt server.

- Once configured, try to connect and if it is successful, save the new configuration in the SPIFFS.

  ```
  {
   "IDname": (ID / Name),
   "mqtt_server":(IP/Host MQTT Server),
   "mqtt_port":(Port MQTT Server),
   "mqtt_user":(User MQTT Server),
   "mqtt_psw":(Password MQTT Server)
  }
  ```


- When it connects to the mqtt server, it sends a **ready** payload and the current state of the configured pinout (default 5).

    ```
    {
     topic : "wemos/state",
     payload: "ready"
    }
    ```

- To toggle the pinout state send payload **ON** to activate and **OFF** to deactivate the relay.

    ```
    {
     topic : "wemos/set/state",
     payload: "ON"
    }
    ```

- There are three commands available:

    *Reboot the device*
    ```
     {
      topic : "wemos/command",
      payload: "reboot"
     }
    ```
 
    *Reset the device to default*
    ```
     {
      topic : "wemos/command",
      payload: "reset"
     }
    ```

    *Starts the update mode via OTA (Over The Air)*
    ```
     {
      topic : "wemos/command",
      payload: "OTA"
     }
    ```
    `Access the established IP from your favorite browser http://<IPAddress>/update`
 
 - The device sends a pinout status payload every 5 minutes to verify the status and correct operation.


## Task lists

- [ ] Option to change the port in the captive portal (currently not working).
- [x] Option to change the ClientID or IPAddress in captive portal.
- [ ] Option to change the pinout.
- [ ] Send the configuration set through the microcontroller's mqtt payload, such as the IP address or data of interest.
- [ ] Send the configuration file with IP in AP mode, WiFi passwords, etc...

## Thanks

Thanks for the great work done to the authors of the bookstores, since without them this would not be possible.
> **@marvinroger** for AsyncMqttClient

> **@ayushsharma82** for AsyncElegantOTA

> **@me-no-dev** for ESPAsyncWebServer

> **@alanswx** for ESPAsyncWiFiManager

> **@bblanchon** for ArduinoJson
