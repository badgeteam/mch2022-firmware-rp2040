#include <stdio.h>
#include <stdlib.h>

#include "hardware.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h"

static uint pio_offset = 0;

void ws2812_setup() { pio_offset = pio_add_program(WS2812_PIO, &ws2812_program); }

void ws2812_enable(bool rgbw) { ws2812_program_init(WS2812_PIO, 0, pio_offset, SAO_IO0_PIN, 800000, rgbw); }

void ws2812_disable() { pio_sm_set_enabled(WS2812_PIO, 0, false); }

void ws2812_put(uint32_t data) { pio_sm_put_blocking(WS2812_PIO, 0, data); }
