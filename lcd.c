/**
 * Copyright (c) 2022 Nicolai Electronics
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "hardware/pwm.h"
#include "hardware.h"

#include "lcd.h"

uint32_t pwm_set_freq_duty(uint slice_num, uint chan, uint32_t freq, int duty) {
    uint32_t clock = 125000000;
    uint32_t divider16 = clock / freq / 4096 +  (clock % (freq * 4096) != 0);
    if (divider16 / 16 == 0) {
        divider16 = 16;
    }
    uint32_t wrap = clock * 16 / divider16 / freq - 1;
    pwm_set_clkdiv_int_frac(slice_num, divider16 / 16, divider16 & 0xF);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, chan, wrap * duty / 255);
    return wrap;
}

void lcd_init() {
    gpio_init(LCD_BACKLIGHT_PIN);
    gpio_init(LCD_MODE_PIN);
    gpio_init(LCD_RESET_PIN);
    gpio_set_dir(LCD_BACKLIGHT_PIN, true);
    gpio_set_dir(LCD_MODE_PIN, true);
    gpio_set_dir(LCD_RESET_PIN, true);
    gpio_put(LCD_BACKLIGHT_PIN, false);
    lcd_mode(false);
    
    gpio_set_function(LCD_BACKLIGHT_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(LCD_BACKLIGHT_PIN);
    uint chan = pwm_gpio_to_channel(LCD_BACKLIGHT_PIN);
    pwm_set_freq_duty(slice_num, chan, 200, 0);
    pwm_set_enabled(slice_num, true);
    lcd_backlight(255);
}

void lcd_mode(bool parallel_mode) {
    gpio_put(LCD_MODE_PIN, parallel_mode);
    gpio_put(LCD_RESET_PIN, false);
    sleep_ms(10);
    gpio_put(LCD_RESET_PIN, true);
}

void lcd_backlight(uint8_t value) {
    uint slice_num = pwm_gpio_to_slice_num(LCD_BACKLIGHT_PIN);
    uint chan = pwm_gpio_to_channel(LCD_BACKLIGHT_PIN);
    pwm_set_freq_duty(slice_num, chan, 200, value);
}
