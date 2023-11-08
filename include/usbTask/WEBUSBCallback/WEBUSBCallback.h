#ifndef __WEBUSBCALLBACK_H__
#define __WEBUSBCALLBACK_H__

#include <Arduino.h>
#include <webusb.h>

class WebUSBCallback : public WebUSBCallbacks
{
  void onConnect(bool state);
};

#endif