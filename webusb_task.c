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
        snprintf(message, sizeof(message), "ESP32 reset to %s mode requested\r\n", webusb_esp32_reset_mode ? "download" : "app");
        webusb_debug(message);
        webusb_esp32_reset_requested = false;
    }

    if (webusb_esp32_baudrate_override_requested) {
        if (get_webusb_connected(1)) {
            uint32_t result = webusb_set_uart_baudrate(0, true, webusb_esp32_baudrate_override_value); // Enable baudrate override
            snprintf(message, sizeof(message), "ESP32 baudrate changed to %u (%u)\r\n", webusb_esp32_baudrate_override_value, result);
            webusb_debug(message);
        } else {
            snprintf(message, sizeof(message), "ESP32 baudrate change to %u ignored\r\n", webusb_esp32_baudrate_override_value);
            webusb_debug(message);
        }
        webusb_esp32_baudrate_override_requested = false;
    }
    if (webusb_fpga_baudrate_override_requested) {
        if (get_webusb_connected(2)) {
            uint32_t result = webusb_set_uart_baudrate(1, true, webusb_fpga_baudrate_override_value); // Enable baudrate override
            snprintf(message, sizeof(message), "FPGA baudrate changed to %u (%u)\r\n", webusb_fpga_baudrate_override_value, result);
            webusb_debug(message);
        } else {
            snprintf(message, sizeof(message), "FPGA baudrate change to %u ignored\r\n", webusb_fpga_baudrate_override_value);
            webusb_debug(message);
        }
        webusb_fpga_baudrate_override_requested = false;
    }
    
    if (webusb_mode_change_requested) {
        if (get_webusb_connected(1)) {
            i2c_set_webusb_mode(webusb_mode_change_target); // Set ESP32 WebUSB mode
            snprintf(message, sizeof(message), "ESP32 mode changed to %02x\r\n", webusb_mode_change_target);
            webusb_debug(message);
        } else {
            snprintf(message, sizeof(message), "ESP32 mode change to %02x ignored\r\n", webusb_mode_change_target);
            webusb_debug(message);
        }
        webusb_mode_change_requested = false;
    }
    
    for (uint8_t idx = 0; idx < CFG_TUD_VENDOR; idx++) {
        // On status change
        if (webusb_status_changed[idx]) {
            if (!get_webusb_connected(idx)) {
                if (idx == 1) { // ESP32
                    uint32_t result = webusb_set_uart_baudrate(0, false, 0); // Restore control over ESP32 baudrate to CDC
                    i2c_set_webusb_mode(0x00); // Disable ESP32 WebUSB mode
                    snprintf(message, sizeof(message), "WebUSB disconnected from ESP32 channel (%u)\r\n", result);
                    webusb_debug(message);
                } else if (idx == 2) { // FPGA
                    uint32_t result = webusb_set_uart_baudrate(1, false, 0); // Restore control over FPGA baudrate to CDC
                    snprintf(message, sizeof(message), "WebUSB disconnected from FPGA channel (%u)\r\n", result);
                    webusb_debug(message);
                }
            }
        }

        // Data transfer
        int available = tud_vendor_n_available(idx);
        if (available > 0) {
            uint8_t buffer[64];
            uint32_t length = tud_vendor_n_read(idx, buffer, sizeof(buffer));
            if (idx == WEBUSB_IDX_CONTROL) {
                //tud_vendor_n_write(idx, buffer, length); // Loopback
                /*length = snprintf(buffer, sizeof(buffer), "Status: %04X (idx %u)\n", webusb_status[idx], idx);
                tud_vendor_n_write(idx, buffer, length);*/
            } else if (WEBUSB_IDX_ESP32) {
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
                if (request->wIndex == ITF_NUM_VENDOR_2) {
                    webusb_status[2] = request->wValue;
                    webusb_status_changed[2] = true;
                }
                return tud_control_status(rhport, request);
            }
            if (request->bRequest == 0x23) { // Reset
                // Channel 0 and 2 are ignored, only the ESP32 can be reset
                if (request->wIndex == ITF_NUM_VENDOR_1) {
                    webusb_esp32_reset_requested = true;
                    webusb_esp32_reset_mode = request->wValue;
                    return tud_control_status(rhport, request);
                }
            }
            if (request->bRequest == 0x24) { // Set baudrate
                // Channel 0 is ignored, only the ESP32 and FPGA channels are connected to an actual UART
                if (request->wIndex == ITF_NUM_VENDOR_1) {
                    webusb_esp32_baudrate_override_requested = true;
                    webusb_esp32_baudrate_override_value = (request->wValue) * 100;
                }
                if (request->wIndex == ITF_NUM_VENDOR_2) {
                    webusb_fpga_baudrate_override_requested = true;
                    webusb_fpga_baudrate_override_value = (request->wValue) * 100;
                }
                return tud_control_status(rhport, request);
            }
            if (request->bRequest == 0x25) { // Set mode
                // Channel 0 and 2 are ignored, only the ESP32 can be switched to different modes
                if (request->wIndex == ITF_NUM_VENDOR_1) {
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
