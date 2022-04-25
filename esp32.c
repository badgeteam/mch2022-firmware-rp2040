#include "esp32.h"

static mutex_t esp32_uart_mtx;

void esp32_uart_init(){
  uart_init(UART_ESP32, UART_ESP32_BAUDRATE);
  gpio_set_function(UART_ESP32_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_ESP32_RX_PIN, GPIO_FUNC_UART);

  irq_set_exclusive_handler(UART0_IRQ, esp32_uart_rx);
  irq_set_enabled(UART0_IRQ, true);
  uart_set_irq_enables(UART_ESP32, true, false);

  uart_set_hw_flow(UART_ESP32, false, false);
  uart_set_format(UART_ESP32, 8, 1, 0);
  uart_set_fifo_enabled(UART_ESP32, true); // test if we need a fifo

  mutex_init(&esp32_uart_mtx);
}

void esp32_uart_rx() {
  #define RX_BUFFSIZE 64
  char buf[RX_BUFFSIZE];
  if (uart_is_readable(UART_ESP32)) {
    uint8_t ch = uart_getc(UART_ESP32);

    mutex_enter_blocking(&esp32_uart_mtx);
    uint16_t pos = 0;
    while (uart_is_readable(UART_ESP32) && pos < RX_BUFFSIZE) {
      buf[pos++] = uart_getc(UART_ESP32);
    }
    tud_cdc_tx_cb(USB_CDC_ESP32, buf, pos);
    mutex_exit(&esp32_uart_mtx);
  }
}

// if pin 0 and 1 are bridged, the strinc received is "MEW mow fotpfo:", this is obvs wrong
void esp32_uart_tx(char *buf, size_t len){
  if(uart_is_writable(UART_ESP32)){
    uart_puts(UART_ESP32, "\n\rMEOW meow pfot pfot: \n");
  }
}
