#ifndef __CDCDEVICE_H__
#define __CDCDEVICE_H__

#include <Arduino.h>
#include <esptinyusb.h>

class CDCDevice : public USBCallbacks
{
  void onMount();
  void onUnmount();
  void onSuspend(bool remote_wakeup_en);
  void onResume();
};

#endif