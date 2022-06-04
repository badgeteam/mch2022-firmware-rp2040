#pragma once

// UART0: connected to ESP32
#define UART_ESP32 uart0
#define UART_ESP32_TX_PIN 0
#define UART_ESP32_RX_PIN 1
#define UART_ESP32_BAUDRATE 115200

// I2C0: system I2C bus
#define I2C_SYSTEM i2c1
#define I2C_SYSTEM_SDA_PIN 2
#define I2C_SYSTEM_SCL_PIN 3

// Buttons
#define BUTTON_MENU   4
#define BUTTON_HOME   5
#define BUTTON_ACCEPT 6
#define BUTTON_JOY_A  7
#define BUTTON_JOY_B  8
#define BUTTON_JOY_C  9
#define BUTTON_JOY_D  10
#define BUTTON_JOY_E  11
#define BUTTON_START  22
#define BUTTON_BACK   26

// FPGA
#define FPGA_CDONE 20
#define FPGA_RESET 21

// UART1: connected to ICE40 FPGA
#define UART_FPGA uart1
#define UART_FPGA_TX_PIN 24
#define UART_FPGA_RX_PIN 25
#define UART_FPGA_BAUDRATE 9600

// LCD control lines
#define LCD_BACKLIGHT_PIN 15

// ESP32 control lines
#define ESP32_BL_PIN 12 // Output, high for normal boot, low for download boot, also serves as IRQ
#define ESP32_EN_PIN 13 // Output, high to enable ESP32, low to reset ESP32
#define ESP32_INT_PIN 14

// Analog inputs
#define ANALOG_VUSB_PIN 28
#define ANALOG_VUSB_ADC 2
#define ANALOG_VBAT_PIN 29
#define ANALOG_VBAT_ADC 3
#define ANALOG_TEMP_ADC 4

// System state GPIOs
#define BATT_CHRG_PIN 23

// Other GPIOs
#define PROTO_0_PIN 16
#define PROTO_1_PIN 17

// SAO GPIOs
#define SAO_IO0_PIN 18
#define SAO_IO1_PIN 19

// IR LED
#define IR_LED 27

// USB virtual device numbers
#define USB_CDC_ESP32 0
#define USB_CDC_FPGA 1
#define USB_CDC_STDIO 2
