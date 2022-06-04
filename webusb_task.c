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

uint16_t webusb_status[CFG_TUD_VENDOR] = {0x0000};

void webusb_task() {
    uint8_t buffer[64];
    for (uint8_t idx = 0; idx < CFG_TUD_VENDOR; idx++) {
        int available = tud_vendor_n_available(idx);
        if (available > 0) {
            uint32_t length = tud_vendor_n_read(idx, buffer, sizeof(buffer));
            if (idx == WEBUSB_IDX_CONTROL) {
                tud_vendor_n_write(idx, buffer, length); // Loopback
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
    return webusb_status[idx] & 1;
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
            default: break;
            }
        break;

        case TUSB_REQ_TYPE_CLASS:
            if (request->bRequest == 0x22) {
                if (request->wIndex == ITF_NUM_VENDOR_0) {
                    webusb_status[0] = request->wValue;
                }
                if (request->wIndex == ITF_NUM_VENDOR_1) {
                    webusb_status[1] = request->wValue;
                }
                if (request->wIndex == ITF_NUM_VENDOR_2) {
                    webusb_status[2] = request->wValue;
                }
                // response with status OK
                return tud_control_status(rhport, request);
            }
            if (request->bRequest == 0x23) {
                //request->wValue;
                // response with status OK
                return tud_control_status(rhport, request);
            }
        break;

        default: break;
    }

    // stall unknown request
    return false;
}

bool tud_vendor_control_complete_cb(uint8_t rhport, tusb_control_request_t const * request) {
  (void) rhport;
  (void) request;
  return true;
}
