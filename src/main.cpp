#include <Arduino.h>
// #include <esptinyusb.h>
// #include "usb_host.hpp"
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"
#include "host/usbh.h"
#include "osal/osal.h"
#include "osal/osal_freertos.h"
// #include "osal/osal_rtthread.h"
}

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <esp_wifi.h>
#include <WebServer_ESP32_SC_W5500.h> //https://github.com/khoih-prog/ESP32_W5500_Manager

#include "class_driver.hpp"

#define TEST_DEV_ADDR 0
#define NUM_URBS 3
#define TRANSFER_MAX_BYTES 256
#define URB_DATA_BUFF_SIZE (sizeof(usb_setup_packet_t) + TRANSFER_MAX_BYTES) // 256 is worst case size for configuration descriptors
#define DAEMON_TASK_PRIORITY 2
#define CLASS_TASK_PRIORITY 3

extern void class_driver_task(void *arg);
static const char *TAG = "DAEMON";

void host_lib_daemon_task(void *arg)
{
    SemaphoreHandle_t signaling_sem = (SemaphoreHandle_t)arg;
    ESP_LOGI(TAG, "Installing USB Host Library");
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xSemaphoreGive(signaling_sem);
    
    vTaskDelay(10); // Short delay to let client task spin up

    bool has_clients = true;
    bool has_devices = true;
    while (has_clients || has_devices)
    {
        uint32_t event_flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            has_clients = false;
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
        {
            has_devices = false;
        }
    }
    ESP_LOGI(TAG, "No more clients and devices");
    ESP_ERROR_CHECK(usb_host_uninstall());

    xSemaphoreGive(signaling_sem);
    vTaskSuspend(NULL);
}
void setup()
{
    // byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xCA};
    // ETH.begin(MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST, mac);
    // ETH.config(IPAddress(10, 10, 241, 9), IPAddress(10, 10, 241, 254), IPAddress(255, 255, 255, 0), IPAddress(10, 13, 10, 13), IPAddress(8, 8, 8, 8));
    // ETH.setHostname(HOSTNAME);
   
    // while (!ESP32_W5500_eth_connected)
    //      delay(500);
    // log_v("connected. Local IP: ");
    
    // if (ESP32_W5500_isConnected())
    // {
    //     log_v("connected. Local IP: ");
    //     log_v("%s", ETH.localIP());
    // }

    SemaphoreHandle_t signaling_sem = xSemaphoreCreateBinary();
    
    TaskHandle_t daemon_task_hdl;
    TaskHandle_t class_driver_task_hdl;
    xTaskCreatePinnedToCore(host_lib_daemon_task,
                            "daemon",
                            4096,
                            (void *)signaling_sem,
                            DAEMON_TASK_PRIORITY,
                            &daemon_task_hdl,
                            0);
    xTaskCreatePinnedToCore(class_driver_task,
                            "class",
                            4096,
                            (void *)signaling_sem,
                            CLASS_TASK_PRIORITY,
                            &class_driver_task_hdl,
                            0);

    vTaskDelay(10); // Add a short delay to let the tasks run
    for (int i = 0; i < 2; i++)
        xSemaphoreTake(signaling_sem, portMAX_DELAY);

    vTaskDelete(class_driver_task_hdl);
    vTaskDelete(daemon_task_hdl);
}

void loop() {}