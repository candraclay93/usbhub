#ifndef __USBCDCCALLBACK_H__
#define __USBCDCCALLBACK_H__

#include <ArduinoJson.h>
#include <Arduino.h>
#include <cdcusb.h>
#include "./usbTask/usbData.h"

typedef struct cdcCallback
{
  bool dtr, rts;
  uint32_t bitRate;
  uint8_t len;
  String jsonResponse;
} cdcCallback_t;

cdcCallback_t cdcCallback;
extern control_t control;

CDCusb CDCUSBSerial;

StaticJsonDocument<48> usbScreenDoc;
  
class USBCDCCallback : public CDCCallbacks
{
  
  void onCodingChange(cdc_line_coding_t const *p_line_coding)
  {
    cdcCallback.bitRate = CDCUSBSerial.getBitrate();
    Serial.printf("\ncdc change bit rate : %d", cdcCallback.bitRate);
  }

  boolean onConnect(bool dtr, bool rts)
  {
    cdcCallback.dtr = dtr;
    cdcCallback.rts = rts;
    return true;
  }

  void onData()
  {
    cdcCallback.len = CDCUSBSerial.available();
    uint8_t buf[cdcCallback.len] = {};

    CDCUSBSerial.read(buf, cdcCallback.len);
    Serial.println("\ncallback cdc data : ");
    Serial.write(buf, cdcCallback.len);
    DeserializationError error = deserializeJson(usbScreenDoc, (char *)buf, cdcCallback.len);
    if (!error)
    {
      String data = usbScreenDoc["data"].as<String>();
      uint8_t value = usbScreenDoc["status"]["syslogin"].as<uint8_t>();
      Serial.printf("\ndata : %s, \nvalue : %d \n", data, value);
      if (value == 0)
        control.lock = 1;
    }
  }
};
#endif