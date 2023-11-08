#include "HIDCallback.h"

void HIDCallback::onData(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    Serial.printf("ID: %d, type: %d, size: %d\n", report_id, (int)report_type, bufsize);
    for (size_t i = 0; i < bufsize; i++)
    {
        Serial.printf("%d\n", buffer[i]);
    }
}