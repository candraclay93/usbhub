#include "wifiTask.h"
#include "esp_log.h"
#include "data.h"
#include <ESP32Ping.h>

extern mqtt_t mqtt;

void wifi(void *param)
{
  long long start = millis();
  
  WiFi.mode(WIFI_STA);
  WiFi.begin((const char*)DEF_SSID, (const char*)DEF_PASS);
  while (WiFi.status() != WL_CONNECTED)
    delay(1000);
  Serial.print("\nconnected\n");
  
  while (1)
  {
    if (millis() - start > WIFI_S)
    {
      if (Ping.ping("test.mosquitto.org"))
      {
        Serial.print("\nping Success !!");
        if(mqtt.networkConnected == 0)
          mqtt.networkConnected = 1;
      }
      else
      {
        Serial.print("\nping Fail !!");
      }
      start = millis();
    }
    vTaskDelay(1);
  }
}