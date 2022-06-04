/*
 * Copyright (c) 2022 Nicolai Electronics
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "pico/time.h"
#include "pico/stdio/driver.h"
#include "pico/binary_info.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "uart_task.h"
#include "webusb_task.h"
#include "hardware.h"
#include "i2c_peripheral.h"

static bool esp32_reset_active = false;
static uint8_t esp32_reset_state = 0;
static uint8_t esp32_reset_app_state = 0;
static absolute_time_t esp32_reset_timeout = 0;

static bool fpga_loopback_active = false;

cdc_line_coding_t current_line_coding[2];
cdc_line_coding_t requested_line_coding[2];

bool ready = false;

bool config_override_active[2];

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
    irq_set_exclusive_handler(UART0_IRQ, on_esp32_uart_rx);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(UART_ESP32, true, false);
    uart_set_hw_flow(UART_ESP32, false, false);
    uart_set_format(UART_ESP32, 8, 1, 0);
    
    current_line_coding[0].bit_rate = 115200;
    current_line_coding[0].data_bits = 8;
    current_line_coding[0].parity = 0;
    current_line_coding[0].stop_bits = 1;
    memcpy(&requested_line_coding[0], &current_line_coding[0], sizeof(cdc_line_coding_t));
    config_override_active[0] = false;

    uart_init(UART_FPGA, 115200);
    gpio_set_function(UART_FPGA_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_FPGA_RX_PIN, GPIO_FUNC_UART);
    irq_set_exclusive_handler(UART1_IRQ, on_fpga_uart_rx);
    irq_set_enabled(UART1_IRQ, true);
    uart_set_irq_enables(UART_FPGA, true, false);
    uart_set_hw_flow(UART_FPGA, false, false);
    uart_set_format(UART_FPGA, 8, 1, 0);

    current_line_coding[1].bit_rate = 115200;
    current_line_coding[1].data_bits = 8;
    current_line_coding[1].parity = 0;
    current_line_coding[1].stop_bits = 1;
    memcpy(&requested_line_coding[1], &current_line_coding[1], sizeof(cdc_line_coding_t));
    config_override_active[1] = false;
    
    ready = true;
}

void on_esp32_uart_rx() {
    if (!ready) return;
    uint8_t buffer[64];
    uint32_t length = 0;
    while (uart_is_readable(UART_ESP32)) {
        buffer[length] = uart_getc(UART_ESP32);
        length++;
        if (length >= sizeof(buffer)) {
            cdc_send(0, buffer, length);
            if (get_webusb_connected(WEBUSB_IDX_ESP32)) tud_vendor_n_write(WEBUSB_IDX_ESP32, buffer, length);
            length = 0;
        }
    }
    if (length > 0) {
        cdc_send(0, buffer, length);
        if (get_webusb_connected(WEBUSB_IDX_ESP32)) tud_vendor_n_write(WEBUSB_IDX_ESP32, buffer, length);
        length = 0;
    }
}

void on_fpga_uart_rx() {
    if (!ready) return;
    uint8_t buffer[64];
    uint32_t length = 0;
    if (fpga_loopback_active) {
        while (uart_is_readable(UART_FPGA)) {
            uart_tx_wait_blocking(UART_FPGA);
            uart_putc_raw(UART_FPGA, uart_getc(UART_FPGA) ^ 0xa5);
	}
        return;
    }
    while (uart_is_readable(UART_FPGA)) {
        buffer[length] = uart_getc(UART_FPGA);
        length++;
        if (length >= sizeof(buffer)) {
            cdc_send(1, buffer, length);
            if (get_webusb_connected(WEBUSB_IDX_FPGA)) tud_vendor_n_write(WEBUSB_IDX_FPGA, buffer, length);
            length = 0;
        }
    }
    if (length > 0) {
        cdc_send(1, buffer, length);
        if (get_webusb_connected(WEBUSB_IDX_FPGA)) tud_vendor_n_write(WEBUSB_IDX_FPGA, buffer, length);
        length = 0;
    }
}

void cdc_send(uint8_t itf, uint8_t* buf, uint32_t count) {
    if (!ready) return;
    tud_cdc_n_write(itf, buf, count);
    tud_cdc_n_write_flush(itf);
}

uint calc_data_bits(uint requested) {
    uint data_bits;
    switch (requested) {
        case 5: data_bits = 5; break;
        case 6: data_bits = 6; break;
        case 7: data_bits = 7; break;
        case 8: data_bits = 8; break;
        case 16: default: data_bits = 8; break; // Not supported
    }
    return data_bits;
}

uint calc_stop_bits(uint requested) {
    uint stop_bits;
    switch (requested) {
        case 0: stop_bits = 1; break; // 1 stop bit
        case 2: stop_bits = 2; break; // 2 stop bits
        case 1: default: stop_bits = 1; break; // 1.5 stop bits (not supported)
    }
    return stop_bits;
}

uint calc_parity(uint requested) {
    uart_parity_t parity;
    switch (requested) {
        case 0: parity = UART_PARITY_NONE; break;
        case 1: parity = UART_PARITY_ODD; break;
        case 2: parity = UART_PARITY_EVEN; break;
        case 3: case 4: default: parity = UART_PARITY_NONE; break; //3: mark, 4: space (not supported)
    }
    return parity;
}

void apply_line_coding(uint8_t itf) {  
    uart_inst_t* uart;
    uint8_t index = 0;
    if (itf == USB_CDC_ESP32) {
        uart = UART_ESP32;
        index = 0;
    } else  if (itf == USB_CDC_FPGA) {
        uart = UART_FPGA;
        index = 1;
    } else {
        return;
    }
    
    bool changed = false;
    
    if ((!config_override_active[index]) && (!((index == 1) && (fpga_loopback_active)))) { // If not in override mode and (in case of FPGA UART) not in fpga loopback mode
        if (current_line_coding[itf].bit_rate != requested_line_coding[itf].bit_rate) {
            int actual_baudrate = uart_set_baudrate(uart, requested_line_coding[itf].bit_rate);
        }
        
        if ((current_line_coding[itf].data_bits != requested_line_coding[itf].data_bits) ||
            (current_line_coding[itf].stop_bits != requested_line_coding[itf].stop_bits) ||
            (current_line_coding[itf].parity != requested_line_coding[itf].parity)) {
            uart_set_format(uart, calc_data_bits(
                requested_line_coding[itf].data_bits),
                calc_stop_bits(requested_line_coding[itf].stop_bits),
                calc_parity(requested_line_coding[itf].parity)
            );
        }
    }

    memcpy(&current_line_coding[itf], &requested_line_coding[itf], sizeof(cdc_line_coding_t));
}

void cdc_task(void) {
    uint8_t buffer[64];
    apply_line_coding(0);
    apply_line_coding(1);
    
    if (tud_cdc_n_available(USB_CDC_ESP32)) {
        uint32_t length = tud_cdc_n_read(USB_CDC_ESP32, buffer, sizeof(buffer));
        /*for (uint32_t position = 0; position < length; position++) {
            uart_tx_wait_blocking(UART_ESP32);
            uart_putc_raw(UART_ESP32, buffer[position]);
        }*/
        uart_write_blocking(UART_ESP32, buffer, length);
    }

    if (tud_cdc_n_available(USB_CDC_FPGA)) {
        uint32_t length = tud_cdc_n_read(USB_CDC_FPGA, buffer, sizeof(buffer));
        /*for (uint32_t position = 0; position < length; position++) {
            uart_tx_wait_blocking(UART_FPGA);
            uart_putc_raw(UART_FPGA, buffer[position]);
        }*/
        uart_write_blocking(UART_FPGA, buffer, length);
    }

    absolute_time_t now = get_absolute_time();
    if((esp32_reset_state || esp32_reset_active) && now > esp32_reset_timeout) {
        if (esp32_reset_active) {
            gpio_put(ESP32_EN_PIN, true);
            esp32_reset_active = false;
            esp32_reset_state = 0xFF;
            esp32_reset_app_state = 0xFF;
            esp32_reset_timeout = delayed_by_ms(get_absolute_time(), 50);
        } else {
            esp32_reset_state = 0x00;
            esp32_reset_app_state = 0x00;
            gpio_set_dir(ESP32_BL_PIN, false);
            gpio_put(ESP32_EN_PIN, true);
        }
    }
}

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* new_line_coding) {
    if (itf == USB_CDC_ESP32) {
        memcpy(&requested_line_coding[0], new_line_coding, sizeof(cdc_line_coding_t));
    }
    if (itf == USB_CDC_FPGA) {
        memcpy(&requested_line_coding[1], new_line_coding, sizeof(cdc_line_coding_t));
    }
}

void esp32_reset(bool download_mode) {
    if (download_mode) {
        gpio_set_dir(ESP32_BL_PIN, true); // Output, RP2040 pulls low
    } else {
        gpio_set_dir(ESP32_BL_PIN, false); // Input, ext. pull-up high
    }
    gpio_put(ESP32_EN_PIN, false);
    esp32_reset_active = true;
    esp32_reset_timeout = delayed_by_ms(get_absolute_time(), 25);
    gpio_put(FPGA_RESET, false); // Always disable the FPGA if the ESP32 gets reset
}


void webusb_set_uart_baudrate(uint8_t index, bool enable_override, uint32_t baudrate) {
    uart_inst_t* uarts[] = {UART_ESP32, UART_FPGA};
    uint8_t usb_cdcs[] = {USB_CDC_ESP32, USB_CDC_FPGA};

    if (index >= sizeof(uarts)) return; // Ignore invalid index
    
    if ((index == 1) && (fpga_loopback_active)) return; // Ignore FPGA baudrate change in FPGA loopback mode

    if (enable_override) {
        config_override_active[index] = true; // Ignore requests from the CDC interface
        uart_set_baudrate(uarts[index], baudrate);
        uart_set_format(uarts[index], 8, 1, UART_PARITY_NONE);
    } else {
        config_override_active[index] = false; // Accept requests from the CDC interface
        apply_line_coding(usb_cdcs[index]); // Restore configuration
    }
}

void fpga_loopback(bool enable) {
    if (enable) {
        fpga_loopback_active = true; // Enable loopback

        // Switch to 1 MBaud 8N1
        uart_set_baudrate(UART_FPGA, 1000000);
        uart_set_format(UART_FPGA, 8, 1, UART_PARITY_NONE);
    } else {
        fpga_loopback_active = false; // Disable loopback
        apply_line_coding(USB_CDC_FPGA); // Restore configuration
    }
}

bool prev_dtr = false;
bool prev_rts = false;
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    if(itf == USB_CDC_ESP32) {
        bool dtr2 = dtr || prev_dtr;
        bool rts2 = rts || prev_rts;
        
        if ((esp32_reset_state == 0) && ( dtr2) && ( rts2)) esp32_reset_state = 1;
        if ((esp32_reset_state == 1) && ( dtr2) && (!rts2)) esp32_reset_state = 2;
        if ((esp32_reset_state == 2) && (!dtr2) && ( rts2)) esp32_reset_state = 3;
        if ((esp32_reset_state == 3) && ( dtr2) && ( rts2)) {
            esp32_reset_state = 4;
            esp32_reset(true);
        }
        
        if ((esp32_reset_app_state == 0) && ((!dtr2) && rts2)) esp32_reset_app_state = 1;
        if ((esp32_reset_app_state == 1) && ((!dtr2) && (!rts2))) {
            esp32_reset_app_state = 2;
            esp32_reset(false);
        }
        
        prev_dtr = dtr;
        prev_rts = rts;
        
        if (!esp32_reset_active) {
            esp32_reset_timeout = delayed_by_ms(get_absolute_time(), 1000);
        }
    }
}
