#pragma once

#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#endif

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif


// UART0: connected to ESP32
#define UART_ESP32 uart0
#define UART_ESP32_TX_PIN 0
#define UART_ESP32_RX_PIN 1
#define UART_ESP32_BAUDRATE 115200

// UART1: connected to ICE40 FPGA
#define UART_FPGA uart1
#define UART_FPGA_TX_PIN 24
#define UART_FPGA_RX_PIN 25
#define UART_FPGA_BAUDRATE 9600

// I2C0: user I2C bus (SAO)
#define I2C_EXT i2c0
#define I2C_EXT_SDA_PIN 8
#define I2C_EXT_SCL_PIN 9

// I2C1: system I2C bus (internal)
#define I2C_SYSTEM i2c1
#define I2C_SYSTEM_SDA_PIN 2
#define I2C_SYSTEM_SCL_PIN 3

// SPI0: SPI bus
#define SPI_SCK 18
#define SPI_TX 19
#define SPI_RX 20
#define SPI_CS 21

// Addressable LEDs
#define LED_DATA_PIN 11
#define LED_PWR_PIN 22

// LCD control lines
#define LCD_BACKLIGHT_PIN 15
#define LCD_RESET_PIN 16
#define LCD_MODE_PIN 17

// ESP32 control lines
#define ESP32_BL_PIN 12 // Output, high for normal boot, low for download boot, also serves as IRQ
#define ESP32_EN_PIN 13 // Output, high to enable ESP32, low to reset ESP32
#define ESP32_WK_PIN 14 // Normally set to input, set to output low to wake up the ESP32

// Analog inputs
#define ANALOG_TEMP_PIN 27
#define ANALOG_TEMP_ADC 1
#define ANALOG_VBAT_PIN 28
#define ANALOG_VBAT_ADC 2
#define ANALOG_VUSB_PIN 29
#define ANALOG_VUSB_ADC 0

// System state GPIOs
#define USB_DET_PIN 26
#define BATT_CHRG_PIN 23

// SAO GPIOs
#define SAO_IO0_PIN 6
#define SAO_IO1_PIN 7

// Other GPIOs
#define PROTO_0_PIN 4
#define PROTO_1_PIN 5
#define PROTO_2_PIN 10

// USB virtual device numbers
#define USB_CDC_ESP32 0
#define USB_CDC_FPGA 1
#define USB_CDC_STDIO 2
