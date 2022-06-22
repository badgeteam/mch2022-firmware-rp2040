#include "webusb_task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/board.h"
#include "hardware.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "i2c_peripheral.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "uart_task.h"
#include "usb_descriptors.h"

uint16_t webusb_status[CFG_TUD_VENDOR]         = {0x0000};
bool     webusb_status_changed[CFG_TUD_VENDOR] = {false};

uint16_t webusb_esp32_status                      = 0x0000;
bool     webusb_esp32_reset_requested             = false;
bool     webusb_esp32_reset_mode                  = false;
bool     webusb_esp32_baudrate_override_requested = false;
uint32_t webusb_esp32_baudrate_override_value     = 0;
bool     webusb_esp32_mode_change_requested       = false;
uint8_t  webusb_esp32_mode_change_target          = 0;

uint16_t webusb_fpga_status                      = 0x0000;
bool     webusb_fpga_baudrate_override_requested = false;
uint32_t webusb_fpga_baudrate_override_value     = 0;

void webusb_task() {
    if (webusb_esp32_reset_requested) {
        esp32_reset(webusb_esp32_reset_mode);  // Value controls the mode: 1 for download mode, 0 for normal mode
        webusb_esp32_reset_requested = false;
    }

    if (webusb_esp32_baudrate_override_requested) {  // Set baudrate of ESP32 UART in WebUSB mode
        webusb_set_uart_baudrate(0, webusb_esp32_baudrate_override_value);
        webusb_esp32_baudrate_override_requested = false;
    }
    if (webusb_fpga_baudrate_override_requested) {  // Set baudrate of FPGA UART in WebUSB mode
        webusb_set_uart_baudrate(1, webusb_fpga_baudrate_override_value);
        webusb_fpga_baudrate_override_requested = false;
    }

    if (webusb_esp32_mode_change_requested) {
        i2c_set_webusb_mode(webusb_esp32_mode_change_target);  // Set ESP32 WebUSB mode
        webusb_esp32_mode_change_requested = false;
    }

    // Data transfer
    for (uint8_t idx = 0; idx < CFG_TUD_VENDOR; idx++) {
        int available = tud_vendor_n_available(idx);
        if (available > 0) {
            uint8_t  buffer[256];
            uint32_t length = tud_vendor_n_read(idx, buffer, sizeof(buffer));
            if (idx == WEBUSB_IDX_ESP32) {
                uart_write_blocking(UART_ESP32, buffer, length);
            } else if (idx == WEBUSB_IDX_FPGA) {
                uart_write_blocking(UART_FPGA, buffer, length);
            }
        }
    }
}

#define WEBUSB_LANDING_PAGE_URL "mch2022.bodge.team"

tusb_desc_webusb_url_t desc_url = {.bLength         = 3 + sizeof(WEBUSB_LANDING_PAGE_URL) - 1,
                                   .bDescriptorType = 3,  // WEBUSB URL type
                                   .bScheme         = 1,  // 0: http, 1: https
                                   .url             = WEBUSB_LANDING_PAGE_URL};

bool get_webusb_connected(uint8_t idx) {
    if (idx == WEBUSB_IDX_ESP32) return webusb_esp32_status & 0x0001;
    if (idx == WEBUSB_IDX_FPGA) return webusb_fpga_status & 0x0001;
    return false;
}

uint16_t get_webusb_status(uint8_t idx) {
    if (idx == WEBUSB_IDX_ESP32) return webusb_esp32_status;
    if (idx == WEBUSB_IDX_FPGA) return webusb_fpga_status;
    return 0x0000;
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    if (stage != CONTROL_STAGE_SETUP) return true;  // nothing to with DATA & ACK stage

    switch (request->bmRequestType_bit.type) {
        case TUSB_REQ_TYPE_VENDOR:
            switch (request->bRequest) {
                case VENDOR_REQUEST_WEBUSB:
                    // match vendor request in BOS descriptor
                    // Get landing page url
                    return tud_control_xfer(rhport, request, (void*) (uintptr_t) &desc_url, desc_url.bLength);

                case VENDOR_REQUEST_MICROSOFT:
                    if (request->wIndex == 7) {
                        // Get Microsoft OS 2.0 compatible descriptor
                        uint16_t total_len;
                        memcpy(&total_len, desc_ms_os_20 + 8, 2);
                        return tud_control_xfer(rhport, request, (void*) (uintptr_t) desc_ms_os_20, total_len);
                    } else {
                        return false;
                    }
                default:
                    break;
            }
            break;

        case TUSB_REQ_TYPE_CLASS:
            {
                if (request->bRequest == 0x22) {  // Set status
                    if (request->wIndex == ITF_NUM_VENDOR_0) {
                        webusb_esp32_status = request->wValue;
                        return tud_control_status(rhport, request);
                    } else if (request->wIndex == ITF_NUM_VENDOR_1) {
                        webusb_fpga_status = request->wValue;
                        return tud_control_status(rhport, request);
                    }
                }
                if (request->bRequest == 0x23) {  // Reset ESP32
                    if (request->wIndex == ITF_NUM_VENDOR_0) {
                        webusb_esp32_reset_requested = true;
                        webusb_esp32_reset_mode      = request->wValue;
                        return tud_control_status(rhport, request);
                    }
                }
                if (request->bRequest == 0x24) {  // Set baudrate
                    if (request->wIndex == ITF_NUM_VENDOR_0) {
                        webusb_esp32_baudrate_override_requested = true;
                        webusb_esp32_baudrate_override_value     = (request->wValue) * 100;
                        return tud_control_status(rhport, request);
                    } else if (request->wIndex == ITF_NUM_VENDOR_1) {
                        webusb_fpga_baudrate_override_requested = true;
                        webusb_fpga_baudrate_override_value     = (request->wValue) * 100;
                        return tud_control_status(rhport, request);
                    }
                }
                if (request->bRequest == 0x25) {  // Set mode
                    if (request->wIndex == ITF_NUM_VENDOR_0) {
                        webusb_esp32_mode_change_requested = true;
                        webusb_esp32_mode_change_target    = request->wValue & 0xFF;
                        return tud_control_status(rhport, request);
                    }
                }
                break;
            }
        default:
            break;
    }
    return false;  // stall unknown request
}

bool tud_vendor_control_complete_cb(uint8_t rhport, tusb_control_request_t const* request) {
    (void) rhport;
    (void) request;
    return true;
}
