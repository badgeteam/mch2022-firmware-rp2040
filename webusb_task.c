#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "webusb_task.h"
#include "uart_task.h"
#include "hardware.h"
#include "i2c_peripheral.h"

uint16_t prev_webusb_status[CFG_TUD_VENDOR] = {0x0000};
uint16_t webusb_status[CFG_TUD_VENDOR] = {0x0000};
bool webusb_status_changed[CFG_TUD_VENDOR] = {false};

#define WEBUSB_STATUS_BIT_CONNECTED 0x0001

bool webusb_esp32_reset_requested = false;
bool webusb_esp32_reset_mode = false;

bool webusb_esp32_baudrate_override_requested = false;
uint32_t webusb_esp32_baudrate_override_value = 0;
bool webusb_fpga_baudrate_override_requested = false;
uint32_t webusb_fpga_baudrate_override_value = 0;

bool webusb_mode_change_requested = false;
uint8_t webusb_mode_change_target = 0;

void webusb_debug(char* message) {
    tud_vendor_n_write(0, message, strlen(message));
}

void webusb_task() {
    char message[128];
    if (webusb_esp32_reset_requested) {
        esp32_reset(webusb_esp32_reset_mode); // Value controls the mode: 1 for download mode, 0 for normal mode
        webusb_esp32_reset_requested = false;
    }

    if (webusb_esp32_baudrate_override_requested) {
        if (get_webusb_connected(0)) {
            webusb_set_uart_baudrate(0, true, webusb_esp32_baudrate_override_value); // Enable baudrate override
        }
        webusb_esp32_baudrate_override_requested = false;
    }
    if (webusb_fpga_baudrate_override_requested) {
        if (get_webusb_connected(1)) {
            webusb_set_uart_baudrate(1, true, webusb_fpga_baudrate_override_value); // Enable baudrate override
        }
        webusb_fpga_baudrate_override_requested = false;
    }

    if (webusb_mode_change_requested) {
        if (get_webusb_connected(0)) {
            i2c_set_webusb_mode(webusb_mode_change_target); // Set ESP32 WebUSB mode
        }
        webusb_mode_change_requested = false;
    }
    
    for (uint8_t idx = 0; idx < CFG_TUD_VENDOR; idx++) {
        // On status change
        if (webusb_status_changed[idx]) {
            if (!get_webusb_connected(idx)) {
                if (idx == 0) { // ESP32
                    webusb_set_uart_baudrate(0, false, 0); // Restore control over ESP32 baudrate to CDC
                    i2c_set_webusb_mode(0x00); // Disable ESP32 WebUSB mode
                } else if (idx == 1) { // FPGA
                    webusb_set_uart_baudrate(1, false, 0); // Restore control over FPGA baudrate to CDC
                }
            }
        }

        // Data transfer
        int available = tud_vendor_n_available(idx);
        if (available > 0) {
            uint8_t buffer[64];
            uint32_t length = tud_vendor_n_read(idx, buffer, sizeof(buffer));
            if (WEBUSB_IDX_ESP32) {
                uart_write_blocking(UART_ESP32, buffer, length);
            } else if (WEBUSB_IDX_FPGA) {
                uart_write_blocking(UART_FPGA, buffer, length);
            }
        }
    }
}

#define WEBUSB_LANDING_PAGE_URL "mch2022.bodge.team"

tusb_desc_webusb_url_t desc_url = {
  .bLength         = 3 + sizeof(WEBUSB_LANDING_PAGE_URL) - 1,
  .bDescriptorType = 3, // WEBUSB URL type
  .bScheme         = 1, // 0: http, 1: https
  .url             = WEBUSB_LANDING_PAGE_URL
};

bool get_webusb_connected(uint8_t idx) {
    return webusb_status[idx] & WEBUSB_STATUS_BIT_CONNECTED;
}

uint16_t get_webusb_status(uint8_t idx) {
    return webusb_status[idx];
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
    // nothing to with DATA & ACK stage
    if (stage != CONTROL_STAGE_SETUP) return true;

    switch (request->bmRequestType_bit.type) {
        case TUSB_REQ_TYPE_VENDOR:
            switch (request->bRequest) {
                case VENDOR_REQUEST_WEBUSB:
                    // match vendor request in BOS descriptor
                    // Get landing page url
                    return tud_control_xfer(rhport, request, (void*)(uintptr_t) &desc_url, desc_url.bLength);

                case VENDOR_REQUEST_MICROSOFT:
                    if ( request->wIndex == 7 ) {
                        // Get Microsoft OS 2.0 compatible descriptor
                        uint16_t total_len;
                        memcpy(&total_len, desc_ms_os_20+8, 2);
                        return tud_control_xfer(rhport, request, (void*)(uintptr_t) desc_ms_os_20, total_len);
                    } else {
                        return false;
                    }
                default:
                    break;
            }
            break;

        case TUSB_REQ_TYPE_CLASS: {
            if (request->bRequest == 0x22) { // Set status
                if (request->wIndex == ITF_NUM_VENDOR_0) {
                    webusb_status[0] = request->wValue;
                    webusb_status_changed[0] = true;
                }
                if (request->wIndex == ITF_NUM_VENDOR_1) {
                    webusb_status[1] = request->wValue;
                    webusb_status_changed[1] = true;
                }
                return tud_control_status(rhport, request);
            }
            if (request->bRequest == 0x23) { // Reset ESP32
                if (request->wIndex == ITF_NUM_VENDOR_0) {
                    webusb_esp32_reset_requested = true;
                    webusb_esp32_reset_mode = request->wValue;
                    return tud_control_status(rhport, request);
                }
            }
            if (request->bRequest == 0x24) { // Set baudrate
                if (request->wIndex == ITF_NUM_VENDOR_0) {
                    webusb_esp32_baudrate_override_requested = true;
                    webusb_esp32_baudrate_override_value = (request->wValue) * 100;
                }
                if (request->wIndex == ITF_NUM_VENDOR_1) {
                    webusb_fpga_baudrate_override_requested = true;
                    webusb_fpga_baudrate_override_value = (request->wValue) * 100;
                }
                return tud_control_status(rhport, request);
            }
            if (request->bRequest == 0x25) { // Set mode
                if (request->wIndex == ITF_NUM_VENDOR_0) {
                    webusb_mode_change_requested = true;
                    webusb_mode_change_target = request->wValue & 0xFF;
                }
                return tud_control_status(rhport, request);
            }
            break;
        }
        default:
            break;
    }
    return false; // stall unknown request
}

bool tud_vendor_control_complete_cb(uint8_t rhport, tusb_control_request_t const * request) {
  (void) rhport;
  (void) request;
  return true;
}
