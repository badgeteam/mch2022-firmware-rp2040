#pragma once

#include <stdbool.h>
#include <stdint.h>

void lcd_init();
void lcd_mode(bool parallel_mode);
void lcd_backlight(uint8_t value);
