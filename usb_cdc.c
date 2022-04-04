#include "usb_cdc.h"

// todo
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  if (dtr){
  }else{
  }
}

// interrupt based usb cdc
void tud_cdc_rx_cb(uint8_t itf){
  if (itf == USB_CDC_ESP32){
    char buf[64];
    uint32_t count = tud_cdc_read(buf, sizeof(buf));
    tud_cdc_write(buf, count);
    tud_cdc_write_flush();
  } else if (itf == USB_CDC_FPGA){

  } else if (itf == USB_CDC_STDIO){

  } else {
  }
}
