#pragma once

#include <stdint.h>
#include <i2c_fifo.h>
#include <i2c_slave.h>
#include <pico/stdlib.h>

void setup_i2c_peripheral(i2c_inst_t *i2c, uint8_t sda_pin, uint8_t scl_pin, uint8_t address, uint32_t baudrate, i2c_slave_handler_t handler);

void setup_i2c_registers();

void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event);
void i2c_task();

void i2c_register_write(uint8_t reg, uint8_t value);

void i2c_usb_set_mounted(bool mounted);
void i2c_usb_set_suspended(bool suspended, bool remote_wakeup_en);

enum {
    I2C_REGISTER_FW_VER,
    I2C_REGISTER_GPIO_DIR,
    I2C_REGISTER_GPIO_IN,
    I2C_REGISTER_GPIO_OUT,
    I2C_REGISTER_LCD_BACKLIGHT,
    I2C_REGISTER_FPGA,
    I2C_REGISTER_INPUT1,
    I2C_REGISTER_INPUT2,
    I2C_REGISTER_INTERRUPT1,
    I2C_REGISTER_INTERRUPT2,
    I2C_REGISTER_ADC_TRIGGER,
    I2C_REGISTER_ADC_VALUE_VUSB1,
    I2C_REGISTER_ADC_VALUE_VUSB2,
    I2C_REGISTER_ADC_VALUE_VBAT1,
    I2C_REGISTER_ADC_VALUE_VBAT2,
    I2C_REGISTER_USB,
    I2C_REGISTER_SCRATCH0, // Used by the ESP32 to store boot parameters, can also be read and written to from WebUSB
    I2C_REGISTER_SCRATCH1,
    I2C_REGISTER_SCRATCH2,
    I2C_REGISTER_SCRATCH3,
    I2C_REGISTER_SCRATCH4,
    I2C_REGISTER_SCRATCH5,
    I2C_REGISTER_SCRATCH6,
    I2C_REGISTER_SCRATCH7,
    I2C_REGISTER_SCRATCH8,
    I2C_REGISTER_SCRATCH9,
    I2C_REGISTER_SCRATCH10,
    I2C_REGISTER_SCRATCH11,
    I2C_REGISTER_SCRATCH12,
    I2C_REGISTER_SCRATCH13,
    I2C_REGISTER_SCRATCH14,
    I2C_REGISTER_SCRATCH15,
    I2C_REGISTER_SCRATCH16,
    I2C_REGISTER_SCRATCH17,
    I2C_REGISTER_SCRATCH18,
    I2C_REGISTER_SCRATCH19,
    I2C_REGISTER_SCRATCH20,
    I2C_REGISTER_SCRATCH21,
    I2C_REGISTER_SCRATCH22,
    I2C_REGISTER_SCRATCH23,
    I2C_REGISTER_SCRATCH24,
    I2C_REGISTER_SCRATCH25,
    I2C_REGISTER_SCRATCH26,
    I2C_REGISTER_SCRATCH27,
    I2C_REGISTER_SCRATCH28,
    I2C_REGISTER_SCRATCH29,
    I2C_REGISTER_SCRATCH30,
    I2C_REGISTER_SCRATCH31,
    I2C_REGISTER_SCRATCH32,
    I2C_REGISTER_SCRATCH33,
    I2C_REGISTER_SCRATCH34,
    I2C_REGISTER_SCRATCH35,
    I2C_REGISTER_SCRATCH36,
    I2C_REGISTER_SCRATCH37,
    I2C_REGISTER_SCRATCH38,
    I2C_REGISTER_SCRATCH39,
    I2C_REGISTER_SCRATCH40,
    I2C_REGISTER_SCRATCH41,
    I2C_REGISTER_SCRATCH42,
    I2C_REGISTER_SCRATCH43,
    I2C_REGISTER_SCRATCH44,
    I2C_REGISTER_SCRATCH45,
    I2C_REGISTER_SCRATCH46,
    I2C_REGISTER_SCRATCH47,
    I2C_REGISTER_SCRATCH48,
    I2C_REGISTER_SCRATCH49,
    I2C_REGISTER_SCRATCH50,
    I2C_REGISTER_SCRATCH51,
    I2C_REGISTER_SCRATCH52,
    I2C_REGISTER_SCRATCH53,
    I2C_REGISTER_SCRATCH54,
    I2C_REGISTER_SCRATCH55,
    I2C_REGISTER_SCRATCH56,
    I2C_REGISTER_SCRATCH57,
    I2C_REGISTER_SCRATCH58,
    I2C_REGISTER_SCRATCH59,
    I2C_REGISTER_SCRATCH60,
    I2C_REGISTER_SCRATCH61,
    I2C_REGISTER_SCRATCH62,
    I2C_REGISTER_SCRATCH63
};
