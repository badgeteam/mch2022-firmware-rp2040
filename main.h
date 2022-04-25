#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <bsp/board.h>
#include <hardware/irq.h>
#include <hardware/structs/sio.h>
#include <hardware/uart.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <pico/mutex.h>
#include <pico/stdio/driver.h>

#include "hardware.h"
#include "tusb.h"
#include "usb_cdc.h"
#include "esp32.h"
