#pragma once

void ws2812_setup();
void ws2812_enable(bool rgbw);
void ws2812_disable();
void ws2812_put(uint32_t data);
