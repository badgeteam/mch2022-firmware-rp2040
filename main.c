/**
 * Copyright (c) 2022 Nicolai Electronics
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "bsp/board.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "uart_task.h"
#include "webusb_task.h"
#include "hardware.h"
#include "lcd.h"
#include "hardware/adc.h"
#include "i2c_peripheral.h"

int main(void) {
    board_init();
    tusb_init();
    setup_uart();
    
    gpio_init(BATT_CHRG_PIN);
    gpio_set_dir(BATT_CHRG_PIN, false);
    
    adc_init();
    adc_gpio_init(ANALOG_VBAT_PIN);
    adc_gpio_init(ANALOG_VUSB_PIN);
    
    gpio_init(IR_LED);
    gpio_set_dir(IR_LED, true);
    gpio_put(IR_LED, false);
    
    lcd_init();
    
    setup_i2c_registers();
    setup_i2c_peripheral(I2C_SYSTEM, I2C_SYSTEM_SDA_PIN, I2C_SYSTEM_SCL_PIN, 0x17, 400000, i2c_slave_handler);
    
    esp32_reset(true); // Reset ESP32
    esp32_reset(false); // Start ESP32

    while (1) {
        tud_task();
        cdc_task();
        i2c_task();
        webusb_task();
    }

    return 0;
}

// Invoked when device is mounted
void tud_mount_cb(void) {
    i2c_usb_set_mounted(true);
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
    i2c_usb_set_mounted(false);
}

// Invoked when usb bus is suspended
void tud_suspend_cb(bool remote_wakeup_en) {
    i2c_usb_set_suspended(true, remote_wakeup_en);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
    i2c_usb_set_suspended(false, false);
}
