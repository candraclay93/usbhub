#ifndef MQTTTASK_H__
#define MQTTTASK_H__

#include "Arduino.h"
#include "ArduinoJson.h"
#include "AsyncMqtt_Generic.h"
#include "WiFi.h"
#include "data.h"

extern mqtt_t mqtt;
extern keystroke_t keystroke;
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}

#if ASYNC_TCP_SSL_ENABLED
#define MQTT_SECURE true
const uint8_t MQTT_SERVER_FINGERPRINT[] = {0xEE, 0xBC, 0x4B, 0xF8, 0x57, 0xE3, 0xD3, 0xE4, 0x07, 0x54, 0x23, 0x1E, 0xF0, 0xC8, 0xA1, 0x56, 0xE0, 0xD3, 0x1A, 0x1C};
#define MQTT_HOST "test.mosquitto.org"
#else
#define MQTT_HOST "test.mosquitto.org"
#endif

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

void connectToWifi()
{
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(DEF_SSID, DEF_PASS);
}

void connectToMqtt()
{
    Serial.println("Connecting to MQTT...");
    mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event)
{
    switch (event)
    {
#if USING_CORE_ESP32_CORE_V200_PLUS

    case ARDUINO_EVENT_WIFI_READY:
        Serial.println("WiFi ready");
        break;

    case ARDUINO_EVENT_WIFI_STA_START:
        Serial.println("WiFi STA starting");
        break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.println("WiFi STA connected");
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.println("WiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        connectToMqtt();
        break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        Serial.println("WiFi lost IP");
        break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");
        xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        xTimerStart(wifiReconnectTimer, 0);
        break;
#else

    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        connectToMqtt();
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");
        xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        xTimerStart(wifiReconnectTimer, 0);
        break;
#endif

    default:
        break;
    }
}

void onMqttConnect(bool sessionPresent)
{
    uint16_t packetIdSub = mqttClient.subscribe("test/lol", 0);
    Serial.printf("\nConnected to MQTT Session present : %d, \nSubscribing at QoS 2, packetId: %d", packetIdSub, sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    Serial.println("\nDisconnected from MQTT.");
    if (WiFi.isConnected())
        xTimerStart(mqttReconnectTimer, 0);
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
    Serial.printf("\nSubscribe acknowledged packetId : %d, qos : %d", packetId, qos);
}

void onMqttUnsubscribe(uint16_t packetId)
{
    Serial.printf("\nUnsubscribe acknowledge : %d", packetId);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    DeserializationError error = deserializeJson(mqtt.mqttJson, payload, len);
    if (!error)
    {
        keystroke.jsonData = mqtt.mqttJson["data"].as<String>();
        keystroke.jsonCommand = mqtt.mqttJson["command"].as<String>();
        keystroke.jsonValue = mqtt.mqttJson["value"];
        keystroke.exec = mqtt.mqttJson["exec"];
    }
}

void onMqttPublish(uint16_t packetId)
{
    Serial.printf("\nPublish acknowledged packetId : %d", packetId);
}

void mqttTask(void *param)
{
    while (!Serial && millis() < 5000);

    Serial.print("\nStarting FullyFeatureSSL_ESP32 on ");
    Serial.println(ARDUINO_BOARD);
    Serial.println(ASYNC_MQTT_GENERIC_VERSION);

    mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0,
                                      reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
    wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0,
                                      reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

    WiFi.onEvent(WiFiEvent);
    boolean isConnectedToMqtt = 0;
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onSubscribe(onMqttSubscribe);
    mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.onPublish(onMqttPublish);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCredentials("rw", "readwrite");
#if ASYNC_TCP_SSL_ENABLED
    mqttClient.setSecure(MQTT_SECURE);
    if (MQTT_SECURE)
        mqttClient.addServerFingerprint((const uint8_t *)MQTT_SERVER_FINGERPRINT);
#endif
    connectToWifi();
    
    long long start = millis();
    while (1)
    {
        if (millis() - start > MQTT_S)
        {
            // if (isConnectedToMqtt == 0 && mqtt.networkConnected == 1)
            // {
            //     isConnectedToMqtt = 1;
            //     mqttClient.connect();
            // }
            // if (isConnectedToMqtt == 1 && mqtt.networkConnected == 1)
                // Serial.print("\nsend to mqtt");
                start = millis();
        }
        vTaskDelay(100);
    }
}
#endif