#pragma once

#include <stdint.h>
#include <i2c_fifo.h>
#include <i2c_slave.h>
#include <pico/stdlib.h>

void setup_i2c_peripheral(i2c_inst_t *i2c, uint8_t sda_pin, uint8_t scl_pin, uint8_t address, uint32_t baudrate, i2c_slave_handler_t handler);

void setup_i2c_registers();

void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event);

void i2c_bl_set_state(uint8_t state);
bool i2c_bl_get_ctrl();

enum {
    I2C_REGISTER_FW_VER,
    I2C_REGISTER_BL_VER,
    I2C_REGISTER_BL_STATE,
    I2C_REGISTER_BL_CTRL
};
