/**
 * Copyright (c) 2022 Nicolai Electronics
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * SPDX-License-Identifier: MIT
 */

#include "tusb.h"
#include "bsp/board.h"
#include "pico/unique_id.h"
#include "usb_descriptors.h"

// String descriptors

char const* string_desc_arr [] = {
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "Badge.team",                  // 1: Manufacturer
  "MCH2022 badge",               // 2: Product
  "ESP32 console",               // 3: CDC Interface
  "FPGA console",                // 4: CDC Interface
};

enum {
    STRING_DESC = 0,
    STRING_DESC_MANUFACTURER,
    STRING_DESC_PRODUCT,
    STRING_DESC_CDC_0,
    STRING_DESC_CDC_1,
    STRING_DESC_SERIAL // (Not in the string description array)
};

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    uint8_t chr_count;
    if (index == STRING_DESC) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else if (index == STRING_DESC_SERIAL) {
        pico_unique_board_id_t id;
        pico_get_unique_board_id(&id);
        const uint8_t *str = id.id;
        chr_count = 16;
        for (uint8_t len = 0; len < chr_count; ++len) {
            uint8_t c = str[len >> 1];
            c = ((c >> (((len & 1) ^ 1) << 2)) & 0x0F) + '0';
            if (c > '9') {
                c += 7;
            }
            _desc_str[1 + len] = c;
        }
    } else {
        // Convert ASCII string into UTF-16
        if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) {
            return NULL;
        }
        const char* str = string_desc_arr[index];
        // Cap at max char
        chr_count = strlen(str);
        if ( chr_count > 31 ) chr_count = 31;
        for(uint8_t i=0; i<chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

    return _desc_str;
}

// Device descriptors

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,   // Endpoint 0 packet size
    .idVendor           = 0x16D0,                   // MCH2022 badge vendor identifier
    .idProduct          = 0x0F9A,                   // MCH2022 badge product identifier
    .bcdDevice          = 0x0100,                   // Protocol version
    .iManufacturer      = STRING_DESC_MANUFACTURER, // Index of manufacturer name string
    .iProduct           = STRING_DESC_PRODUCT,      // Index of product name string
    .iSerialNumber      = STRING_DESC_SERIAL,       // Index of serial number string
    .bNumConfigurations = 0x01                      // Number of configurations supported
};

uint8_t const * tud_descriptor_device_cb(void) {
  return (uint8_t const *) &desc_device;
}

// Configuration Descriptor

enum {
  ITF_NUM_CDC_0,
  ITF_NUM_CDC_0_DATA,
  ITF_NUM_CDC_1,
  ITF_NUM_CDC_1_DATA,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + CFG_TUD_CDC * TUD_CDC_DESC_LEN)

#define EPNUM_CDC_0_NOTIF  0x81 // Endpoint 1: CDC serial port for ESP32 console, control
#define EPNUM_CDC_0_OUT    0x02 // Endpoint 2: CDC serial port for ESP32 console, data
#define EPNUM_CDC_0_IN     0x82

#define EPNUM_CDC_1_NOTIF  0x83 // Endpoint 3: CDC serial port for FPGA console, control
#define EPNUM_CDC_1_OUT    0x04 // Endpoint 4: CDC serial port for FPGA console, data
#define EPNUM_CDC_1_IN     0x84

uint8_t const desc_fs_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, STRING_DESC_CDC_0, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),

    // 2nd CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, STRING_DESC_CDC_1, EPNUM_CDC_1_NOTIF, 8, EPNUM_CDC_1_OUT, EPNUM_CDC_1_IN, 64)
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_fs_configuration;
}
