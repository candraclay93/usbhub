/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#define TUSB_OPT_HOST_ENABLED

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "tusb.h"
#include "usb/usb_host.h"
#include "host/usbh.h"
#include "host/hub.h"
#include "host/hcd.h"
#include "common/tusb_common.h"
#include "osal/osal_freertos.h"
#include "osal/osal.h"

#define CLIENT_NUM_EVENT_MSG 5
#define ACTION_CONF_DEV 0x00
#define ACTION_OPEN_DEV 0x01
#define ACTION_GET_DEV_INFO 0x02
#define ACTION_GET_DEV_DESC 0x04
#define ACTION_GET_CONFIG_DESC 0x08
#define ACTION_GET_STR_DESC 0x10
#define ACTION_CLOSE_DEV 0x20
#define ACTION_EXIT 0x40

static osal_mutex_def_t _usbh_mutexdef;
static osal_mutex_t _usbh_mutex;
const usb_device_desc_t *usbDesc;

usb_config_desc_t *usbConf;
tusb_desc_interface_t *usbInterface;
usb_ep_desc_t *usbEndpoint;
usb_iad_desc_t *usbIad;

typedef struct __attribute__((packed))
{
    uint8_t message;
    uint32_t paketLen;
    uint8_t slot;
    uint8_t sequence;
    uint8_t volTAG_CLASSeLevel;
    uint16_t reserved;
} mock_ccid_get_t;

typedef struct
{
    usb_host_client_handle_t client_hdl;
    uint8_t dev_addr;
    usb_device_handle_t dev_hdl;
    uint32_t actions;
} class_driver_t;

typedef struct
{
    uint8_t itf_num;
    uint8_t ep_in;
    uint8_t port_count;
    uint8_t status_change; // data from status change interrupt endpoint

    hub_port_status_response_t port_status;
} usbh_hub_t;
CFG_TUSB_MEM_SECTION static usbh_hub_t hub_data[CFG_TUSB_HOST_DEVICE_MAX];
TU_ATTR_ALIGNED(4)
CFG_TUSB_MEM_SECTION static uint8_t _hub_buffer[sizeof(descriptor_hub_desc_t)];
CFG_TUSB_MEM_SECTION usbh_device_t _usbh_devices[CFG_TUSB_HOST_DEVICE_MAX + 1];

typedef union
{
    struct
    {
        uint8_t mType;
        uint32_t pLen;
        uint8_t slot;
        uint8_t sequence;
        uint8_t voltL;
        uint16_t res;
    } __attribute__((packed));
    uint8_t val[10]; /**< Descriptor value */
} ccid_connect_t;
ESP_STATIC_ASSERT(sizeof(ccid_connect_t) == 10, "Size of usb_setup_packet_t incorrect");

// typedef struct mock_ccid_get_t *ccid_handle_t;
const usb_config_desc_t *config_desc_hub;
static const char *TAG_CLASS = "CLASS";

static void tfCb(usb_transfer_t *inData)
{
    unsigned char *dataCB = (unsigned char *)inData;
    for (uint8_t i = 0; i < 10; i++)
        printf(" 0x%x,", dataCB[i]);
}

static void bulkTf(class_driver_t *driver_obj)
{
    ESP_LOGI(TAG_CLASS, "setup connect");
    uint8_t interfaceNum = 1;
    unsigned char *buff;
    if (config_desc_hub == NULL)
        return;

    int offset = 0;
    uint16_t wTotalLength = config_desc_hub->wTotalLength;
    const usb_standard_desc_t *next_desc = (const usb_standard_desc_t *)config_desc_hub;

    do
    {
        switch (next_desc->bDescriptorType)
        {
        case USB_B_DESCRIPTOR_TYPE_CONFIGURATION:
        {
            usbConf = (usb_config_desc_t *)next_desc;
            ESP_LOGI("USB_DESC", "desc config type");
            break;
        }
        case USB_B_DESCRIPTOR_TYPE_INTERFACE:
        {
            usbInterface = (tusb_desc_interface_t *)next_desc;
            ESP_LOGI("USB_DESC", "desc interface type");
            break;
        }
        case USB_B_DESCRIPTOR_TYPE_ENDPOINT:
        {
            usbEndpoint = (usb_ep_desc_t *)next_desc;
            ESP_LOGI("USB_DESC", "desc endpoint type");
            break;
        }
        case USB_B_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION:
        {
            usbIad = (usb_iad_desc_t *)next_desc;
            ESP_LOGI("USB_DESC", "desc interface assoctiation type");
            break;
        }
        default:
            break;
        }
        next_desc = usb_parse_next_descriptor(next_desc, wTotalLength, &offset);
    } while (next_desc != NULL);

    usbh_device_t *dev = &_usbh_devices[driver_obj->dev_addr];
    const uint8_t rhport = dev->rhport;

    ESP_LOGI("EP", "Endpoint descriptor");
    hub_init();
    printf("\n rhport : (%d), dev addr : (%d), max packet : (%d), bInterface (%d)", rhport, driver_obj->dev_addr, usbDesc->bMaxPacketSize0, usbInterface->bInterfaceNumber);
    if (hub_open(rhport, driver_obj->dev_addr, usbInterface, usbDesc->bMaxPacketSize0))
        ESP_LOGI("EP", "hub open");
    if (hub_set_config(driver_obj->dev_addr, 0))
        ESP_LOGI("EP", "hub set config");
    ESP_LOGI("EP", "hub configured");
    
    }

static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    class_driver_t *driver_obj = (class_driver_t *)arg;
    switch (event_msg->event)
    {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
        if (driver_obj->dev_addr == 0)
        {
            driver_obj->dev_addr = event_msg->new_dev.address;
            driver_obj->actions |= ACTION_OPEN_DEV;
        }
        break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        if (driver_obj->dev_hdl != NULL)
        {
            driver_obj->actions = ACTION_CLOSE_DEV;
        }
        break;
    default:
        abort();
    }
}

static void action_open_dev(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_addr != 0);
    ESP_LOGI(TAG_CLASS, "Opening device at address %d", driver_obj->dev_addr);
    ESP_ERROR_CHECK(usb_host_device_open(driver_obj->client_hdl, driver_obj->dev_addr, &driver_obj->dev_hdl));
    // Get the device's information next
    driver_obj->actions &= ~ACTION_OPEN_DEV;
    driver_obj->actions |= ACTION_GET_DEV_INFO;
}

static void action_get_info(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_hdl != NULL);
    ESP_LOGI(TAG_CLASS, "Getting device information");
    usb_device_info_t dev_info;
    ESP_ERROR_CHECK(usb_host_device_info(driver_obj->dev_hdl, &dev_info));
    ESP_LOGI(TAG_CLASS, "\t%s speed", (dev_info.speed == USB_SPEED_LOW) ? "Low" : "Full");
    ESP_LOGI(TAG_CLASS, "\tbConfigurationValue %d", dev_info.bConfigurationValue);
    // Todo: Print string descriptors
    // Get the device descriptor next
    driver_obj->actions &= ~ACTION_GET_DEV_INFO;
    driver_obj->actions |= ACTION_GET_DEV_DESC;
}

static void action_get_dev_desc(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_hdl != NULL);
    ESP_LOGI(TAG_CLASS, "Getting device descriptor");
    const usb_device_desc_t *dev_desc;
    ESP_ERROR_CHECK(usb_host_get_device_descriptor(driver_obj->dev_hdl, &dev_desc));
    ESP_ERROR_CHECK(usb_host_get_device_descriptor(driver_obj->dev_hdl, &usbDesc));
    usb_print_device_descriptor(dev_desc);
    driver_obj->actions &= ~ACTION_GET_DEV_DESC;
    driver_obj->actions |= ACTION_GET_CONFIG_DESC;
}

static void action_get_config_desc(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_hdl != NULL);
    ESP_LOGI(TAG_CLASS, "Getting config descriptor");
    const usb_config_desc_t *config_desc;
    ESP_ERROR_CHECK(usb_host_get_active_config_descriptor(driver_obj->dev_hdl, &config_desc));
    ESP_ERROR_CHECK(usb_host_get_active_config_descriptor(driver_obj->dev_hdl, &config_desc_hub));
    usb_print_config_descriptor(config_desc, NULL);
    driver_obj->actions &= ~ACTION_GET_CONFIG_DESC;
    driver_obj->actions |= ACTION_GET_STR_DESC;
}

static void action_get_str_desc(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_hdl != NULL);
    usb_device_info_t dev_info;
    ESP_ERROR_CHECK(usb_host_device_info(driver_obj->dev_hdl, &dev_info));
    if (dev_info.str_desc_manufacturer)
    {
        ESP_LOGI(TAG_CLASS, "Getting Manufacturer string descriptor");
        usb_print_string_descriptor(dev_info.str_desc_manufacturer);
    }
    if (dev_info.str_desc_product)
    {
        ESP_LOGI(TAG_CLASS, "Getting Product string descriptor");
        usb_print_string_descriptor(dev_info.str_desc_product);
    }
    if (dev_info.str_desc_serial_num)
    {
        ESP_LOGI(TAG_CLASS, "Getting Serial Number string descriptor");
        usb_print_string_descriptor(dev_info.str_desc_serial_num);
    }
    driver_obj->actions &= ~ACTION_GET_STR_DESC;
    driver_obj->actions |= ACTION_CONF_DEV;
}

static void action_close_dev(class_driver_t *driver_obj)
{
    ESP_ERROR_CHECK(usb_host_device_close(driver_obj->client_hdl, driver_obj->dev_hdl));
    driver_obj->dev_hdl = NULL;
    driver_obj->dev_addr = 0;
    // We need to exit the event handler loop
    driver_obj->actions &= ~ACTION_CLOSE_DEV;
    driver_obj->actions |= ACTION_EXIT;
}

void class_driver_task(void *arg)
{
    SemaphoreHandle_t signaling_sem = (SemaphoreHandle_t)arg;
    class_driver_t driver_obj = {0};

    xSemaphoreTake(signaling_sem, portMAX_DELAY);

    ESP_LOGI(TAG_CLASS, "Registering Client");
    usb_host_client_config_t client_config = {
        .is_synchronous = false, // Synchronous clients currently not supported. Set this to false
        .max_num_event_msg = CLIENT_NUM_EVENT_MSG,
        .async = {
            .client_event_callback = client_event_cb,
            .callback_arg = (void *)&driver_obj,
        },
    };
    ESP_ERROR_CHECK(usb_host_client_register(&client_config, &driver_obj.client_hdl));

    while (1)
    {
        if (driver_obj.actions == 0)
        {
            usb_host_client_handle_events(driver_obj.client_hdl, portMAX_DELAY);
        }
        else
        {
            if (driver_obj.actions & ACTION_OPEN_DEV)
                action_open_dev(&driver_obj);
            if (driver_obj.actions & ACTION_GET_DEV_INFO)
                action_get_info(&driver_obj);
            if (driver_obj.actions & ACTION_GET_DEV_DESC)
                action_get_dev_desc(&driver_obj);
            if (driver_obj.actions & ACTION_GET_CONFIG_DESC)
                action_get_config_desc(&driver_obj);
            if (driver_obj.actions & ACTION_GET_STR_DESC)
                action_get_str_desc(&driver_obj);
            if (driver_obj.actions & ACTION_CLOSE_DEV)
                action_close_dev(&driver_obj);
            if (driver_obj.actions == ACTION_CONF_DEV)
            {
                // xSemaphoreGive(signaling_sem);
                bulkTf(&driver_obj);
            }
            if (driver_obj.actions & ACTION_EXIT)
                break;
        }
    }

    ESP_LOGI(TAG_CLASS, "Deregistering Client");
    ESP_ERROR_CHECK(usb_host_client_deregister(driver_obj.client_hdl));

    // Wait to be deleted
    xSemaphoreGive(signaling_sem);
    vTaskSuspend(NULL);
}
