#include "sensorTask.h"

void sensor(void *param)
{
  long long start = millis();
  while (1)
  {
    if (start - millis() > SENSOR_S)
    {
      Serial.print("get sensor");
      start = millis();
    }
    vTaskDelay(1);
  }
}
