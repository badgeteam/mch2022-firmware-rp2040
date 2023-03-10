/*
 * Copyright (c) 2022 Nicolai Electronics
 *
 * SPDX-License-Identifier: MIT
 */

#include "uart_task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/board.h"
#include "hardware.h"
#include "hardware/uart.h"
#include "i2c_peripheral.h"
#include "pico/binary_info.h"
#include "pico/stdio/driver.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/types.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "webusb_task.h"

bool    esp32_reset_active    = false;
uint8_t esp32_reset_state     = 0;
uint8_t esp32_reset_app_state = 0;
#ifdef NDEBUG
absolute_time_t esp32_reset_timeout = 0;
#else
absolute_time_t esp32_reset_timeout = {._private_us_since_boot = 0};
#endif
bool fpga_loopback_active = false;
bool esp32_msc_active = false;

cdc_line_coding_t current_line_coding[2];
cdc_line_coding_t cdc_requested_line_coding[2];
cdc_line_coding_t webusb_requested_line_coding[2];
cdc_line_coding_t fpga_loopback_requested_line_coding;
cdc_line_coding_t msc_requested_line_coding;

void setup_uart() {
    gpio_init(ESP32_BL_PIN);
    gpio_set_dir(ESP32_BL_PIN, false);
    gpio_put(ESP32_BL_PIN, false);

    gpio_init(ESP32_EN_PIN);
    gpio_set_dir(ESP32_EN_PIN, true);
    gpio_put(ESP32_EN_PIN, false);

    uart_init(UART_ESP32, 115200);
    gpio_set_function(UART_ESP32_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_ESP32_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_ESP32, 8, 1, 0);

    current_line_coding[0].bit_rate  = 115200;
    current_line_coding[0].data_bits = 8;
    current_line_coding[0].parity    = 0;
    current_line_coding[0].stop_bits = 1;
    memcpy(&cdc_requested_line_coding[0], &current_line_coding[0], sizeof(cdc_line_coding_t));
    memcpy(&webusb_requested_line_coding[0], &current_line_coding[0], sizeof(cdc_line_coding_t));

    uart_init(UART_FPGA, 115200);
    gpio_set_function(UART_FPGA_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_FPGA_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_FPGA, 8, 1, 0);

    current_line_coding[1].bit_rate  = 115200;
    current_line_coding[1].data_bits = 8;
    current_line_coding[1].parity    = 0;
    current_line_coding[1].stop_bits = 1;
    memcpy(&cdc_requested_line_coding[1], &current_line_coding[1], sizeof(cdc_line_coding_t));
    memcpy(&webusb_requested_line_coding[1], &current_line_coding[1], sizeof(cdc_line_coding_t));

    fpga_loopback_requested_line_coding.bit_rate  = 1000000;
    fpga_loopback_requested_line_coding.data_bits = 8;
    fpga_loopback_requested_line_coding.parity    = 0;
    fpga_loopback_requested_line_coding.stop_bits = 1;
    
    msc_requested_line_coding.bit_rate  = 2000000;
    msc_requested_line_coding.data_bits = 8;
    msc_requested_line_coding.parity    = 0;
    msc_requested_line_coding.stop_bits = 1;
}

void cdc_send(uint8_t itf, uint8_t* buf, uint32_t count) {
    tud_cdc_n_write(itf, buf, count);
    tud_cdc_n_write_flush(itf);
}

uint calc_data_bits(uint requested) {
    uint data_bits;
    switch (requested) {
        case 5:
            data_bits = 5;
            break;
        case 6:
            data_bits = 6;
            break;
        case 7:
            data_bits = 7;
            break;
        case 8:
            data_bits = 8;
            break;
        case 16:
        default:
            data_bits = 8;
            break;  // Not supported
    }
    return data_bits;
}

uint calc_stop_bits(uint requested) {
    uint stop_bits;
    switch (requested) {
        case 0:
            stop_bits = 1;
            break;  // 1 stop bit
        case 2:
            stop_bits = 2;
            break;  // 2 stop bits
        case 1:
        default:
            stop_bits = 1;
            break;  // 1.5 stop bits (not supported)
    }
    return stop_bits;
}

uint calc_parity(uint requested) {
    uart_parity_t parity;
    switch (requested) {
        case 0:
            parity = UART_PARITY_NONE;
            break;
        case 1:
            parity = UART_PARITY_ODD;
            break;
        case 2:
            parity = UART_PARITY_EVEN;
            break;
        case 3:
        case 4:
        default:
            parity = UART_PARITY_NONE;
            break;  // 3: mark, 4: space (not supported)
    }
    return parity;
}

void apply_line_coding(uint8_t itf) {
    uart_inst_t* uart;
    uint8_t      webusb_index;
    if (itf == USB_CDC_ESP32) {
        uart         = UART_ESP32;
        webusb_index = WEBUSB_IDX_ESP32;
    } else if (itf == USB_CDC_FPGA) {
        uart         = UART_FPGA;
        webusb_index = WEBUSB_IDX_FPGA;
    } else {
        return;
    }

    cdc_line_coding_t* target_line_coding;

    if ((itf == USB_CDC_FPGA) && (fpga_loopback_active)) {  // If FPGA loopback is active the loopback settings take priority
        target_line_coding = &fpga_loopback_requested_line_coding;
    } else if ((itf == USB_CDC_ESP32) && (esp32_msc_active)) { // If ESP32 mass storage is active the mass storage settings take priority
        target_line_coding = &msc_requested_line_coding;
    } else if (get_webusb_connected(webusb_index)) {  // If WebUSB is connected WebUSB controls the settings
        target_line_coding = &webusb_requested_line_coding[itf];
    } else {  // And if WebUSB is not connected the settings are controlled by the CDC interface
        target_line_coding = &cdc_requested_line_coding[itf];
    }

    bool changed = false;

    if (current_line_coding[itf].bit_rate != target_line_coding->bit_rate) {
        uart_set_baudrate(uart, target_line_coding->bit_rate);
        changed = true;
    }

    if ((current_line_coding[itf].data_bits != target_line_coding->data_bits) || (current_line_coding[itf].stop_bits != target_line_coding->stop_bits) ||
        (current_line_coding[itf].parity != target_line_coding->parity)) {
        uart_set_format(uart, calc_data_bits(target_line_coding->data_bits), calc_stop_bits(target_line_coding->stop_bits),
                        calc_parity(target_line_coding->parity));
        changed = true;
    }

    if (changed) {
        memcpy(&current_line_coding[itf], target_line_coding, sizeof(cdc_line_coding_t));
    }
}

void uart_task(void) {
    uint8_t  buffer[256];
    uint32_t length = 0;

    apply_line_coding(USB_CDC_ESP32);
    apply_line_coding(USB_CDC_FPGA);

    if (!esp32_msc_active) {
        length = 0;
        while (uart_is_readable(UART_ESP32) && (length < sizeof(buffer))) {
            buffer[length] = uart_getc(UART_ESP32);
            length++;
        }

        if (length > 0) {
            if (get_webusb_connected(WEBUSB_IDX_ESP32)) {
                tud_vendor_n_write(WEBUSB_IDX_ESP32, buffer, length);
            } else {
                cdc_send(0, buffer, length);
            }
        }
    }

    length = 0;
    while (uart_is_readable(UART_FPGA) && (length < sizeof(buffer))) {
        buffer[length] = uart_getc(UART_FPGA);
        length++;
    }

    if (length > 0) {
        if (fpga_loopback_active) {
            for (uint32_t position = 0; position < length; position++) {
                buffer[position] = buffer[position] ^ 0xa5;
            }
            uart_write_blocking(UART_FPGA, buffer, length);
        } else {
            if (get_webusb_connected(WEBUSB_IDX_FPGA)) {
                tud_vendor_n_write(WEBUSB_IDX_FPGA, buffer, length);
            } else {
                cdc_send(1, buffer, length);
            }
        }
    }

    if (!esp32_msc_active) {
        if (tud_cdc_n_available(USB_CDC_ESP32) && !get_webusb_connected(WEBUSB_IDX_ESP32)) {
            length = tud_cdc_n_read(USB_CDC_ESP32, buffer, sizeof(buffer));
            uart_write_blocking(UART_ESP32, buffer, length);
        }
    }

    if (tud_cdc_n_available(USB_CDC_FPGA) && !fpga_loopback_active && !get_webusb_connected(WEBUSB_IDX_FPGA)) {
        length = tud_cdc_n_read(USB_CDC_FPGA, buffer, sizeof(buffer));
        uart_write_blocking(UART_FPGA, buffer, length);
    }

    absolute_time_t now = get_absolute_time();
#ifdef NDEBUG
    if ((esp32_reset_active) && now > esp32_reset_timeout) {
#else
    if ((esp32_reset_active) && now._private_us_since_boot > esp32_reset_timeout._private_us_since_boot) {
#endif
        if (esp32_reset_active) {
            gpio_put(ESP32_EN_PIN, true);
            esp32_reset_active  = false;
            esp32_reset_timeout = delayed_by_ms(get_absolute_time(), 1);
        } else {
            gpio_put(ESP32_EN_PIN, true);
            gpio_set_dir(ESP32_BL_PIN, false);
        }
    }
}

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* new_line_coding) {
    if (itf == USB_CDC_ESP32) {
        memcpy(&cdc_requested_line_coding[0], new_line_coding, sizeof(cdc_line_coding_t));
    }
    if (itf == USB_CDC_FPGA) {
        memcpy(&cdc_requested_line_coding[1], new_line_coding, sizeof(cdc_line_coding_t));
    }
}

void esp32_reset(bool download_mode) {
    if (esp32_reset_active) return;
    esp32_reset_active = true;
    gpio_put(FPGA_RESET, false);    // Always disable the FPGA if the ESP32 gets reset
    gpio_put(ESP32_EN_PIN, false);  // Disable the ESP32
    if (download_mode) {
        gpio_set_dir(ESP32_BL_PIN, true);  // Output, RP2040 pulls low
    } else {
        gpio_set_dir(ESP32_BL_PIN, false);  // Input, ext. pull-up high
    }
    esp32_reset_timeout = delayed_by_ms(get_absolute_time(), 1);

    esp32_reset_state     = 0;
    esp32_reset_app_state = 0;
}

void webusb_set_uart_baudrate(uint8_t index, uint32_t baudrate) {
    uint8_t usb_cdcs[] = {USB_CDC_ESP32, USB_CDC_FPGA};
    if (index >= sizeof(usb_cdcs)) return;  // Ignore invalid index
    webusb_requested_line_coding[usb_cdcs[index]].bit_rate = baudrate;
}

void fpga_loopback(bool enable) {
    fpga_loopback_active = enable;
    if (enable) {
        apply_line_coding(USB_CDC_FPGA);
    }
}

void uart_esp32_msc(bool enable) {
    esp32_msc_active = enable;
    apply_line_coding(USB_CDC_ESP32);
    char logbuf[64];
    sprintf(logbuf, "MSC uart state change to %s\r\n", enable ? "enabled" : "disabled");
    cdc_send(0, logbuf, strlen(logbuf));
}

bool prev_dtr = false;
bool prev_rts = false;
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    if ((itf == USB_CDC_ESP32) && (!get_webusb_connected(WEBUSB_IDX_ESP32))) {
        bool dtr2 = dtr || prev_dtr;
        bool rts2 = rts || prev_rts;

        if (!esp32_reset_active) {
            if ((esp32_reset_state == 0) && (dtr2) && (rts2)) esp32_reset_state = 1;
            if ((esp32_reset_state == 1) && (dtr2) && (!rts2)) esp32_reset_state = 2;
            if ((esp32_reset_state == 2) && (!dtr2) && (rts2)) esp32_reset_state = 3;
            if ((esp32_reset_state == 3) && (dtr2) && (rts2)) {
                if (i2c_get_reset_allowed()) {
                    esp32_reset(true);
                } else {
                    i2c_set_reset_attempted(true);
                    esp32_reset(false);
                }
            }

            if ((esp32_reset_app_state == 0) && ((!dtr2) && rts2)) esp32_reset_app_state = 1;
            if ((esp32_reset_app_state == 1) && ((!dtr2) && (!rts2))) {
                esp32_reset(false);
            }
        }

        prev_dtr = dtr;
        prev_rts = rts;
    }
}
