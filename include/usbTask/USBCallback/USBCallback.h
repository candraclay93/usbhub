#ifndef __USBCALLBACK_H__
#define __USBCALLBACK_H__

#include <Arduino.h>
#include "esptinyusb.h"

class USBCallback : public USBCallbacks
{
  void onMount();
  void onUnmount();
  void onSuspend(bool remote_wakeup_en);
  void onResume(bool resume);
};

#endif