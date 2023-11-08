#include "WEBUSBCallback.h"

void WebUSBCallback::onConnect(bool state)
{
    Serial.printf("webusb is %s\n", state ? "connected" : "disconnected");
}