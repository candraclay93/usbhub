#include <Arduino.h>
#include <ArduinoJson.h>

typedef struct mqtt{
    String command;
    String data;
    String topic;
    String server;
    boolean networkConnected = 0;
    StaticJsonDocument<1024> mqttJson;
}mqtt_t;

typedef struct keystroke{
    uint8_t exec;
    String jsonData;
    String jsonCommand;
    uint8_t jsonValue;
}keystroke_t;

typedef struct sensor{
    uint16_t sensorSuhu;
    uint16_t sensorPh;
    uint16_t sensorUltrasonic;
} sensor_t;

typedef struct wifi{
    String wifiSSID;
    String wifiPass;
    String checkServer[3];
} wifi_t;
