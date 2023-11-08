#include "CDCDevice.h"

void CDCDevice::onMount() { Serial.println("Mount"); }
void CDCDevice::onUnmount() {Serial.println("Unmount");} // 
void CDCDevice::onSuspend(bool remote_wakeup_en) { Serial.println("Suspend"); }
void CDCDevice::onResume() { Serial.println("Resume"); }
