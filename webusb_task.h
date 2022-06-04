#pragma once

#define WEBUSB_IDX_CONTROL 0
#define WEBUSB_IDX_ESP32 1
#define WEBUSB_IDX_FPGA 2

void webusb_task(void);
bool get_webusb_connected();
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request);
bool tud_vendor_control_complete_cb(uint8_t rhport, tusb_control_request_t const * request);
