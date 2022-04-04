#include "usb_cdc.h"

void cdc_task(void)
{
  if ( tud_cdc_n_available(USB_CDC_ESP32) )
  {
    char buf[64];
    uint32_t count = tud_cdc_read(buf, sizeof(buf));
    tud_cdc_write(buf, count);
    tud_cdc_write_flush();
  }
}

// todo
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  if (dtr){
  }else{
  }
}

// invoced when data is received, use this instead of cdc task?
void tud_cdc_rx_cb(uint8_t itf){
}
