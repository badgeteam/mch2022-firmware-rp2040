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
#include "hardware.h"
#include "lcd.h"
#include "hardware/adc.h"

int main(void) {
    board_init();
    tusb_init();
    setup_uart();
    
    gpio_init(USB_DET_PIN);
    gpio_init(BATT_CHRG_PIN);
    gpio_set_dir(USB_DET_PIN, false);
    gpio_set_dir(BATT_CHRG_PIN, false);
    
    adc_init();
    adc_gpio_init(ANALOG_TEMP_PIN);
    adc_gpio_init(ANALOG_VBAT_PIN);
    adc_gpio_init(ANALOG_VUSB_PIN);
    
    esp32_reset(true); // Reset ESP32
    lcd_init();
    esp32_reset(false); // Start ESP32

    while (1) {
        tud_task(); // tinyusb device task
        cdc_task();
        
        if (board_button_read()) {
            printf("Reset to USB bootloader...\r\n");
            reset_usb_boot(0, 0);
        }
    }

    return 0;
}

// Invoked when device is mounted
void tud_mount_cb(void) {
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
}

// Invoked when usb bus is suspended
void tud_suspend_cb(bool remote_wakeup_en) {
  (void) remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
}
