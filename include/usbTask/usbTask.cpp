#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#include "data.h"
#include "webusb.h"
#include "cdcusb.h"
#include "mscusb.h"
#include "dfuusb.h"
#include "flashdisk.h"
#include "hidkeyboard.h"
#include "ArduinoJson.h"

#include "./usbTask/HIDCallback/HIDCallback.h"
#include "./usbTask/CDCDevice/CDCDevice.h"
#include "./usbTask/USBCallback/USBCallback.h"
#include "./usbTask/USBCDCCallback/USBCDCCallback.hpp"
#include "./usbTask/WEBUSBCallback/WEBUSBCallback.h"

WebUSB WebUSBSerial;
HIDkeyboard keyboard;
FlashUSB fat1;
FlashUSB fat2;
DFUusb dfu;

extern control_t control;
extern keystroke_t keystroke;

void echo_all(char c)
{
    WebUSBSerial.write(c);
    CDCUSBSerial.write(c);
    Serial.write(c);
}

void keyboardTask(void *param)
{
    unsigned long long logginTime = millis();
    while (1)
    {
        if (keystroke.jsonValue != 0)
        {
            Serial.println(keyboard.sendString(String(keystroke.jsonData) + "\n") ? "\ndata ok" : "\ndata fail");
            Serial.println(keyboard.sendString(String(keystroke.jsonCommand) + "\n") ? "\ncommand ok" : "\ncommand fail");
            Serial.println("\n value : " + String(keystroke.jsonValue));
            keystroke.jsonValue = 0;
        }
        if (millis() - logginTime > LOGIN_WAITING && control.lock == 1)
        {
            control.lock = 2;
            Serial.println(keyboard.sendString(String("pi\n")) ? "OK" : "FAIL");
            delay(1000);
            Serial.println(keyboard.sendString(String("aba1234567\n")) ? "OK" : "FAIL");
            delay(1000);
            logginTime = millis();
        }

        delay(1);
    }
}

void usb(void *param)
{
    if (fat1.init("/fat1", "ffat"))
    {
        if (fat1.begin())
            Serial.println("MSC lun 1 begin");
        else
            log_e("LUN 1 failed");
    }

    keyboard.setBaseEP(3);
    keyboard.begin();
    keyboard.setCallbacks(new HIDCallback());
    WebUSBSerial.setBaseEP(5);

    if (!WebUSBSerial.begin())
        Serial.println("Failed to start webUSB stack");
    if (!CDCUSBSerial.begin())
        Serial.println("Failed to start CDC USB stack");

    WebUSBSerial.setCallbacks(new WebUSBCallback());
    CDCUSBSerial.setCallbacks(new USBCDCCallback());
    dfu.begin();

    EspTinyUSB::registerDeviceCallbacks(new USBCallback());
    xTaskCreate(keyboardTask, "kTask", 3 * 1024, NULL, 5, NULL);
    while (1)
    {
        while (WebUSBSerial.available())
            echo_all(WebUSBSerial.read());
        while (CDCUSBSerial.available())
            echo_all(CDCUSBSerial.read());
        while (Serial.available())
            echo_all(Serial.read());
        vTaskDelay(1);
    }
}