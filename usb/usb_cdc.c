#include "usb_cdc.h"

// todo
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  if (dtr){
  }else{
  }
}

// interrupt based usb cdc
void tud_cdc_rx_cb(uint8_t itf){ // itf is always 0, why?
  if (itf == USB_CDC_ESP32){
    char buf[64];
    uint32_t len = tud_cdc_read(buf, sizeof(buf));
    esp32_uart_tx(buf, len);
  } else if (itf == USB_CDC_FPGA){ // this does not work, why?
  } else if (itf == USB_CDC_STDIO){
  } else {
  }
}

void tud_cdc_tx_cb(uint8_t itf, char *buf, size_t len){
  if (itf == USB_CDC_ESP32){
    tud_cdc_write(buf, len);
    tud_cdc_write_flush();
  } else if (itf == USB_CDC_FPGA){

  } else if (itf == USB_CDC_STDIO){

  } else {
  }
}
