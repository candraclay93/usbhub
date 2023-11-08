#ifndef __HIDCALLBACK_H__
#define __HIDCALLBACK_H__

#include <Arduino.h>
#include <hidusb.h>

class HIDCallback : public HIDCallbacks
{
  void onData(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize);
};
#endif