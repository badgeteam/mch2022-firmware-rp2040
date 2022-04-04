#pragma once

#include "tusb.h"
#include "bsp/board.h"
#include "pico/unique_id.h"

enum
{
  VENDOR_REQUEST_WEBUSB = 1,
  VENDOR_REQUEST_MICROSOFT = 2
};

extern uint8_t const desc_ms_os_20[];
