#include "USBCallback.h"

void USBCallback::onMount()
{
    Serial.println("device mounted");
}
void USBCallback::onUnmount()
{
    Serial.println("device unmounted");
}
void USBCallback::onSuspend(bool remote_wakeup_en)
{
    Serial.println("device suspended");
}
void USBCallback::onResume(bool resume)
{
    Serial.println("device resumed");
}
